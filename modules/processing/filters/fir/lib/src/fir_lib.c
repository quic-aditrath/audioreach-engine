/*============================================================================
  FILE:          fir_lib.c

  OVERVIEW:      Implements the firiter algorithm.

  DEPENDENCIES:  None

                 Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
                 SPDX-License-Identifier: BSD-3-Clause-Clear

============================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "fir_lib.h"
#include "../inc/FIR_ASM_macro.h"
#include "audio_basic_op.h"
#include "stringl.h"
#include "audio_dsp.h"
#include "audio_divide_qx.h"
/*----------------------------------------------------------------------------
* Private Function Declarations
* -------------------------------------------------------------------------*/
FIR_RESULT fir_processing_mode(fir_static_struct_t *pStatic, fir_state_struct_t *pState, fir_config_struct_t *pCfg);

/*----------------------------------------------------------------------------
* Function Definitions
* -------------------------------------------------------------------------*/
/*======================================================================

FUNCTION      fir_get_mem_req

DESCRIPTION   Determine lib mem size. Called once at audio connection set up time.

DEPENDENCIES  Input pointers must not be NULL.

PARAMETERS    fir_lib_mem_requirements_ptr: [out] Pointer to lib mem requirements structure
fir_static_struct_ptr: [in] Pointer to static structure

SIDE EFFECTS  None

======================================================================*/
FIR_RESULT fir_get_mem_req(fir_lib_mem_requirements_t *fir_lib_mem_requirements_ptr, fir_static_struct_t* fir_static_struct_ptr)
{
	uint32 libMemStructSize;
	uint32 staticStructSize;
	uint32 featureModeStructSize;
	uint32 crossFadingStructSize;
	uint32 pannerStructSize;
	uint32 cfgStructSize;

	uint32 stateStructSize, stateSize;
	uint32 historyBufferSize, outputSize=0;
	uint32 prevOutputBufferSize;
	uint32 size;

	// clear memory
	memset(fir_lib_mem_requirements_ptr,0,sizeof(fir_lib_mem_requirements_t));

	// determine mem size
	libMemStructSize = sizeof(fir_lib_mem_t);
	libMemStructSize = ALIGN8(libMemStructSize);
	staticStructSize = sizeof(fir_static_struct_t);
	staticStructSize = ALIGN8(staticStructSize);
	featureModeStructSize = sizeof(fir_feature_mode_t);
	featureModeStructSize = ALIGN8(featureModeStructSize);
	crossFadingStructSize = sizeof(fir_cross_fading_struct_t);
	crossFadingStructSize = ALIGN8(crossFadingStructSize);
	pannerStructSize = sizeof(fir_panner_struct_t);
	pannerStructSize = ALIGN8(pannerStructSize);
	cfgStructSize = sizeof(fir_config_struct_t);
	cfgStructSize = ALIGN8(cfgStructSize);

	stateStructSize= sizeof(fir_state_struct_t);
	stateStructSize = ALIGN8(stateStructSize);

	size = sizeof(fir_filter_t);
	stateSize = ALIGN8(size);

	//coefBufferSize = (uint32)(COEF_16BIT == fir_static_struct_ptr->coef_width ? s64_shl_s64(fir_static_struct_ptr->max_num_taps,1) : s64_shl_s64(fir_static_struct_ptr->max_num_taps,2));
	//coefBufferSize = (uint32)(ALIGN8(coefBufferSize));
	historyBufferSize = (uint32)(DATA_16BIT == fir_static_struct_ptr->data_width ? s64_shl_s64(fir_static_struct_ptr->max_num_taps,1) : s64_shl_s64(fir_static_struct_ptr->max_num_taps,2));
#ifdef QDSP6_ASM_OPT_FIR_FILTER
	historyBufferSize += (uint32)(DATA_16BIT == fir_static_struct_ptr->data_width ? s64_shl_s64(MAX_PROCESS_FRAME_SIZE,1) : s64_shl_s64(MAX_PROCESS_FRAME_SIZE,2));           // [<-----STATE(=TAPS-1)----->][<------INPUT BLOCK------>] //INPUT BLOCK = 5ms of 48KHz Sampling rate = 240
	outputSize = (uint32)(DATA_16BIT == fir_static_struct_ptr->data_width ?s64_shl_s64(MAX_PROCESS_FRAME_SIZE,1) : s64_shl_s64(MAX_PROCESS_FRAME_SIZE,2));
	outputSize = (uint32) (ALIGN8(outputSize));       //Alligned to 8 bytes output buffer to store results // output block = INPUT BLOCK
#endif
	historyBufferSize = (uint32)(ALIGN8(historyBufferSize));

	prevOutputBufferSize = (uint32)(DATA_16BIT == fir_static_struct_ptr->data_width ? s64_shl_s64(fir_static_struct_ptr->frame_size, 1) : s64_shl_s64(fir_static_struct_ptr->frame_size, 2));
	prevOutputBufferSize = (uint32)(ALIGN8(prevOutputBufferSize));

	// lib memory arrangement

	// -------------------  ----> fir_lib_mem_requirements_ptr->lib_mem_size
	// fir_lib_mem_t
	// -------------------
	// fir_static_struct_t
	// -------------------
	// fir_feature_mode_t
	// -------------------
	// fir_cross_fading_struct_t
	// -------------------
	// fir_panner_struct_t
	// -------------------
	// fir_config_struct_t (for current cfg)
	// -------------------
	// fir_config_struct_t (for prev cfg)
	// -------------------
	// fir_config_struct_t (for queue cfg)
	// -------------------
	// fir_state_struct_t  (for current cfg)
	// -------------------
	// states
	// -------------------
	// history buffer
	// -------------------
	// fir_state_struct_t  (for prev cfg)
	// -------------------
	// states
	// -------------------
	// history buffer
	// -------------------
	// prev output buffer
	// -------------------

	// total lib mem needed = fir_lib_mem_t + fir_static_struct_t + fir_feature_mode_t + fir_processing_t + fir_state_struct_t + stateSize + coeff_buffer + history_buffer
	fir_lib_mem_requirements_ptr->lib_mem_size = libMemStructSize + staticStructSize + featureModeStructSize + crossFadingStructSize + pannerStructSize + cfgStructSize*3 +
												(stateStructSize + stateSize + historyBufferSize)*2 + prevOutputBufferSize + outputSize * 2;

	// maximal lib stack mem consumption
	fir_lib_mem_requirements_ptr->lib_stack_size = FIR_MAX_STACK_SIZE;

	return FIR_SUCCESS;
}


