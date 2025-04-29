#ifndef FIR_CALIBRATION_API_H
#define FIR_CALIBRATION_API_H
/*============================================================================
  @file CFIRCalibApi.h

  Public api for FIR.

Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear
============================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "AEEStdDef.h"
#include "audio_dsp.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/
#define DATA_16BIT	16
#define DATA_32BIT	32
#define COEF_16BIT	16
#define COEF_32BIT	32
#define FIR_DISABLED 0
#define FIR_ENABLED 1
#define MAX_NUM_TAPS_DEFAULT 1024
#define DATA_WIDTH_DEFAULT 16


// param ID and the corresponding payload for lib
#define FIR_PARAM_GET_LIB_VER (0)			// read only
typedef int32	fir_lib_ver_t;				// lib version(major.minor.bug); (8bits.16bits.8bits)

// param ID and the corresponding payload for FIR feature mode
#define FIR_PARAM_FEATURE_MODE (1)			// read/write
typedef int32	fir_feature_mode_t;			// 1 is with FIR processing; 0 is no FIR filtering

// param ID and the corresponding payload for FIR filtering
#define FIR_PARAM_CONFIG (2)				// read/write
typedef struct fir_config_struct_t
{
	uint64	coeffs_ptr;						// stores the address of coeff buffer starting point
	uint32	coef_width;						// 16 :16-bits coef  or 32: 32-bits coef
	int16	coefQFactor;					// Qfactor of the coeffs
	int16	num_taps;						// Filter length

} fir_config_struct_t;


// param ID for reset(to reset internal history buffer)
// no payload needed for this ID
#define FIR_PARAM_RESET (3)		// write only


// param ID and the corresponding payload for group delay(in samples) for Linear-phase FIR only
#define FIR_PARAM_GET_LINEAR_PHASE_DELAY (4)// read only
typedef uint32	fir_delay_t;				// Q0 Delay in samples


// param ID and the corresponding payload for transition status
#define FIR_PARAM_GET_TRANSITION_STATUS (5)  // read only
typedef struct fir_transition_status_struct_t
{
	uint32	coeffs_ptr;						// Address of coeff buffer currently used.
	uint32  flag;                           // This flag indicates the status of transition (0:no transition, 1: in the middle of transition)
	uint32  reserved;
}fir_transition_status_struct_t;

// param ID and the corresponding payload for crossfading on/off
#define FIR_PARAM_CROSS_FADING_MODE (6)// write/read
typedef struct fir_cross_fading_struct_t
{
	uint32	fir_cross_fading_mode;			// Crossfadin mode: = 0-Disable, 1-Enable
	uint32  transition_period_ms;           // Transition period in milli seconds in Q0, eg. 20 ==> 20 msec.
											// min = 0ms, max = 50ms
}fir_cross_fading_struct_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* #ifndef FIR_CALIB_API_H */
