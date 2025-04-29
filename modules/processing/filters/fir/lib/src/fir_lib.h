#ifndef FIRLIB_H
#define FIRLIB_H
/*============================================================================
  @file fir_lib.h

  Internal header file for the FIR library.

        Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
        SPDX-License-Identifier: BSD-3-Clause-Clear

============================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "../inc/FIR_ASM_macro.h"

#include "fir_api.h"

#ifndef CAPI_STANDALONE
#include "shared_lib_api.h"
#endif

#ifdef QDSP6_ASM_OPT_FIR_FILTER
#define MAX_PROCESS_FRAME_SIZE 240

void fir_c16xd32_asm(int32 *memPtr, int16 *reverse_coeff, int nInputProcSize, int tap_length, int32 *outPtr,int16 qx);
void fir_c16xd16_asm(int16 *memPtr, int16 *reverse_coeff, int nInputProcSize, int tap_length, int16 shift, int16 *outPtr);
void fir_c32xd16_asm(int16 *memPtr, int32 *reverse_coeff, int nInputProcSize, int tap_length, int16 *outPtr,int16 qx);
void fir_c32xd32_asm(int32 *memPtr, int32 *reverse_coeff, int nInputProcSize, int tap_length, int32 *outPtr,int16 qx);

#endif
// worst case stack mem consumption in bytes;
// this number is obtained offline via stack profiling
// stack mem consumption should be no bigger than this number

/*----------------------------------------------------------------------------
* Constants Definition
* -------------------------------------------------------------------------*/
#define FIR_LIB_VER 0x02000000 //(8bits.16bits.8bits)
#define fir_filter_t fir_lib_filter_t
#define ALIGN8(o)         (((o)+7)&(~7))
#define FIR_MAX_STACK_SIZE 200
#define FIR_QFACTOR_CURRENT_GAIN 30
#define FIR_CROSS_FADING_MODE_DEFAULT 0
#define FIR_TRANSITION_PERIOD_MS_DEFAULT 20

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

// Default calibration parameters; internal use only
typedef enum FIRParamsDefault
{

	MODE_DEFAULT				= 0x0, // 1 is with FIR processing; 0 is no FIR processing(disabled)

	QFACTOR_16BIT_DEFAULT		= 13,
	QFACTOR_32BIT_DEFAULT		= 29,

	NUM_TAPS_DEFAULT			= 1,
	COEF_WIDTH_DEFAULT			= 16,

	UNITY_16BIT_DEFAULT			= 0x2000,     // Q13
	UNITY_32BIT_DEFAULT  		= 0x20000000, // Q29
	UNITY_32BIT_Q30             = 0x40000000,


} FIRParamsDefault;

typedef enum FIRProcessMode
{
	COEF16XDATA16 = 0,
	COEF32XDATA16 = 1,
	COEF16XDATA32 = 2,
	COEF32XDATA32 = 3
} FIRProcessMode;

// defined in audio_common library
typedef struct fir_filter_t
{
    int32   mem_idx;                   /* filter memory index               */
    int32   taps;                      /* filter taps                       */
    void    *history;                  /* filter memory (history)           */
    void    *coeffs;                   /* filter coefficients               */
#ifdef QDSP6_ASM_OPT_FIR_FILTER
    void    *output;                   /* output                            */
#endif
} fir_filter_t;

//FIR state params structure
typedef struct fir_state_struct_t
{

   fir_filter_t	fir_data;							// history(circular) buffer to store the past numTaps input sample values
   FIRProcessMode	firProcessMode;

} fir_state_struct_t;

//FIR panner structure
typedef struct fir_panner_struct_t
{
	uint32 max_transition_samples;        // This is the total number of transition samples
	uint32 remaining_transition_samples;  // This will be set to max value at the start of transition and reduce by 1 for every sample
	uint32 current_gain;				  // alpha increases from 0 to 1 during transition, will be 1 in steady state, set to 0 at the start of transition
	uint32 gain_step;                     // step size for alpha. calculated based on max_transition_samples.
	                                      // for current_gain, gain_step the Q-factor = 30
} fir_panner_struct_t;

// FIR lib mem structure
typedef struct fir_lib_mem_t
{
	fir_static_struct_t*  fir_static_struct_ptr;    // ptr to the static struct in mem
	int32 fir_static_struct_size;                   // size of the allocated mem pointed by the static struct
    fir_feature_mode_t* fir_feature_mode_ptr;
	int32 fir_feature_mode_size;
	fir_cross_fading_struct_t* fir_cross_fading_struct_ptr;
	int32 fir_cross_fading_struct_size;
	fir_panner_struct_t* fir_panner_struct_ptr;         // ptr to the panner struct in lib mem
	int32 fir_panner_struct_size;                       // size of the allocated mem pointed by the state struct
	fir_config_struct_t* fir_config_struct_ptr;         // Current config to which we are transitioning
	fir_config_struct_t* prev_fir_config_struct_ptr;    // Previous config from which we are transitioning
	fir_config_struct_t* queue_fir_config_struct_ptr;   // Just in-case if we get new config during transition
	                                                    // If we get multiple configs during transition, only the latest will be remembered
	int32 fir_config_size;
	int32 fir_config_flag;                  // 0 during init, 1 when fir_config_struct_ptr is populated using set_param
	int32 prev_fir_config_flag;             // 0 during init, 1 when prev_fir_config_struct_ptr is populated during transition, 0 after transition is completed
	int32 queue_fir_config_flag;            // 0 during init, 1 when queue_fir_config_struct_ptr is populated if setparam triggered during transition
	                                                       // set back to 0 after fir_config_struct_ptr is populated using queue_fir_config_struct_ptr
	fir_state_struct_t* fir_state_struct_ptr;       // ptr to the state struct in lib mem
	fir_state_struct_t* prev_fir_state_struct_ptr;  // ptr to the state struct in lib mem for prev coeffs
    int32 fir_state_struct_size;                    // size of the allocated mem pointed by the state struct
	int32* out32_prev_ptr;                          // ptr to the chunk of memory allocated to save the output with prev coeffs for 32-bit input
	int16* out16_prev_ptr;                          // ptr to the chunk of memory allocated to save the output with prev coeffs for 32-bit input

} fir_lib_mem_t;


void fir_lib_reset(fir_filter_t *filter, int32 data_width);
void fir_lib_process_c16xd16_rnd(fir_filter_t *filter, int16 *dest, int16 *src, int32 samples, int16 qx);
void fir_lib_process_c32xd16_rnd(fir_filter_t *filter, int16 *dest, int16 *src, int32 samples, int16 qx);
void fir_lib_process_c16xd32_rnd(fir_filter_t *filter, int32 *dest, int32 *src, int32 samples, int16 qx);
void fir_lib_process_c32xd32_rnd(fir_filter_t *filter, int32 *dest, int32 *src, int32 samples, int16 qx);

/*----------------------------------------------------------------------------
* Local function
* -------------------------------------------------------------------------*/

FIR_RESULT fir_audio_cross_fade_16(fir_panner_struct_t* pData, int16* pOutPtrL16, int16* pPrevOutPtrL16, int32 cross_fading_samples_current_frame);
FIR_RESULT fir_audio_cross_fade_32(fir_panner_struct_t* pData, int32* pOutPtrL32, int32* pPrevOutPtrL32, int32 cross_fading_samples_current_frame);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* #ifndef FIRLIB_H */