/*======================================================================

FUNCTION      fir_init_memory

DESCRIPTION   Performs partition(allocation) and initialization of lib memory for the
fir algorithm. Called once at audio connection set up time.

DEPENDENCIES  Input pointers must not be NULL.

PARAMETERS    fir_lib_ptr: [in, out] Pointer to lib structure
fir_static_struct_ptr: [in] Pointer to static structure
pMem:		[in] Pointer to the lib memory
memSize:	[in] Size of the memory pointed by pMem

SIDE EFFECTS  None

======================================================================*/
FIR_RESULT fir_init_memory(fir_lib_t *fir_lib_ptr, fir_static_struct_t *fir_static_struct_ptr, int8 *pMem, uint32 memSize)
{
	fir_lib_mem_t* pFIRLibMem = NULL;
	int8 *pTemp= pMem;

	uint32 libMemSize, libMemStructSize, staticStructSize, featureModeStructSize, crossFadingStructSize, pannerStructSize, cfgStructSize, stateStructSize, stateSize;
	uint32 historyBufferSize, prevOutputBufferSize, outputSize=0;
	// re-calculate lib mem size
	libMemStructSize = ALIGN8(sizeof(fir_lib_mem_t));
	staticStructSize = ALIGN8(sizeof(fir_static_struct_t));
	featureModeStructSize = ALIGN8(sizeof(fir_feature_mode_t));
	crossFadingStructSize = ALIGN8(sizeof(fir_cross_fading_struct_t));
	pannerStructSize = ALIGN8(sizeof(fir_panner_struct_t));
	cfgStructSize = ALIGN8(sizeof(fir_config_struct_t));

	stateStructSize = ALIGN8(sizeof(fir_state_struct_t));
	stateSize = ALIGN8(sizeof(fir_filter_t));

	//coefBufferSize = (uint32)(COEF_16BIT == fir_static_struct_ptr->coef_width ? s64_shl_s64(fir_static_struct_ptr->max_num_taps,1) : s64_shl_s64(fir_static_struct_ptr->max_num_taps,2));
	//coefBufferSize = (uint32)(ALIGN8(coefBufferSize));
	historyBufferSize = (uint32)(DATA_16BIT == fir_static_struct_ptr->data_width ? s64_shl_s64((fir_static_struct_ptr->max_num_taps),1) : s64_shl_s64((fir_static_struct_ptr->max_num_taps),2));
#ifdef QDSP6_ASM_OPT_FIR_FILTER
	historyBufferSize += (uint32)(DATA_16BIT == fir_static_struct_ptr->data_width ? s64_shl_s64(MAX_PROCESS_FRAME_SIZE,1) : s64_shl_s64(MAX_PROCESS_FRAME_SIZE,2));           // [<-----STATE(=TAPS-1)----->][<------INPUT BLOCK------>] //INPUT BLOCK = 5ms of 48KHz Sampling rate = 240
	outputSize = (uint32)(DATA_16BIT == fir_static_struct_ptr->data_width ?s64_shl_s64(MAX_PROCESS_FRAME_SIZE,1) : s64_shl_s64(MAX_PROCESS_FRAME_SIZE,2));
	outputSize = (uint32) (ALIGN8(outputSize));       //Alligned to 8 bytes output buffer to store results // output block = INPUT BLOCK
#endif
	historyBufferSize = (uint32)(ALIGN8(historyBufferSize));

	prevOutputBufferSize = (uint32)(DATA_16BIT == fir_static_struct_ptr->data_width ? s64_shl_s64(fir_static_struct_ptr->frame_size, 1) : s64_shl_s64(fir_static_struct_ptr->frame_size, 2));
	prevOutputBufferSize = (uint32)(ALIGN8(prevOutputBufferSize));

	// total lib mem needed = fir_lib_mem_t + fir_static_struct_t + fir_feature_mode_t + fir_processing_t + fir_state_struct_t + stateSize + delay buffer
	libMemSize = libMemStructSize + staticStructSize + featureModeStructSize + crossFadingStructSize + pannerStructSize + cfgStructSize * 3 +
				(stateStructSize + stateSize + historyBufferSize) * 2 + prevOutputBufferSize + outputSize * 2;

	// error out if the mem space given is not enough
	if (memSize < libMemSize)
	{
		return FIR_MEMERROR;
	}

	// before initializing lib_mem_ptr, it is FW job to make sure that pMem is 8 bytes aligned(with enough space)
	memset(pMem,0,memSize);                                   // clear the mem
	fir_lib_ptr->lib_mem_ptr = pMem;                          // init fir_lib_t;

	// lib memory arrangement

	// -------------------  ----> fir_lib_mem_requirements_ptr->lib_mem_size
	// fir_lib_mem_t
	// -------------------
	// fir_static_struct_t
	// -------------------
	// fir_feature_mode_t
	// -------------------
	// fir_cross_fading_mode_t
	// -------------------
	// fir_panner_struct_t
	// -------------------
	// fir_config_struct_t (for current cfg)
	// -------------------
	// fir_config_struct_t (for prev cfg)
	// -------------------
	// fir_config_struct_t (for queue cfg)
	// -------------------
	// fir_state_struct_t  (for current cfg)
	// -------------------
	// states
	// -------------------
	// history buffer
	// -------------------
	// fir_state_struct_t  (for prev cfg)
	// -------------------
	// states
	// -------------------
	// history buffer
	// -------------------
	// prev output buffer
	// -------------------

	// lib memory partition starts here
	pFIRLibMem = (fir_lib_mem_t*)fir_lib_ptr->lib_mem_ptr;				// allocate memory for fir_lib_mem_t
	pTemp += libMemStructSize;											// pTemp points to where fir_static_struct_t will be located

	pFIRLibMem->fir_static_struct_ptr = (fir_static_struct_t*)pTemp;	// init fir_lib_mem_t; allocate memory for fir_static_struct_t
	pFIRLibMem->fir_static_struct_size = staticStructSize;				// init fir_lib_mem_t
	// init fir_static_struct_t
	pFIRLibMem->fir_static_struct_ptr->data_width = fir_static_struct_ptr->data_width;
	pFIRLibMem->fir_static_struct_ptr->sampling_rate = fir_static_struct_ptr->sampling_rate;
	pFIRLibMem->fir_static_struct_ptr->max_num_taps = fir_static_struct_ptr->max_num_taps;
	pFIRLibMem->fir_static_struct_ptr->frame_size = fir_static_struct_ptr->frame_size;
	pTemp += pFIRLibMem->fir_static_struct_size;						// pTemp points to where fir_feature_mode_t will be located

	pFIRLibMem->fir_feature_mode_ptr = (fir_feature_mode_t*)pTemp;      // init fir_lib_mem_t; allocate memory for fir_feature_mode_t
	pFIRLibMem->fir_feature_mode_size = featureModeStructSize;          // init fir_lib_mem_t
	// init fir_processing_t with defaults
	*pFIRLibMem->fir_feature_mode_ptr = (fir_feature_mode_t)MODE_DEFAULT;
	pTemp += pFIRLibMem->fir_feature_mode_size;

	pFIRLibMem->fir_cross_fading_struct_ptr = (fir_cross_fading_struct_t*)pTemp;      // init fir_lib_mem_t; allocate memory for fir_cross_fading_struct_t
	pFIRLibMem->fir_cross_fading_struct_size = crossFadingStructSize;
	pFIRLibMem->fir_cross_fading_struct_ptr->fir_cross_fading_mode = (uint32)FIR_CROSS_FADING_MODE_DEFAULT;
	pFIRLibMem->fir_cross_fading_struct_ptr->transition_period_ms = (uint32)FIR_TRANSITION_PERIOD_MS_DEFAULT;
	pTemp += pFIRLibMem->fir_cross_fading_struct_size;

	pFIRLibMem->fir_panner_struct_ptr = (fir_panner_struct_t*)pTemp;      // init fir_lib_mem_t; allocate memory for fir_panner_struct_t
	pFIRLibMem->fir_panner_struct_size = pannerStructSize;
	// init fir_panner_struct_t with defaults
	pFIRLibMem->fir_panner_struct_ptr->max_transition_samples = divide_int32_qx(fir_static_struct_ptr->sampling_rate * pFIRLibMem->fir_cross_fading_struct_ptr->transition_period_ms, 1000, 0);
	pFIRLibMem->fir_panner_struct_ptr->remaining_transition_samples = 0;
	pFIRLibMem->fir_panner_struct_ptr->current_gain = UNITY_32BIT_Q30;    //1 in Q30
	pFIRLibMem->fir_panner_struct_ptr->gain_step = divide_int32_qx(1, pFIRLibMem->fir_panner_struct_ptr->max_transition_samples, FIR_QFACTOR_CURRENT_GAIN);//(uint32) (0x40000000/ fir_static_struct_ptr->transition_period);       //1/(fir_static_struct_ptr->transition_period) in Q30
	pTemp += pFIRLibMem->fir_panner_struct_size;							// pTemp points to where fir_config_struct_t will be located
	//current config
	pFIRLibMem->fir_config_struct_ptr = (fir_config_struct_t*)pTemp;					// init fir_lib_mem_t; allocate memory for fir_processing_t
	pFIRLibMem->fir_config_size = cfgStructSize;						// init fir_lib_mem_t
	pFIRLibMem->fir_config_struct_ptr->coefQFactor = QFACTOR_16BIT_DEFAULT;
	pFIRLibMem->fir_config_struct_ptr->num_taps = NUM_TAPS_DEFAULT;
	pFIRLibMem->fir_config_struct_ptr->coef_width = COEF_WIDTH_DEFAULT;
	pFIRLibMem->fir_config_struct_ptr->coeffs_ptr = (uint64)NULL;
	pTemp += pFIRLibMem->fir_config_size;								// pTemp points to where prev_fir_config_struct_t will be located
	//previous config
	pFIRLibMem->prev_fir_config_struct_ptr = (fir_config_struct_t*)pTemp;					// init fir_lib_mem_t; allocate memory for fir_processing_t
	pFIRLibMem->prev_fir_config_struct_ptr->coefQFactor = QFACTOR_16BIT_DEFAULT;
	pFIRLibMem->prev_fir_config_struct_ptr->num_taps = NUM_TAPS_DEFAULT;
	pFIRLibMem->prev_fir_config_struct_ptr->coef_width = COEF_WIDTH_DEFAULT;
	pFIRLibMem->prev_fir_config_struct_ptr->coeffs_ptr = (uint64)NULL;
	pTemp += pFIRLibMem->fir_config_size;								// pTemp points to where queue_fir_config_struct_t will be located
	//queue config
	pFIRLibMem->queue_fir_config_struct_ptr = (fir_config_struct_t*)pTemp;					// init fir_lib_mem_t; allocate memory for fir_processing_t
	pFIRLibMem->queue_fir_config_struct_ptr->coefQFactor = QFACTOR_16BIT_DEFAULT;
	pFIRLibMem->queue_fir_config_struct_ptr->num_taps = NUM_TAPS_DEFAULT;
	pFIRLibMem->queue_fir_config_struct_ptr->coef_width = COEF_WIDTH_DEFAULT;
	pFIRLibMem->queue_fir_config_struct_ptr->coeffs_ptr = (uint64)NULL;
	pTemp += pFIRLibMem->fir_config_size;								// pTemp points to where fir_state_struct_t will be located
	// current state struct
	pFIRLibMem->fir_state_struct_ptr = (fir_state_struct_t*)pTemp;      // init fir_lib_mem_t; allocate memory for fir_state_struct_t
	pFIRLibMem->fir_state_struct_size = stateStructSize;                // init fir_lib_mem_t
	pTemp += pFIRLibMem->fir_state_struct_size;                         // pTemp points to where fir_data(fir_filter_t*) will be pointing to
	// init fir_state_struct_t
	pFIRLibMem->fir_state_struct_ptr->fir_data = *((fir_filter_t*)pTemp);
	pFIRLibMem->fir_state_struct_ptr->fir_data.taps = pFIRLibMem->fir_static_struct_ptr->max_num_taps;
	pFIRLibMem->fir_state_struct_ptr->fir_data.mem_idx = 0;
	pFIRLibMem->fir_state_struct_ptr->fir_data.coeffs = NULL;
	pTemp += stateSize;													// pTemp points to where history buffer address in fir_data
	//pFIRLibMem->fir_config_struct_ptr->coeffs= pTemp;
	//pTemp += coefBufferSize;
	pFIRLibMem->fir_state_struct_ptr->fir_data.history = pTemp;
	pTemp += historyBufferSize;
	// prev state struct
	pFIRLibMem->prev_fir_state_struct_ptr = (fir_state_struct_t*)pTemp;      // init fir_lib_mem_t; allocate memory for fir_state_struct_t
	pTemp += pFIRLibMem->fir_state_struct_size;                         // pTemp points to where fir_data(fir_filter_t*) will be pointing to
	// init fir_state_struct_t
	pFIRLibMem->prev_fir_state_struct_ptr->fir_data = *((fir_filter_t*)pTemp);
	pFIRLibMem->prev_fir_state_struct_ptr->fir_data.taps = pFIRLibMem->fir_static_struct_ptr->max_num_taps;
	pFIRLibMem->prev_fir_state_struct_ptr->fir_data.mem_idx = 0;
	pFIRLibMem->prev_fir_state_struct_ptr->fir_data.coeffs = NULL;
	pTemp += stateSize;													// pTemp points to where history buffer address in fir_data
	//pFIRLibMem->fir_config_struct_ptr->coeffs= pTemp;
	//pTemp += coefBufferSize;
	pFIRLibMem->prev_fir_state_struct_ptr->fir_data.history = pTemp;
	pTemp += historyBufferSize;
	// previous output buffer
	if (DATA_16BIT == fir_static_struct_ptr->data_width)
	{
		pFIRLibMem->out16_prev_ptr = (int16*)pTemp;
		pFIRLibMem->out32_prev_ptr = NULL;
	}
	else
	{
		pFIRLibMem->out16_prev_ptr = NULL;
		pFIRLibMem->out32_prev_ptr = (int32*)pTemp;
	}
	pTemp += prevOutputBufferSize;

#ifdef QDSP6_ASM_OPT_FIR_FILTER
	pFIRLibMem->fir_state_struct_ptr->fir_data.output = pTemp ;
	pTemp += outputSize ;         //assigning pointer to output buffer
	pFIRLibMem->prev_fir_state_struct_ptr->fir_data.output = pTemp ;
	pTemp += outputSize ;         //assigning pointer to output buffer
#endif
	// update fir processing mode
	fir_processing_mode(pFIRLibMem->fir_static_struct_ptr, pFIRLibMem->fir_state_struct_ptr, pFIRLibMem->fir_config_struct_ptr);

	// check to see if memory partition is correct
	if (pTemp != (int8*)pMem + libMemSize)
	{
		return FIR_MEMERROR;
	}

	return FIR_SUCCESS;
}


