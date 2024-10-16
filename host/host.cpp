
#include <stdint.h>

#include <iostream>
#include <cstring>
#include "FPGAHandler.h"

//// XRT includes
//#include "xrt/xrt_bo.h"
//#include <experimental/xrt_xclbin.h>
//#include "xrt/xrt_device.h"
//#include "xrt/xrt_kernel.h"
//#include <experimental/xrt_queue.h>

#include "StreamInterface.h"


int main(int argc, char** argv) {
	srand(time(NULL));


	StreamInterface* ifc = StreamInterface::getInstance();
	uint64_t cnt = 0;
	uint64_t sendcnt = 0;
	for ( int i = 0; i < (32*1024); i++ ) {
		uint32_t nd = rand()%256;
		while ( ifc->send(&nd, sizeof(uint32_t)) < 0 ) {
			//printf( "Trying receive\n" );
			uint32_t dd = 0;
			while ( ifc->recv(&dd, sizeof(uint32_t)) > 0 ) {
				//printf( "Received\n" );
				cnt++;
			}
		}
		sendcnt ++;
	}

	printf( "SendCnt: %lu Cnt: %lu\n", sendcnt, cnt );
	printf( "total receive: %lu\n", ifc->m_totalRecvBytes );

	return 0;
}
