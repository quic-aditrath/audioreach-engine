/*============================================================================
 * Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
============================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
//#include "fir_macro.h"
#include "fir_lib.h"
#include "audio_basic_op_ext.h"
#include "audio_basic_op.h"
#include <stdlib.h>
#include <stdio.h>

#include "../inc/FIR_ASM_macro.h"
//void fir_lib_process_c16xd16_rnd(fir_filter_t *, int16 *, int16 *, int32, int16);
/*===========================================================================*/
/* FUNCTION : fir_reset                                                      */
/*                                                                           */
/* DESCRIPTION: Clean FIR filter buffer and reset memory index.              */
/*                                                                           */
/* INPUTS: filter-> FIR filter struct                                        */
/*         data_width: bit with of data (16 or 32)                           */
/* OUTPUTS: filter history and index set to zeros.                           */
/*                                                                           */
/* IMPLEMENTATION NOTES:                                                     */
/*===========================================================================*/
void fir_lib_reset(fir_filter_t *filter, int32 data_width)
{
    int32 i;
    int16 *ptr16 = NULL;
    int32 *ptr32 = NULL;

    // reset filter memory index
    filter->mem_idx = 0;

    // clean filter histories
    switch (data_width) {
    case 16:
       ptr16 = (int16 *)filter->history;
       for (i = 0; i < filter->taps; ++i) {
          *ptr16++ = 0;
       }
       break;

    case 32:
       ptr32 = (int32 *)filter->history;
       for (i = 0; i < filter->taps; ++i)
       {
          *ptr32++ = 0;
       }
       break;

    default:
       return;
    }

}  //------------------------ end of function fir_reset ---------------------

#ifdef QDSP6_ASM_OPT_FIR_FILTER
void fir_lib_process_c16xd16_rnd(fir_filter_t *filter, int16 *dest, int16 *src, int32 samples, int16 qx)
{
	uint32 nZeroPadInput;
	int32 nOffsetIntoInputBuf = 0;
	int32 nInputProcSize = 0;
	int32 fir_len = 0;
	int16 *memPtr = filter->history;
	int16 *outPtr = filter->output;
	int16 *coeffPtr = (int16 *)filter->coeffs;
	int32 shift = s16_sub_s16_s16(qx,16);

	//  To make filter taps multiple of 4
	nZeroPadInput = (filter->taps) & 0x3;
	nZeroPadInput = nZeroPadInput ? (4 - nZeroPadInput) : 0;
	fir_len = filter->taps + nZeroPadInput;

	while (samples)
	{
	   nInputProcSize = (samples > MAX_PROCESS_FRAME_SIZE) ? MAX_PROCESS_FRAME_SIZE : samples;
	   //Size of input block is a multiple of 4
	   nZeroPadInput = (nInputProcSize) & 0x3;
	   if (nZeroPadInput) {
	       nZeroPadInput = 4 - nZeroPadInput;
	    }
	    samples -= nInputProcSize;

	    // [<-----STATE(=TAPS-1)----->][<------INPUT BLOCK------>]
	    // Copy Input buffer into structure to feed into fir_c16xd16_asm
	    memsmove((memPtr + (filter->taps - 1)), nInputProcSize * sizeof(int16),
	                 (src + nOffsetIntoInputBuf),
	                 nInputProcSize * sizeof(int16));


	    fir_c16xd16_asm(memPtr,
	        				  coeffPtr,
	                          fir_len,
	                          nInputProcSize + nZeroPadInput,
	                          (int32) shift,
	                          outPtr
	                          );

	    // Copy to destination Buffer
	    memsmove(dest + nOffsetIntoInputBuf,
	                 (nInputProcSize * sizeof(int16)), outPtr,
	                 (nInputProcSize * sizeof(int16)));


	    nOffsetIntoInputBuf += nInputProcSize;


	    // Copy Filter States
	    memsmove(memPtr, ((filter->taps - 1) * sizeof(int16)),
	                 (memPtr + nInputProcSize),
	                 ((filter->taps - 1) * sizeof(int16)));
	    }
}