/*======================================================================

FUNCTION      fir_get_param

DESCRIPTION   Get the default calibration params from pFIRLib and store in pMem

DEPENDENCIES  Input pointers must not be NULL.

PARAMETERS  pFIRLib: [in] Pointer to lib structure
paramID:	[in] ID of the param
pMem:		[out] Pointer to the memory where params are to be stored
memSize:	[in] Size of the memory pointed by pMem
pParamSize: [out] Pointer to param size which indicates the size of the retrieved param(s)

SIDE EFFECTS  None

======================================================================*/
FIR_RESULT fir_get_param(fir_lib_t* pFIRLib, uint32 paramID, int8 *pMem, uint32 memSize, uint32 *pParamSize)
{
	fir_lib_mem_t* pFIRLibMem = (fir_lib_mem_t*)pFIRLib->lib_mem_ptr;
	//fir_static_struct_t* pStatic = pFIRLibMem->fir_static_struct_ptr;

	memset(pMem,0,memSize);

	switch (paramID)
	{
	case FIR_PARAM_FEATURE_MODE:
		{
			// check if the memory buffer has enough space to write the parameter data
			if(memSize >= sizeof(fir_feature_mode_t))
			{
				fir_feature_mode_t*	fir_feature_mode_ptr = (fir_feature_mode_t*)pMem;
				*fir_feature_mode_ptr = *pFIRLibMem->fir_feature_mode_ptr;

				*pParamSize = sizeof(fir_feature_mode_t);
			}
			else
			{
				return FIR_MEMERROR;
			}
			break;
		}
	case FIR_PARAM_CONFIG:
		{
			// check if the memory buffer has enough space to write the parameter data
			if(memSize >= sizeof(fir_config_struct_t))
			{
				*(fir_config_struct_t*)pMem = *pFIRLibMem->fir_config_struct_ptr;

				*pParamSize = sizeof(fir_config_struct_t);
			}
			else
			{
				return FIR_MEMERROR;
			}
			break;
		}
	case FIR_PARAM_GET_LIB_VER:
		{
			// check if the memory buffer has enough space to write the parameter data
			if(memSize >= sizeof(fir_lib_ver_t))
			{
				*(fir_lib_ver_t *)pMem = FIR_LIB_VER;
				*pParamSize = sizeof(fir_lib_ver_t);
			}
			else
			{
				return FIR_MEMERROR;
			}
			break;
		}
		case FIR_PARAM_GET_TRANSITION_STATUS:
	{
		// check if the memory buffer has enough space to write the parameter data
		if (memSize >= sizeof(fir_transition_status_struct_t))
		{
			fir_transition_status_struct_t*	fir_transition_status_ptr = (fir_transition_status_struct_t*)pMem;
			fir_transition_status_ptr->coeffs_ptr = pFIRLibMem->fir_config_struct_ptr->coeffs_ptr;
			fir_transition_status_ptr->flag = pFIRLibMem->prev_fir_config_flag;
			*pParamSize = sizeof(fir_transition_status_struct_t);
		}
		else
		{
			return FIR_MEMERROR;
		}
		break;
	}
	case FIR_PARAM_CROSS_FADING_MODE:
	{
		// check if the memory buffer has enough space to write the parameter data
		if (memSize >= sizeof(fir_cross_fading_struct_t))
		{
			fir_cross_fading_struct_t*	fir_cross_fading_struct_ptr = (fir_cross_fading_struct_t*)pMem;
			fir_cross_fading_struct_ptr->fir_cross_fading_mode = pFIRLibMem->fir_cross_fading_struct_ptr->fir_cross_fading_mode;
			fir_cross_fading_struct_ptr->transition_period_ms = pFIRLibMem->fir_cross_fading_struct_ptr->transition_period_ms;

			*pParamSize = sizeof(fir_cross_fading_struct_t);
		}
		else
		{
			return FIR_MEMERROR;
		}
		break;
	}

	default:
		{

			return FIR_FAILURE;
		}
	}


	return FIR_SUCCESS;
}

