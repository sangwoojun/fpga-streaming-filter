#ifndef __STREAM_INTERFACE_H__
#define __STREAM_INTERFACE_H__

#ifdef HWTARGET

// XRT includes
#include "xrt/xrt_bo.h"
#include <experimental/xrt_xclbin.h>
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include <experimental/xrt_queue.h>

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// 1<23 == 8 MB
// 8MB is kind of random, hopefully large enough
#define BUFFER_BYTES_LG 12
#define BUFFER_BYTES (1<<BUFFER_BYTES_LG)
#define XRT_QUEUE_CNT 4

typedef enum {
	BUF_INIT,
	BUF_INFLIGHT,
	BUF_USEREADY,
	BUF_USEDONE
} BufferState;

class StreamInterface {
private:
	static StreamInterface* m_pInstance;
	StreamInterface();


#ifdef HWTARGET
	// "in" to the kernel
	xrt::bo m_inBufDev[XRT_QUEUE_CNT];

	// "out" of the kernel
	xrt::bo m_outBufDev[XRT_QUEUE_CNT];

	xrt::queue::event m_writeBackEventQueue[XRT_QUEUE_CNT];
	xrt::queue m_asyncQueue[XRT_QUEUE_CNT];
	xrt::run m_kernelRun[XRT_QUEUE_CNT];
	xrt::kernel m_kernel;
#endif

	BufferState m_outQueueState[XRT_QUEUE_CNT];
	void* m_inBufHost[XRT_QUEUE_CNT];
	void* m_outBufHost[XRT_QUEUE_CNT];
	
	int m_curInQueue;
	int m_curOutQueue;//which one the user is reading out of

	int32_t m_curInByteOff = 0;
	int32_t m_curOutByteOff = 0;
public:
	uint64_t m_totalRecvBytes;
	// disable copy constructor
	StreamInterface(const StreamInterface& obj) = delete; 
	static StreamInterface* getInstance() {
		if (m_pInstance == NULL) {
			m_pInstance = new StreamInterface(); 
			return m_pInstance; 
		}
		else return m_pInstance;
	}

	int32_t send(void* ptr, int32_t bytes);
	bool flush();
	int32_t recv(void* ptr, int32_t bytes);
	bool inflight();
};

#endif
