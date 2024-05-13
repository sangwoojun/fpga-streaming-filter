/*
#Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
#SPDX-License-Identifier: X11
#*/

extern "C" {
	void kernel(
		const unsigned int *in,	// Input stream
		unsigned int *out,		// Output stream
		int in_off,				// Byte offset for input
		int out_off,			// Byte offset for output
		int size				// Size in integer. Set to output size on completion
		)
	{
#pragma HLS INTERFACE m_axi port=in depth=8192 max_read_burst_length=32  bundle=aximm1
#pragma HLS INTERFACE m_axi port=out depth=8192 max_read_burst_length=32  bundle=aximm1

		int insize = size;
		int outidx = 0;
		#pragma HLS pipeline II=1 off rewind style=stp
		for(int i = 0; i < insize/sizeof(int); ++i) {
			int inidx = in_off + i;
			if ( in[inidx] < 128 ) {
				out[outidx+out_off+(64/sizeof(unsigned int))] = in[inidx];
				outidx++;
			}
		}

		out[out_off] = outidx*sizeof(int);
	}
}