/*======================================================================

FUNCTION      fir_set_param

DESCRIPTION   Set the calibration params in the lib memory using the values pointed by pMem

DEPENDENCIES  Input pointers must not be NULL.

PARAMETERS    pFIRLib: [in, out] Pointer to lib structure
paramID:	[in] ID of the param
pMem:		[in] Pointer to the memory where the values stored are used to set up the params in the lib memory
memSize:	[in] Size of the memory pointed by pMem

SIDE EFFECTS  None

======================================================================*/
FIR_RESULT fir_set_param(fir_lib_t* pFIRLib, uint32 paramID, int8* pMem, uint32 memSize)
{
	fir_lib_mem_t *pFIRLibMem = (fir_lib_mem_t*)pFIRLib->lib_mem_ptr;
	fir_static_struct_t* pStatic = pFIRLibMem->fir_static_struct_ptr;
	fir_state_struct_t* pState = pFIRLibMem->fir_state_struct_ptr;
	//uint32 bytesCoef;

	switch(paramID)
	{
	case FIR_PARAM_FEATURE_MODE:
		{
			// copy only when mem size matches to what is allocated in the lib memory
			if(memSize == sizeof(fir_feature_mode_t))
			{
				// set the calibration params in the lib memory
				*pFIRLibMem->fir_feature_mode_ptr = *(fir_feature_mode_t*)pMem;

				// update fir processing mode
				fir_processing_mode(pStatic, pFIRLibMem->fir_state_struct_ptr, pFIRLibMem->fir_config_struct_ptr );
			    if (pFIRLibMem->fir_config_struct_ptr->coeffs_ptr && pFIRLibMem->fir_cross_fading_struct_ptr->fir_cross_fading_mode && (*pFIRLibMem->fir_feature_mode_ptr))
			    {
					pFIRLibMem->fir_config_flag = 1;
				}
			}
			else //
			{

				return FIR_MEMERROR;
			}

			break;
		}
	case FIR_PARAM_CONFIG:
		{

			// copy only when mem size matches to what is allocated in the lib memory
			if(memSize == sizeof(fir_config_struct_t))
			{

				// set the calibration params in the lib memory
				//int8* tmpCoefPtr;
				fir_config_struct_t* tmpCfgPtr = (fir_config_struct_t*)pMem;
                // check if num_taps <= max_num_taps in static parameter
				if (tmpCfgPtr->num_taps <= (int16) pFIRLibMem->fir_static_struct_ptr->max_num_taps)
				{
					// we can proceed further as the num_taps is less than max_num_taps
					if (0 == pFIRLibMem->fir_config_flag)// this will be 0 only at the start of the setup
					{
						//set the config params in the library
						pFIRLibMem->fir_config_struct_ptr->coefQFactor = tmpCfgPtr->coefQFactor;
						pFIRLibMem->fir_config_struct_ptr->num_taps = tmpCfgPtr->num_taps;
						pFIRLibMem->fir_config_struct_ptr->coeffs_ptr = tmpCfgPtr->coeffs_ptr;
						pFIRLibMem->fir_config_struct_ptr->coef_width = tmpCfgPtr->coef_width;

						pFIRLibMem->fir_state_struct_ptr->fir_data.taps = pFIRLibMem->fir_config_struct_ptr->num_taps;
						pFIRLibMem->fir_state_struct_ptr->fir_data.coeffs = (void *)pFIRLibMem->fir_config_struct_ptr->coeffs_ptr;
						if ((pFIRLibMem->fir_cross_fading_struct_ptr->fir_cross_fading_mode == 1) && (*(pFIRLibMem->fir_feature_mode_ptr) == 1))
						{
							pFIRLibMem->fir_config_flag = 1;
						}
					}
					else
					{
						if (0 == pFIRLibMem->prev_fir_config_flag)
							// prev_fir_config_flag = 0 during init_memory() and
							// everytime after the crossfading is completed, prev_fir_config_flag = 0
						{
							// check num_taps of new configuration
                        	// cur num_taps <= prev num_taps
							if (tmpCfgPtr->num_taps <= pFIRLibMem->fir_config_struct_ptr->num_taps)
							{
								//copy current config params to previous
								pFIRLibMem->prev_fir_config_struct_ptr->coefQFactor = pFIRLibMem->fir_config_struct_ptr->coefQFactor;
								pFIRLibMem->prev_fir_config_struct_ptr->num_taps = pFIRLibMem->fir_config_struct_ptr->num_taps;
								pFIRLibMem->prev_fir_config_struct_ptr->coeffs_ptr = pFIRLibMem->fir_config_struct_ptr->coeffs_ptr;
								pFIRLibMem->prev_fir_config_struct_ptr->coef_width = pFIRLibMem->fir_config_struct_ptr->coef_width;

								pFIRLibMem->prev_fir_state_struct_ptr->fir_data.taps = pFIRLibMem->fir_state_struct_ptr->fir_data.taps;
								pFIRLibMem->prev_fir_state_struct_ptr->fir_data.coeffs = pFIRLibMem->fir_state_struct_ptr->fir_data.coeffs;
								uint32 frameBytes = (DATA_16BIT == pFIRLibMem->fir_static_struct_ptr->data_width ? s64_shl_s64(pFIRLibMem->fir_static_struct_ptr->max_num_taps, 1) : s64_shl_s64(pFIRLibMem->fir_static_struct_ptr->max_num_taps, 2));
								memscpy(pFIRLibMem->prev_fir_state_struct_ptr->fir_data.history, frameBytes, pFIRLibMem->fir_state_struct_ptr->fir_data.history, frameBytes);
								pFIRLibMem->prev_fir_state_struct_ptr->fir_data.mem_idx = pFIRLibMem->fir_state_struct_ptr->fir_data.mem_idx;

								pFIRLibMem->prev_fir_config_flag = 1;
								pFIRLibMem->fir_config_struct_ptr->coefQFactor = tmpCfgPtr->coefQFactor;
								pFIRLibMem->fir_config_struct_ptr->num_taps = tmpCfgPtr->num_taps;
								pFIRLibMem->fir_config_struct_ptr->coeffs_ptr = tmpCfgPtr->coeffs_ptr;
								pFIRLibMem->fir_config_struct_ptr->coef_width = tmpCfgPtr->coef_width;

								pFIRLibMem->fir_state_struct_ptr->fir_data.taps = pFIRLibMem->fir_config_struct_ptr->num_taps;
								pFIRLibMem->fir_state_struct_ptr->fir_data.coeffs = (void *)pFIRLibMem->fir_config_struct_ptr->coeffs_ptr;

								// trigger cross-fading
								// initialize panner structure
								if (pFIRLibMem->fir_panner_struct_ptr->max_transition_samples != 0) {
									pFIRLibMem->fir_panner_struct_ptr->remaining_transition_samples = pFIRLibMem->fir_panner_struct_ptr->max_transition_samples;
									pFIRLibMem->fir_panner_struct_ptr->current_gain = 0;
									pFIRLibMem->fir_panner_struct_ptr->gain_step = divide_int32_qx(1, pFIRLibMem->fir_panner_struct_ptr->max_transition_samples, FIR_QFACTOR_CURRENT_GAIN);//
								}
								else {
									pFIRLibMem->fir_panner_struct_ptr->remaining_transition_samples = 0;
									pFIRLibMem->fir_panner_struct_ptr->current_gain = UNITY_32BIT_Q30;    //1 in Q30
									pFIRLibMem->fir_panner_struct_ptr->gain_step = UNITY_32BIT_Q30;//
								}
							}
							else
							{
								//Cross-fading will not be applied
								//set the config params in the library
								pFIRLibMem->fir_config_struct_ptr->coefQFactor = tmpCfgPtr->coefQFactor;
								pFIRLibMem->fir_config_struct_ptr->num_taps = tmpCfgPtr->num_taps;
								pFIRLibMem->fir_config_struct_ptr->coeffs_ptr = tmpCfgPtr->coeffs_ptr;
								pFIRLibMem->fir_config_struct_ptr->coef_width = tmpCfgPtr->coef_width;

								pFIRLibMem->fir_state_struct_ptr->fir_data.taps = pFIRLibMem->fir_config_struct_ptr->num_taps;
								pFIRLibMem->fir_state_struct_ptr->fir_data.coeffs = (void*)pFIRLibMem->fir_config_struct_ptr->coeffs_ptr;

								if (pFIRLibMem->fir_cross_fading_struct_ptr->fir_cross_fading_mode == 1)
								{
									pFIRLibMem->fir_config_flag = 1;
								}

							}

						}
						else
							// During crossfading, both the structures, fir_config_struct and prev_fir_config_struct are filled
							// So, the config params are stored in queue_fir_config_struct
						{
							// check num_taps of new configuration
							// queue num_taps <= cur num_taps
							if (tmpCfgPtr->num_taps <= pFIRLibMem->fir_config_struct_ptr->num_taps)
							{
								pFIRLibMem->queue_fir_config_struct_ptr->coefQFactor = tmpCfgPtr->coefQFactor;
								pFIRLibMem->queue_fir_config_struct_ptr->num_taps = tmpCfgPtr->num_taps;
								pFIRLibMem->queue_fir_config_struct_ptr->coeffs_ptr = tmpCfgPtr->coeffs_ptr;
								pFIRLibMem->queue_fir_config_struct_ptr->coef_width = tmpCfgPtr->coef_width;

								pFIRLibMem->queue_fir_config_flag = 1;
							}
							else
							{
								return FIR_FAILURE;
							}
						}
					}


					// update fir processing mode
					fir_processing_mode(pStatic, pFIRLibMem->fir_state_struct_ptr, pFIRLibMem->fir_config_struct_ptr);
				}
				else
				{
					return FIR_FAILURE;
				}

			}
			else //
			{

				return FIR_MEMERROR;
			}

			break;
		}
	case FIR_PARAM_RESET:
		{
			// Reset internal states(flush memory) here; wrapper no need to provide memory space for doing this

				pState->fir_data.mem_idx = 0;
				fir_lib_reset(&(pState->fir_data), pStatic->data_width);

				/*tmpPtr = (int8*)pState->fir_data.history;

				bytesHistory = (pStatic->data_width == DATA_16BIT) ? 2*pStatic->num_taps : 4*pStatic->num_taps;
				memset(tmpPtr,0x0,bytesHistory);*/

				break;

		}
	case FIR_PARAM_CROSS_FADING_MODE:
	{
		// copy only when mem size matches to what is allocated in the lib memory
		if (memSize == sizeof(fir_cross_fading_struct_t))
		{
			fir_cross_fading_struct_t* tmpCrossFadingPtr = (fir_cross_fading_struct_t*)pMem;
			// set the calibration params in the lib memory
			pFIRLibMem->fir_cross_fading_struct_ptr->fir_cross_fading_mode = tmpCrossFadingPtr->fir_cross_fading_mode;
			// update fir config flag
			if (pFIRLibMem->fir_cross_fading_struct_ptr->fir_cross_fading_mode == 0)
			{
				pFIRLibMem->fir_config_flag = 0;
				pFIRLibMem->prev_fir_config_flag = 0;
				pFIRLibMem->queue_fir_config_flag = 0;
			}

			if (pFIRLibMem->fir_cross_fading_struct_ptr->transition_period_ms != tmpCrossFadingPtr->transition_period_ms)
			{
				pFIRLibMem->fir_cross_fading_struct_ptr->transition_period_ms = tmpCrossFadingPtr->transition_period_ms;
				//calculate max_transition_samples by crossfading duration
				pFIRLibMem->fir_panner_struct_ptr->max_transition_samples = divide_int32_qx(pFIRLibMem->fir_static_struct_ptr->sampling_rate * pFIRLibMem->fir_cross_fading_struct_ptr->transition_period_ms, 1000, 0);
			}

			if (pFIRLibMem->fir_config_struct_ptr->coeffs_ptr && pFIRLibMem->fir_cross_fading_struct_ptr->fir_cross_fading_mode && (*pFIRLibMem->fir_feature_mode_ptr))
			{
				pFIRLibMem->fir_config_flag = 1;
			}

		}
		else //
		{

			return FIR_MEMERROR;
		}

		break;
	}

	default:
		{
			return FIR_FAILURE;
		}
	}

	return FIR_SUCCESS;

}