void fir_lib_process_c16xd32_rnd(fir_filter_t *filter, int32 *dest, int32 *src, int32 samples, int16 qx)

{
    uint32 fir_len;
    int32 nOffsetIntoInputBuf = 0;
    int32 nInputProcSize = 0;
    int32 *memPtr = (int32*)filter->history;
    int32 *outPtr = filter->output;
    int16 *coeffPtr = (int16 *) filter->coeffs;

    fir_len = filter->taps;
    // To make filter taps multiple of 2
    if(filter->taps & 0x1)
    {
    	fir_len++;
    }

    while (samples)
    {
        nInputProcSize = (samples >  MAX_PROCESS_FRAME_SIZE) ?  MAX_PROCESS_FRAME_SIZE : samples;

        samples -= nInputProcSize;
        // [<-----STATE(=TAPS-1)----->][<------INPUT BLOCK------>]
        // Copy Input buffer into structure to feed into fir_c16xd32_asm
        memsmove((memPtr + filter->taps - 1),
                 nInputProcSize * sizeof(int32),
                 (src + nOffsetIntoInputBuf),
                 nInputProcSize * sizeof(int32));
        //==================================================================
        fir_c16xd32_asm(memPtr, coeffPtr, nInputProcSize, fir_len, outPtr, qx);

        // Copy to destination Buffer
        memsmove(dest + nOffsetIntoInputBuf, nInputProcSize * sizeof(int32),
                 outPtr, nInputProcSize * sizeof(int32) );

        nOffsetIntoInputBuf += nInputProcSize;

        // Copy Filter States
        memsmove(memPtr, (filter->taps - 1) * sizeof(int32),
                 (memPtr + nInputProcSize), (filter->taps - 1) * sizeof(int32) );
    }
}

void fir_lib_process_c32xd32_rnd(fir_filter_t *filter, int32 *dest, int32 *src, int32 samples, int16 qx)
{
	   uint32 fir_len;
	   int32 nOffsetIntoInputBuf = 0;
	   int32 nInputProcSize = 0;
	   int32 *memPtr = (int32*)filter->history;
	   int32 *outPtr =(int32 *) filter->output;
	   int32 *coeffPtr = (int32 *) filter->coeffs;

	   fir_len = filter->taps;
	   //To make filter taps multiple of 2
	   if(filter->taps & 0x1)
	   {
	      fir_len++;
	   }

	   while (samples)
	   {
	           nInputProcSize = (samples >  MAX_PROCESS_FRAME_SIZE) ?  MAX_PROCESS_FRAME_SIZE : samples;

	           samples -= nInputProcSize;
	           // [<-----STATE(=TAPS-1)----->][<------INPUT BLOCK------>]
	           // Copy Input buffer into structure to feed into fir_c32xd32_asm
	           memsmove((memPtr + filter->taps - 1),
	                    nInputProcSize * sizeof(int32),
	                    (src + nOffsetIntoInputBuf),
	                    nInputProcSize * sizeof(int32));
	           //==================================================================
	           fir_c32xd32_asm(memPtr, coeffPtr, nInputProcSize, fir_len, outPtr, qx);

	           // Copy to destination Buffer
	           memsmove(dest + nOffsetIntoInputBuf, nInputProcSize * sizeof(int32),
	                    outPtr, nInputProcSize * sizeof(int32) );

	           nOffsetIntoInputBuf += nInputProcSize;

	           // Copy Filter States
	           memsmove(memPtr, (filter->taps - 1) * sizeof(int32),
	                    (memPtr + nInputProcSize), (filter->taps - 1) * sizeof(int32) );
	       }
}

