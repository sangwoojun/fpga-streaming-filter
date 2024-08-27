#/*
#Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
#SPDX-License-Identifier: X11
#*/

#include <stdint.h>

#include <iostream>
#include <cstring>
#include "StreamInterface.h"


int main(int argc, char** argv) {
	srand(time(NULL));
	StreamInterface* ifc = StreamInterface::getInstance();

	uint64_t cnt = 0;
	uint64_t sendcnt = 0;
    uint32_t nd = 0;
    uint32_t dd = 0;

    for (uint32_t i = 0; i < 1024 * 1 + 1; i++) {
        nd = (uint32_t)i;
        while ( ifc->send(&nd, sizeof(uint32_t)) < 0 ) {}
    }

    printf("Entered Rec!\n");

    for (uint32_t i = 0; i < 1024 * 1; i++) {
        if (ifc->recv(&dd, sizeof(uint32_t)) == -1)
            continue;
        else
            std::cout<<"Rec " << i << ": " << dd << std::endl;
    }

	printf( "SendCnt: %lu Cnt: %lu\n", sendcnt, cnt );
	printf( "total receive: %lu\n", ifc->m_totalRecvBytes );

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