/*======================================================================

FUNCTION      fir_process

DESCRIPTION   Process single-channel input audio signal
sample by sample. The input can be in any sampling rate
- 8, 16, 22.05, 32, 44.1, 48, 96, 192KHz. If the input is 16-bit
Q15 and the output is also in the form of 16-bit Q15. If
the input is 32-bit Q27, the output is also in the form of 32-bit Q27.

DEPENDENCIES  Input pointers must not be NULL.

PARAMETERS    pFIRLib: [in] Pointer to lib structure
pOutPtr: [out] Pointer to single - channel output PCM samples
pInPtr: [in] Pointer to single - channel input PCM samples
samples: [in] Number of samples to be processed

SIDE EFFECTS  None.

======================================================================*/

FIR_RESULT fir_module_process(fir_lib_t *pFIRLib, int8 *pOutPtr, int8 *pInPtr, uint32 samples)
{
	fir_lib_mem_t* pFIRLibMem = (fir_lib_mem_t*)pFIRLib->lib_mem_ptr;
	fir_static_struct_t* pStatic = pFIRLibMem->fir_static_struct_ptr;
	fir_state_struct_t *pState = pFIRLibMem->fir_state_struct_ptr;
	fir_config_struct_t* pCfg = pFIRLibMem->fir_config_struct_ptr;

	//Previoues config and previous state
	fir_state_struct_t *pPrevState = pFIRLibMem->prev_fir_state_struct_ptr;
	fir_config_struct_t* pPrevCfg = pFIRLibMem->prev_fir_config_struct_ptr;
	fir_panner_struct_t* pPannerStruct = pFIRLibMem->fir_panner_struct_ptr;

	if (samples > pFIRLibMem->fir_static_struct_ptr->frame_size)
	{
		return FIR_FAILURE;
	}

	//-------------------- variable declarations -----------------------------

	if(((int8 *)pCfg->coeffs_ptr==NULL)||(*(pFIRLibMem->fir_feature_mode_ptr) == FIR_DISABLED))
	{
		uint32 frameBytes = (pStatic->data_width == DATA_16BIT) ? 2*samples : 4*samples;

		memscpy(pOutPtr, frameBytes, pInPtr, frameBytes);
	}
	else
	{
		// switching between fir processing modes
		switch(pState->firProcessMode)
		{
		case COEF16XDATA16:
			fir_lib_process_c16xd16_rnd(&(pState->fir_data), (int16*)pOutPtr, (int16*)pInPtr, samples, pCfg->coefQFactor);
			if (1 == pFIRLibMem->prev_fir_config_flag)
			{
				fir_lib_process_c16xd16_rnd(&(pPrevState->fir_data), pFIRLibMem->out16_prev_ptr, (int16*)pInPtr, samples, pPrevCfg->coefQFactor);
			}
			break;

		case COEF32XDATA16:

			fir_lib_process_c32xd16_rnd(&(pState->fir_data), (int16*)pOutPtr, (int16*)pInPtr, samples, pCfg->coefQFactor);
			if (1 == pFIRLibMem->prev_fir_config_flag)
			{
				fir_lib_process_c32xd16_rnd(&(pPrevState->fir_data), pFIRLibMem->out16_prev_ptr, (int16*)pInPtr, samples, pPrevCfg->coefQFactor);
			}
			break;

		case COEF16XDATA32:

			fir_lib_process_c16xd32_rnd(&(pState->fir_data), (int32*)pOutPtr, (int32*)pInPtr, samples, pCfg->coefQFactor);
			if (1 == pFIRLibMem->prev_fir_config_flag)
			{
				fir_lib_process_c16xd32_rnd(&(pPrevState->fir_data), pFIRLibMem->out32_prev_ptr, (int32*)pInPtr, samples, pPrevCfg->coefQFactor);
			}
			break;

		case COEF32XDATA32:

			fir_lib_process_c32xd32_rnd(&(pState->fir_data), (int32*)pOutPtr, (int32*)pInPtr, samples, pCfg->coefQFactor);
			if (1 == pFIRLibMem->prev_fir_config_flag)
			{
				fir_lib_process_c32xd32_rnd(&(pPrevState->fir_data), pFIRLibMem->out32_prev_ptr, (int32*)pInPtr, samples, pPrevCfg->coefQFactor);
			}
			break;


		default:

			return FIR_FAILURE;
		}
		if (1 == pFIRLibMem->prev_fir_config_flag)
		{
			int32 cross_fading_samples_current_frame = samples;
			if(samples > pPannerStruct->remaining_transition_samples)
				cross_fading_samples_current_frame = pPannerStruct->remaining_transition_samples;
			if (pStatic->data_width == DATA_16BIT)
			{
				//crossfade with pOutPtr and pFIRLibMem->out16_prev_ptr
				fir_audio_cross_fade_16(pPannerStruct, (int16*)pOutPtr, pFIRLibMem->out16_prev_ptr, cross_fading_samples_current_frame);
			}
			else
			{
				//crossfade with pOutPtr and pFIRLibMem->out32_prev_ptr
				fir_audio_cross_fade_32(pPannerStruct, (int32*)pOutPtr, pFIRLibMem->out32_prev_ptr, cross_fading_samples_current_frame);
			}
			// if remaining samples = 0, make transition flag = 0
			if (0 == pPannerStruct->remaining_transition_samples)
			{
				pFIRLibMem->prev_fir_config_flag = 0;
				pFIRLibMem->fir_panner_struct_ptr->current_gain = 1;
				// check queue
				if (1 == pFIRLibMem->queue_fir_config_flag)
				{
					//call setparam with queue config
					fir_set_param(pFIRLib, FIR_PARAM_CONFIG, (int8*)pFIRLibMem->queue_fir_config_struct_ptr, sizeof(fir_config_struct_t));
					pFIRLibMem->queue_fir_config_flag = 0;
				}
			}
		}
	}

	return FIR_SUCCESS;
}