// 32 coeff, 16 data
void fir_lib_process_c32xd16_rnd(fir_filter_t *filter, int16 *dest, int16 *src, int32 samples, int16 qx)
{
		int32 nOffsetIntoInputBuf = 0;
		int32 nInputProcSize = 0;
		int32 fir_len = 0;
		int16 *memPtr = filter->history;
		int16 *outPtr = (int16 *)filter->output;
		int32 *coeffPtr = (int32 *)filter->coeffs;

		fir_len = filter->taps;
		//To make filter taps multiple of 2
		if(filter->taps & 0x1)
		{
		   fir_len++;
		}

		while (samples)
		{
		        nInputProcSize = (samples >  MAX_PROCESS_FRAME_SIZE) ?  MAX_PROCESS_FRAME_SIZE : samples;

		        samples -= nInputProcSize;
		        // [<-----STATE(=TAPS-1)----->][<------INPUT BLOCK------>]
		        // Copy Input buffer into structure to feed into fir_c32xd16_asm
		        memsmove((memPtr + filter->taps - 1),
		                 nInputProcSize * sizeof(int16),
		                 (src + nOffsetIntoInputBuf),
		                 nInputProcSize * sizeof(int16));
		        //==================================================================
		        fir_c32xd16_asm(memPtr, coeffPtr, nInputProcSize, fir_len, outPtr, qx);

		        // Copy to destination Buffer
		        memsmove(dest + nOffsetIntoInputBuf, nInputProcSize * sizeof(int16),
		                 outPtr, nInputProcSize * sizeof(int16) );

		        nOffsetIntoInputBuf += nInputProcSize;

		        // Copy Filter States
		        memsmove(memPtr, (filter->taps - 1) * sizeof(int16),
		                 (memPtr + nInputProcSize), (filter->taps - 1) * sizeof(int16) );
		    }
}

#else


void fir_lib_process_c16xd16_rnd(fir_filter_t *filter, int16 *dest, int16 *src, int32 samples, int16 qx)
{

	int32   i, j;
	int16   shift;
	int32   idx = filter->mem_idx;
	int32   taps = filter->taps;
	int16   *filter_mem = (int16 *)filter->history;
	int16   *coeff_ptr = NULL;
	int64   y64;
	//int16   value;
	// determine the up-shift amount according to Q factor
	shift = s16_sub_s16_s16(15, qx);

	for (i = 0; i < samples; ++i) {

		// update "current" sample with the new input
		filter_mem[idx] = *src++;

		// reset coeff ptr & accum
		coeff_ptr = (int16 *)filter->coeffs;
		y64 = 0;

		for (j = 0; j < taps - 1; ++j) {
			// convolution, y = sum (c[k] * x[n-k]) , k = 0, ..., taps-1
			//value=filter_mem[idx];
			y64 = s64_mac_s64_s16_s16_s1(y64, *coeff_ptr++, filter_mem[idx]);

			// wrap around circular buf
			idx = s32_modwrap_s32_u32(idx + 1, taps);
		} // end of j loop

		// process last sample, then current idx points to the next input position
		y64 = s64_mac_s64_s16_s16_s1(y64, *coeff_ptr++, filter_mem[idx]);


		// shift and output sample
		*dest++ = s16_extract_s64_h_sat(s64_add_s64_s32(s64_shl_s64(y64, shift), 0x8000));

	} // end of i loop

	// update index in filter struct
	filter->mem_idx = idx;
}



void fir_lib_process_c16xd32_rnd(fir_filter_t *filter, int32 *dest, int32 *src, int32 samples, int16 qx)
{
	   int32   i, j;
	   int32   idx = filter->mem_idx;
	   int32   taps = filter->taps;
	   int32   *filter_mem = (int32 *)filter->history;
	   int16   *coeff_ptr = NULL;
	   int64   y64;
	   int16   neg_qx = -qx;
	   int32   tmpShiftL32=0;

	   if(qx > 0)
			tmpShiftL32 = ((int32)1) << (qx-1);

	   for (i = 0; i < samples; ++i) {
	      // update "current" sample with new input
	      filter_mem[idx] = *src++;

	      // reset coeff ptr & accum
	      coeff_ptr = (int16 *)filter->coeffs;
	      y64 = 0;

	      for (j = 0; j < taps-1; ++j) {
	         // convolution, y = sum (c[k] * x[n-k]) , k = 0, ..., taps-1
	         y64 = s64_mac_s32_s32(y64, filter_mem[idx], (int32)*coeff_ptr++);
	         idx = s32_modwrap_s32_u32(idx+1, taps);
	      } // end of j loop

	      // process last sample, then current idx points to the next input position
	      y64 = s64_mac_s32_s32(y64, filter_mem[idx], (int32)(*coeff_ptr++));

	      // round, shift and output sample
	      *dest++ = s32_saturate_s64(s64_shl_s64(s64_add_s64_s32(y64, tmpShiftL32), neg_qx));

	   } // end of i loop

	   // update index in filter struct
	   filter->mem_idx = idx;
}

