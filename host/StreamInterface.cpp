#include "StreamInterface.h"

#include <signal.h>


StreamInterface*
StreamInterface::m_pInstance = NULL;

StreamInterface::StreamInterface() {
    std::string binaryFile = "./kernel.xclbin";
    int device_index = 0;

    // Open the FPGA device
    std::cout << "Open the device " << device_index << std::endl;
    xrt::device device(device_index);

    // Load the .xclbin file onto the FPGA
    std::cout << "Load the xclbin " << binaryFile << std::endl;
    auto uuid = device.load_xclbin(binaryFile);

    // Create the kernel object
    m_kernel = xrt::kernel(device, uuid, "kernel:{kernel_1}");//, xrt::kernel::cu_access_mode::exclusive);

    
    std::cout << "Buffer matching " << binaryFile << std::endl;
    // Allocate input and output buffers
	for ( int i = 0; i < XRT_QUEUE_CNT; i++ ) {
		m_inBufDev[i] = xrt::bo(device, BUFFER_BYTES, m_kernel.group_id(1)); 
		m_outBufDev[i] = xrt::bo(device, BUFFER_BYTES, m_kernel.group_id(2));
		m_inBufHost[i] = m_inBufDev[i].map();
		m_outBufHost[i] = m_outBufDev[i].map();
		std::fill((uint8_t*)m_inBufHost[i], (uint8_t*)m_inBufHost[i] + (BUFFER_BYTES), 0);
		std::fill((uint8_t*)m_outBufHost[i],(uint8_t*)m_outBufHost[i] + (BUFFER_BYTES), 0);
	}

	m_curInQueue = 0;
	m_curOutQueue = 0;
	for ( int i = 0; i < XRT_QUEUE_CNT; i++ ) {
		m_outQueueState[i] = BUF_INIT;
	}

	m_totalRecvBytes = 0;

	printf( "Initialized kernel stream access\n" ); fflush(stdout);
}

int32_t
StreamInterface::send(void* ptr, int32_t bytes) {
	if ( m_curInByteOff + bytes > BUFFER_BYTES ) {
		bool ret = this->flush();
		if ( !ret ) return -1; // flush failed (probably because there was no receive buffer space)
	}

    // Copy to m_inBufHost ** in **
	memcpy(((uint8_t*)(m_inBufHost[m_curInQueue])) + m_curInByteOff, ptr, bytes); //FIXME no safety!
	m_curInByteOff += bytes;
	return bytes;
}

int cnt = 0;
bool 
StreamInterface::flush() {

	BufferState bufstate = m_outQueueState[m_curInQueue];
	if ( bufstate == BUF_INFLIGHT ) {
		this->m_writeBackEventQueue[m_curInQueue].wait();
		bufstate = BUF_USEREADY;
		m_outQueueState[m_curInQueue] = bufstate;

		m_totalRecvBytes += 4096;
	}

	// user needs to read out buffer first
	if ( !(bufstate == BUF_INIT || bufstate == BUF_USEDONE) ) {
        printf( "Need to read output buffer!\n");
        return false;
    }

	xrt::run& grun = m_kernelRun[m_curInQueue];

	grun = xrt::run(m_kernel);
    /// Need to fix
    printf("Byte off set is %d \n" , m_curInByteOff);

	xrt::bo bh = m_inBufDev[m_curInQueue];
	xrt::bo bd = m_outBufDev[m_curInQueue];
	grun.set_arg(0, m_curInByteOff);
	grun.set_arg(1, m_inBufDev[m_curInQueue]);// m_curInQueue<<(BUFFER_BYTES_LG-2));
	grun.set_arg(2, m_outBufDev[m_curInQueue]);
    
    /* std::fill((uint8_t*)(m_outBufHost[m_curInQueue]),(uint8_t*)(m_outBufHost[m_curInQueue]) + BUFFER_BYTES, 0); */

	int32_t* obuf = ((int32_t*)m_inBufHost[m_curInQueue]);
	
    printf("Input buffer is \n");
    for (int i = 0; i < 24; ++i) {
        printf("%d ", obuf[i]);
    }
	fflush(stdout);

	int send_bytes = m_curInByteOff;
	xrt::queue::event sync_send = m_asyncQueue[m_curInQueue].enqueue([&bh, send_bytes] { 
		bh.sync(XCL_BO_SYNC_BO_TO_DEVICE, send_bytes, 0); 
	});
	sync_send.wait();
	printf( "Buffer send done\n" ); fflush(stdout);
	xrt::queue::event run_kernel = m_asyncQueue[m_curInQueue].enqueue([&grun] {
		grun.start();
        //grun.wait();
	});
	run_kernel.wait();
	printf( "Kernel done\n" ); fflush(stdout);

    this->m_writeBackEventQueue[m_curInQueue] = m_asyncQueue[m_curInQueue].enqueue([&bd, send_bytes] {
            bd.sync(XCL_BO_SYNC_BO_FROM_DEVICE, send_bytes, 0); 
            });


    m_writeBackEventQueue[m_curInQueue].wait();
	//printf( "Sent buffer\n" ); fflush(stdout);

    printf("Buffer recv! \n");
	
	m_outQueueState[m_curInQueue] = BUF_INFLIGHT;

	m_curInQueue = (m_curInQueue +1)%XRT_QUEUE_CNT;
	m_curInByteOff = 0;

	return true;
}
int32_t
StreamInterface::recv(void* ptr, int32_t bytes) {
	BufferState cstate = m_outQueueState[m_curOutQueue];
    int32_t bufbytes = 1024 * 4;

	if ( cstate  == BUF_USEREADY && 
		(  m_curOutByteOff + bytes > bufbytes || m_curOutByteOff + bytes > BUFFER_BYTES )) {
		cstate = BUF_USEDONE;
		m_outQueueState[m_curOutQueue] = cstate;

		m_curOutQueue ++;
		if ( m_curOutQueue >= XRT_QUEUE_CNT ) 
            m_curOutQueue = 0;
        m_curOutByteOff = 0; // first 512 bits are header
	}

	//printf( "Recv state 1\n" );

	if ( m_outQueueState[m_curOutQueue] == BUF_INFLIGHT ) {
        this->m_writeBackEventQueue[m_curOutQueue].wait();
		m_outQueueState[m_curOutQueue] = BUF_USEREADY;
	
		//printf( "Recv state 2\n" );

		m_totalRecvBytes += bufbytes;
	}
    if ( m_outQueueState[m_curOutQueue] != BUF_USEREADY ) return -1;
		
	//printf( "Recv state 3\n" );

	memcpy(ptr, (uint8_t*)(m_outBufHost[m_curOutQueue]) + m_curOutByteOff, bytes);

	m_curOutByteOff += bytes;

	return bytes;
}
bool 
StreamInterface::inflight() {
	for ( int i = 0; i < XRT_QUEUE_CNT; i++ ) {
		BufferState bufstate = m_outQueueState[i];
		if ( !(bufstate == BUF_INIT || bufstate == BUF_USEDONE) ) return true;
	}
	return false;
}