FIR_RESULT fir_audio_cross_fade_16(fir_panner_struct_t *pData,
                       int16 *pOutPtrL16,//out
                       int16 *pPrevOutPtrL16,//prev_out
                       int32 cross_fading_samples_current_frame)
{
    int16   temp1L16, temp2L16, i = 0;
	//current_gain 0->1
	//out = (1-current_gain) * prev_out + current_gain * out
	//out = prev_out + current_gain * (out - prev_out)
	//   temp1L16 = (out - prev_out)
	//   temp2L16 = current_gain * temp1L16
	//   out = temp2L16 + prev_out;
	for (i = 0; i < cross_fading_samples_current_frame; i++)
    {
		temp1L16 = s16_sub_s16_s16(pOutPtrL16[i] , pPrevOutPtrL16[i]);
		// current_gain -> Q30 ,  temp1L16 -> Q_IN_16
        temp2L16 = (int16)s64_mult_s32_s16_shift(pData->current_gain , temp1L16, 16 - FIR_QFACTOR_CURRENT_GAIN);
		// temp2L16 -> Q_IN_16
		pOutPtrL16[i] = s16_add_s16_s16_sat(temp2L16, pPrevOutPtrL16[i]);

        // Update current_gain; current_gain += gain_step;
        pData->current_gain = s32_add_s32_s32_sat(pData->current_gain, pData->gain_step);
		if(pData->current_gain > UNITY_32BIT_Q30)
			pData->current_gain = UNITY_32BIT_Q30;

    }

	pData->remaining_transition_samples -= cross_fading_samples_current_frame;
	return FIR_SUCCESS;
}

