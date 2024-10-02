
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


	FPGAHandler* fgpainterface = FPGAHandler::getInstance();
	double dst[64] = {0};
	for (int j = 0; j < 64; j++)
	{
		dst[j] = j;
	}
	double pos[4] = {0};
	
	double result[64];

	uint64_t cnt = 0;
	uint64_t sendcnt = 0;

	for ( int i = 0; i < (120); i++ ) {
		int key = fgpainterface->send(dst, pos);
		// printf("key#:%i ", key);
		sendcnt++;
		//fgpainterface->receive(result, key);
		cnt++;

	}
	// printf("\n");
	// StreamInterface* ifc = StreamInterface::getInstance();
	// StreamInterface* ifc = fgpainterface->getStreamInterface();

	// ChristoffelInput christoffelInput{{1},{1}};
	//ChristoffelInput christoffelInput[120];
    // christoffelInput.dst = dst;
    // christoffelInput.pos = pos;
    // for (int i = 0; i < 100; i++)
	// {
	// 	while ( ifc->send(&christoffelInput, sizeof(ChristoffelInput)*20) < 0 ) {
	// 		double result[64];
	// 		while ( ifc->recv(&result, sizeof(double[64])) > 0 ) {
	// 			break;
	// 	}	
	// }
	// }
	

	//uint64_t cnt = 0;
	//uint64_t sendcnt = 0;
	// for ( int i = 0; i < (32*1024); i++ ) {
	// 	uint32_t nd = rand()%256;
	// 	while ( ifc->send(&nd, sizeof(uint32_t)) < 0 ) {
	// 		uint32_t dd = 0;
	// 		while ( ifc->recv(&dd, sizeof(uint32_t)) > 0 ) {
	// 			cnt++;
	// 		}
	// 	}
	// 	sendcnt ++;
	// }

	printf( "SendCnt: %lu Cnt: %lu\n", sendcnt, cnt );
	printf( "total receive: %lu\n", fgpainterface->getReceiveByteCount() );
	printf("end line------\n");
    return 0;
}


/* int main(int argc, char** argv) {
 *     srand(time(NULL));
 *     StreamInterface* ifc = StreamInterface::getInstance();
 *
 *     uint64_t cnt = 0;
 *     uint64_t sendcnt = 0;
 *     for ( int i = 0; i < (32*1024); i++ ) {
 *         uint32_t nd = i;
 *         while ( ifc->send(&nd, sizeof(uint32_t)) < 0 ) {
 *             uint32_t dd = 0;
 *             while ( ifc->recv(&dd, sizeof(uint32_t)) > 0 ) {
 *                 cnt++;
 *             }
 *         }
 *         sendcnt ++;
 *     }
 *
 *     printf( "SendCnt: %lu Cnt: %lu\n", sendcnt, cnt );
 *     printf( "total receive: %lu\n", ifc->m_totalRecvBytes );
 *
 *     return 0;
 * } */