void fir_lib_process_c32xd32_rnd(fir_filter_t *filter, int32 *dest, int32 *src, int32 samples, int16 qx)
{

	   int32   i, j;
	   int32   idx = filter->mem_idx;
	   int32   taps = filter->taps;
	   int32   *filter_mem = (int32 *)filter->history;
	   int32   *coeff_ptr = NULL;
	   int64   y64;
	   int16   neg_qx = -qx;
	   int64   tmpShiftL64 =0;

	   if(qx > 0)
			tmpShiftL64 = ((int64)1) << (qx-1);

	   for (i = 0; i < samples; ++i) {

	      // update "current" sample with new input
	      filter_mem[idx] = *src++;

	      // reset coeff ptr & accum
	      coeff_ptr = (int32 *)filter->coeffs;
	      y64 = 0;

	      for (j = 0; j < taps-1; ++j) {
	         // convolution, y = sum (c[k] * x[n-k]) , k = 0, ..., taps-1
	         y64 = s64_mac_s32_s32(y64, filter_mem[idx], *coeff_ptr++);
	         idx = s32_modwrap_s32_u32(idx+1, taps);
	      } // end of j loop

	      // process last sample, then current idx points to the next input position
	      y64 = s64_mac_s32_s32(y64, filter_mem[idx], *coeff_ptr++);

	      // shift and output sample
	      //*dest++ = s32_saturate_s64(s64_shl_s64(y64, neg_qx));
		  *dest++ = s32_saturate_s64(s64_shl_s64(s64_add_s64_s64(y64, tmpShiftL64), neg_qx));

	   } // end of i loop

	   // update index in filter struct
	   filter->mem_idx = idx;
}




/* 32 coeff, 16 data */
void fir_lib_process_c32xd16_rnd(fir_filter_t *filter, int16 *dest, int16 *src, int32 samples, int16 qx)
{
	   int32   i, j;
	   int32   idx = filter->mem_idx;
	   int32   taps = filter->taps;
	   int16   *filter_mem = (int16 *)filter->history;
	   int32   *coeff_ptr = NULL;
	   int64   y64;
	   int16   neg_qx = -qx;
	   int64   tmpShiftL64=0;

	   if(qx > 0)
			tmpShiftL64 = ((int64)1) << (qx-1);

	   for (i = 0; i < samples; ++i) {

	      // update "current" sample with the new input
	      filter_mem[idx] = *src++;

	      // reset coeff ptr & accum
	      coeff_ptr = (int32 *)filter->coeffs;
	      y64 = 0;

	      for (j = 0; j < taps-1; ++j) {
	         // convolution, y = sum (c[k] * x[n-k]) , k = 0, ..., taps-1
	         y64 = s64_mac_s32_s32(y64, (int32)filter_mem[idx], *coeff_ptr++);
	         idx = s32_modwrap_s32_u32(idx+1, taps);
	      }

	      // process last sample, then current idx points to the next input position
	      y64 = s64_mac_s32_s32(y64, (int32)filter_mem[idx], *coeff_ptr++);

	      // shift and output sample
	      //*dest++ = s16_saturate_s32(s32_saturate_s64(s64_shl_s64(y64, neg_qx)));
		  *dest++ = s16_saturate_s32(s32_saturate_s64(s64_shl_s64(s64_add_s64_s64(y64, tmpShiftL64), neg_qx)));

	   } // end of i loop

	   // update index in filter struct
	   filter->mem_idx = idx;
}

#endif
/* 32 coeff, 32 data */