FIR_RESULT fir_audio_cross_fade_32(fir_panner_struct_t *pData,
	int32 *pOutPtrL32,//out
	int32 *pPrevOutPtrL32,//prev_out
	int32 cross_fading_samples_current_frame)
{
	int32   temp1L32, temp2L32, i = 0;
	//current_gain 0->1
	//out = (1-current_gain) * prev_out + current_gain * out
	//out = prev_out + current_gain * (out - prev_out)
	//   temp1L32 = (out - prev_out)
	//   temp2L32 = current_gain * temp1L32
	//   out = temp2L32 + prev_out;
	for (i = 0; i < cross_fading_samples_current_frame; i++)
	{
		temp1L32 = s32_sub_s32_s32(pOutPtrL32[i], pPrevOutPtrL32[i]);
		// current_gain -> Q30 ,  temp1L32 -> Q_IN_32
		temp2L32 = (int32)s64_mult_s32_s32_shift(pData->current_gain, temp1L32, 32 - FIR_QFACTOR_CURRENT_GAIN);
		// temp2L32 -> Q_IN_32
		pOutPtrL32[i] = s32_add_s32_s32_sat(temp2L32, pPrevOutPtrL32[i]);

		// Update current_gain; current_gain += gain_step;
		pData->current_gain = s32_add_s32_s32_sat(pData->current_gain, pData->gain_step);
		if (pData->current_gain > UNITY_32BIT_Q30)
			pData->current_gain = UNITY_32BIT_Q30;

	}

	pData->remaining_transition_samples -= cross_fading_samples_current_frame;

	return FIR_SUCCESS;
}

/*======================================================================

FUNCTION      fir_processing_mode

DESCRIPTION   Checks on the static/calib parameters to determine FIR processing mode

PARAMETERS    pStatic: [in] pointer to the static structure
pCfg:	[in] point to the config structure
pState: [out] pointer to the state structure that saves the FIR processing states

RETURN VALUE  Failure or Success

SIDE EFFECTS  None.

======================================================================*/
FIR_RESULT fir_processing_mode(fir_static_struct_t *pStatic, fir_state_struct_t *pState, fir_config_struct_t * pCfg)
{

	// FIR process mode determination to avoid checks in the process function (Ying)

	if (DATA_16BIT == pStatic->data_width) // 16bit
	{
		if (COEF_16BIT == pCfg->coef_width)
		{
			pState->firProcessMode = COEF16XDATA16;
		}
		else
		{
			pState->firProcessMode = COEF32XDATA16;
		}
	}
	else // 32bit
	{
		if (COEF_16BIT == pCfg->coef_width)
		{
			pState->firProcessMode = COEF16XDATA32;
		}
		else
		{
			pState->firProcessMode = COEF32XDATA32;
		}
	}


	return FIR_SUCCESS;
}



