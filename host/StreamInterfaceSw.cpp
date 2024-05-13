#include "StreamInterface.h"

#include <signal.h>

extern "C"
void kernel(
	const unsigned int *in,	// Input stream
	unsigned int *out,		// Output stream
	int in_off,				// Byte offset for input
	int out_off,			// Byte offset for output
	int size				// Size in integer. Set to output size on completion
	);

StreamInterface*
StreamInterface::m_pInstance = NULL;

StreamInterface::StreamInterface() {
	for ( int i = 0; i < XRT_QUEUE_CNT; i++ ) {
		m_inBufHost[i] = malloc(BUFFER_BYTES);
		m_outBufHost[i] = malloc(BUFFER_BYTES);
		m_outQueueState[i] = BUF_INIT;
	}

	m_curInQueue = 0;
	m_curOutQueue = 0;

	m_curInByteOff = 0;
	m_curOutByteOff = 64; // First 64 bytes are header
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

bool 
StreamInterface::flush() {
	BufferState bufstate = m_outQueueState[m_curInQueue];
	if ( !(bufstate == BUF_INIT || bufstate == BUF_USEDONE) ) return false;

	kernel((unsigned int*)m_inBufHost[m_curInQueue], (unsigned int*)m_outBufHost[m_curOutQueue], 0, 0, m_curInByteOff);
	m_outQueueState[m_curInQueue] = BUF_USEREADY;
	m_curOutByteOff = 64; // First 64 bytes are header

	m_curInByteOff = 0;

	return true;
}

int32_t
StreamInterface::recv(void* ptr, int32_t bytes) {
	BufferState bufstate = m_outQueueState[m_curOutQueue];
	if ( !(bufstate == BUF_USEREADY) ) return -1;

	int32_t bufbytes = ((int32_t*)m_outBufHost[m_curOutQueue])[0];
		
	if (  m_curOutByteOff + bytes > bufbytes || m_curOutByteOff + bytes > BUFFER_BYTES ) {
		m_outQueueState[m_curOutQueue] = BUF_USEDONE;
		return -1;
	}

	memcpy(ptr, (uint8_t*)(m_outBufHost[m_curOutQueue]) + m_curOutByteOff, bytes);
	m_curOutByteOff += bytes;
	m_totalRecvBytes += bytes;

	return bytes;
}

bool 
StreamInterface::inflight() {
	if ( m_outQueueState[m_curInQueue] == BUF_USEREADY ) return true;
	return false;
}
