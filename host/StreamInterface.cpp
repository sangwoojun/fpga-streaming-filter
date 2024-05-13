#include "StreamInterface.h"

#include <signal.h>


StreamInterface*
StreamInterface::m_pInstance = NULL;

StreamInterface::StreamInterface() {
	std::string binaryFile = "./kernel.xclbin";
	int device_index = 0;
	//std::cout << "Open the device" << device_index << std::endl;
	auto device = xrt::device(device_index);
	//std::cout << "Load the xclbin " << binaryFile << std::endl;
	auto uuid = device.load_xclbin("./kernel.xclbin");
	m_kernel = xrt::kernel(device, uuid, "kernel", xrt::kernel::cu_access_mode::exclusive);

	//Match kernel arguments to RTL kernel
	for ( int i = 0; i < XRT_QUEUE_CNT; i++ ) {
		m_inBufDev[i] = xrt::bo(device, BUFFER_BYTES, m_kernel.group_id(0)); 
		m_outBufDev[i] = xrt::bo(device, BUFFER_BYTES, m_kernel.group_id(1));
		m_inBufHost[i] = m_inBufDev[i].map();
		m_outBufHost[i] = m_outBufDev[i].map();
		std::fill((uint8_t*)m_inBufHost[i], (uint8_t*)m_inBufHost[i] + (BUFFER_BYTES), 0);
		std::fill((uint8_t*)m_outBufHost[i],(uint8_t*)m_outBufHost[i] + (BUFFER_BYTES), 0);
	}
	for ( int i = 0; i < XRT_QUEUE_CNT; i++ ) {
		m_inBufHost[i] = malloc(BUFFER_BYTES);
		m_outBufHost[i] = malloc(BUFFER_BYTES);
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



		int32_t* obuf = (int32_t*)(m_outBufHost[m_curInQueue]);
		int32_t bufbytes = obuf[0];
		printf( "Received buffer %d bytes %d - %d\n", bufbytes, m_curInQueue, obuf[16] ); fflush(stdout);
		//raise(SIGINT);
		m_totalRecvBytes += bufbytes;
	}
	// user needs to read out buffer first
	if ( !(bufstate == BUF_INIT || bufstate == BUF_USEDONE) ) return false;


	xrt::run& grun = m_kernelRun[m_curInQueue];

	grun = xrt::run(m_kernel);
	grun.set_arg(0, m_inBufDev[m_curInQueue]);
	grun.set_arg(1, m_outBufDev[m_curInQueue]);
	grun.set_arg(2,0);// m_curInQueue<<(BUFFER_BYTES_LG-2));
	grun.set_arg(3,0);// m_curInQueue<<(BUFFER_BYTES_LG-2));
	grun.set_arg(4, m_curInByteOff);
    
	//std::fill((uint8_t*)(m_outBufHost[m_curInQueue]),(uint8_t*)(m_outBufHost[m_curInQueue]) + BUFFER_BYTES, 0);

	int32_t* obuf = ((int32_t*)m_inBufHost[m_curInQueue]);
	obuf[0] = cnt;
	cnt++;
	
	xrt::bo bh = m_inBufDev[m_curInQueue];
	int send_bytes = m_curInByteOff;
	//int send_off = m_curInQueue<<BUFFER_BYTES_LG;
	xrt::queue::event sync_send = m_asyncQueue[m_curInQueue].enqueue([&bh, send_bytes] { 
		bh.sync(XCL_BO_SYNC_BO_TO_DEVICE, send_bytes, 0); 
	});
	xrt::queue::event run_kernel = m_asyncQueue[m_curInQueue].enqueue([&grun] {
		grun.start();
		grun.wait();
	});

	//FIXME need to sync full buffer every time?
	// maybe only read back 8KB or so first, and see what's there
	xrt::bo bd = m_outBufDev[m_curInQueue];
	//int recv_off = m_curInQueue<<BUFFER_BYTES_LG;
	this->m_writeBackEventQueue[m_curInQueue] = m_asyncQueue[m_curInQueue].enqueue([&bd] { bd.sync(XCL_BO_SYNC_BO_FROM_DEVICE); });
	printf( "Sent buffer\n" ); fflush(stdout);

	m_writeBackEventQueue[m_curInQueue].wait();
	
	m_outQueueState[m_curInQueue] = BUF_INFLIGHT;

	m_curInQueue = (m_curInQueue +1)%XRT_QUEUE_CNT;
	m_curInByteOff = 0;


	return true;
}
int32_t
StreamInterface::recv(void* ptr, int32_t bytes) {
	BufferState cstate = m_outQueueState[m_curOutQueue];
	// if INFLIGHT, try wait
	int32_t bufbytes = ((int32_t*)m_outBufHost[m_curOutQueue])[0];
	if ( cstate  == BUF_USEREADY && 
		(  m_curOutByteOff + bytes > bufbytes || m_curOutByteOff + bytes > BUFFER_BYTES )) {
		cstate = BUF_USEDONE;
		m_outQueueState[m_curOutQueue] = cstate;

		m_curOutQueue ++;
		if ( m_curOutQueue >= XRT_QUEUE_CNT ) m_curOutQueue = 0;
		m_curOutByteOff = 64; // first 512 bits are header
	}

	if ( m_outQueueState[m_curOutQueue] == BUF_INFLIGHT ) {
		this->m_writeBackEventQueue[m_curOutQueue].wait();
		m_outQueueState[m_curOutQueue] = BUF_USEREADY;

		int32_t* obuf = ((int32_t*)m_outBufHost[m_curOutQueue]);
		int32_t bufbytesr = obuf[0];
		//printf( "Received buffer %d bytes, during recv\n", bufbytesr ); fflush(stdout);
		printf( "Received buffer %d bytes,  during recv- %d\n", bufbytesr, obuf[16] ); fflush(stdout);
		m_totalRecvBytes += bufbytesr;
		m_curOutByteOff = 64; // first 512 bits are header
	}
	if ( m_outQueueState[m_curOutQueue] != BUF_USEREADY ) return -1;

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
