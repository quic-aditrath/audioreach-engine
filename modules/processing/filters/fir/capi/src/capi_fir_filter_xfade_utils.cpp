/* ======================================================================== */
/**
  @file  capi_fir_filter_xfade_utils.cpp

  C++ source file to implement the utility functions for
  Common Audio Processor Interface v2 for FIR module.
 */

/* =========================================================================
 * Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
   ========================================================================= */

/*------------------------------------------------------------------------
 * Include files and Macro definitions
 * -----------------------------------------------------------------------*/
#include "capi_fir_filter.h"
#include "capi_fir_filter_utils.h"
/**/
/*------------------------------------------------------------------------
 * Macro definitions and Structure definitions
 * -----------------------------------------------------------------------*/

/*------------------------------------------------------------------------
 * Static declarations
 * -----------------------------------------------------------------------*/

/*------------------------------------------------------------------------
 * Function definitions
 * -----------------------------------------------------------------------*/
/*========================================================================
  Function name: capi_fir_check_channel_map_crossfade_cfg
  Description : Function to check the channel map for crossfade configuration.
  ========================================================================*/

capi_err_t capi_fir_check_channel_map_crossfade_cfg(fir_filter_crossfade_cfg_t *cfg_ptr, uint32_t num_config)
{
   uint64_t channel_mask = 0;
   uint64_t channel_map  = 0;

   for (uint32_t count = 0; count < num_config; count++, cfg_ptr++)
   {
      channel_map = capi_fir_calculate_channel_mask(cfg_ptr->channel_mask_lsb, cfg_ptr->channel_mask_msb);
      if (channel_mask & channel_map)
      {
         return CAPI_EBADPARAM;
      }
      channel_mask |= channel_map;
   }
   return CAPI_EOK;
}

/*========================================================================
  Function name: capi_fir_cache_crossfade_payload
  Description : Function to cache the crossfade payload.
  ========================================================================*/

capi_err_t capi_fir_cache_crossfade_payload(capi_fir_t *   me_ptr,
                                            int8_t *const  payload_ptr,
                                            const uint32_t size_of_payload)
{
   if (me_ptr->cache_fir_crossfade_cfg_size != size_of_payload)
   {
      // allocate memory for config
      if (NULL != me_ptr->cache_fir_crossfade_cfg)
      {
         posal_memory_free(me_ptr->cache_fir_crossfade_cfg);
         me_ptr->cache_fir_crossfade_cfg      = NULL;
         me_ptr->cache_fir_crossfade_cfg_size = 0;
      }

      int8_t *temp_crossfade_cache_ptr =
         (int8_t *)posal_memory_malloc(size_of_payload, (POSAL_HEAP_ID)me_ptr->heap_info.heap_id);
      if (NULL == temp_crossfade_cache_ptr)
      {
         FIR_MSG(me_ptr->miid, DBG_FATAL_PRIO, "No memory to cache fir_filter_crossfade config. Requires %lu bytes",
                size_of_payload);
         return CAPI_ENOMEMORY;
      }
      me_ptr->cache_fir_crossfade_cfg = (param_id_fir_filter_crossfade_cfg_t *)temp_crossfade_cache_ptr;
   }

   memscpy(me_ptr->cache_fir_crossfade_cfg, size_of_payload, payload_ptr, size_of_payload);
   me_ptr->cache_fir_crossfade_cfg_size = size_of_payload;
   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Cached crossfade param of %lu bytes", size_of_payload);
   return CAPI_EOK;
}
/*========================================================================
  Function name: capi_fir_set_cached_filter_crossfade_config
  Description : Function to set the cached crossfade payload.
   	   	   	    This is called only when none of the channels are in transition.
  ========================================================================*/

capi_err_t capi_fir_set_cached_filter_crossfade_config(capi_fir_t *me_ptr)
{
   param_id_fir_filter_crossfade_cfg_t *fir_crossfade_cfg      = me_ptr->cache_fir_crossfade_cfg;
   int8_t *                             temp_crossfade_cfg_ptr = (int8_t *)me_ptr->cache_fir_crossfade_cfg;

   if ((NULL == fir_crossfade_cfg))
   {
      if(TRUE == me_ptr->is_xfade_cfg_pending)
	  {
	     FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Invalid state for crossfade param");
         return CAPI_EFAILED;
	  }
	  return CAPI_EOK;
   }

   uint32_t num_cfg = fir_crossfade_cfg->num_config;
   temp_crossfade_cfg_ptr += sizeof(param_id_fir_filter_crossfade_cfg_t);
   fir_filter_crossfade_cfg_t *const fir_cfade_cfg = (fir_filter_crossfade_cfg_t *)temp_crossfade_cfg_ptr;

   capi_fir_channel_lib_t *fir_channel_lib = me_ptr->fir_channel_lib;

   const uint32_t channels_allocated = me_ptr->input_media_fmt[0].format.num_channels;
   FIR_RESULT     set_param_result   = FIR_SUCCESS;
   uint64_t       channel_map        = 0;
   uint64_t       channel_mask       = 0;
   me_ptr->xfade_flag                = FALSE; // Initially False everytime we come to set param: cross-fade disabled

   for (uint32_t chan_num = 0; chan_num < channels_allocated; chan_num++)
   {
      channel_map      = ((uint64_t)1) << me_ptr->input_media_fmt[0].channel_type[chan_num];
      bool_t temp_flag = FALSE;

      if (NULL != fir_channel_lib[chan_num].fir_lib_instance.lib_mem_ptr)
      {
         for (uint32_t count = 0; count < num_cfg; count++)
         {
            channel_mask = capi_fir_calculate_channel_mask(fir_cfade_cfg[count].channel_mask_lsb,
                                                           fir_cfade_cfg[count].channel_mask_msb);
            if (channel_map & channel_mask) // if config exists for this channel
            {

               temp_flag = TRUE;
               // storing library info
               fir_channel_lib[chan_num].fir_crossfade_variables.fir_cross_fading_mode =
                  fir_cfade_cfg[count].fir_cross_fading_mode;
               fir_channel_lib[chan_num].fir_crossfade_variables.transition_period_ms =
                  fir_cfade_cfg[count].transition_period_ms;
#ifdef CAPI_FIR_DEBUG_MSG
               FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "cross-fade mode: %d for channel %lu",
                      fir_cfade_cfg[count].fir_cross_fading_mode,
                      chan_num);
               FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "cross-fade period: %d for channel %lu",
                      fir_cfade_cfg[count].transition_period_ms,
                      chan_num);
#endif
               set_param_result = fir_set_param(&(fir_channel_lib[chan_num].fir_lib_instance),
                                                FIR_PARAM_CROSS_FADING_MODE,
                                                (int8_t *)&fir_channel_lib[chan_num].fir_crossfade_variables,
                                                sizeof(fir_cross_fading_struct_t));
               if (FIR_SUCCESS != set_param_result)
               {
                  FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Crossfade Set param failed for channel %lu with lib error %u",
                         chan_num,
                         set_param_result);
				  me_ptr->is_xfade_cfg_pending = -1; //failed to apply config
                  return CAPI_EFAILED;
               }
               // if any ch has xfade enabled, xfade_flag would be true
               me_ptr->xfade_flag |= fir_cfade_cfg[count].fir_cross_fading_mode;
               break;
            }
         }
         // did not get config for this channel but library exists; set default xfade values for this channel
         if (!temp_flag) // temp_flag is still False, did not get xfade config for this channel, do set param for
                         // default values
         {
            // xfade flag will always be false here
            fir_channel_lib[chan_num].fir_crossfade_variables.fir_cross_fading_mode = 0;
            fir_channel_lib[chan_num].fir_crossfade_variables.transition_period_ms  = 0;
            set_param_result = fir_set_param(&(fir_channel_lib[chan_num].fir_lib_instance),
                                             FIR_PARAM_CROSS_FADING_MODE,
                                             (int8_t *)&fir_channel_lib[chan_num].fir_crossfade_variables,
                                             sizeof(fir_cross_fading_struct_t));
            if (FIR_SUCCESS != set_param_result)
            {
               FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Setting default values for ch %d failed for crossfade config with result %d",
                      chan_num,
                      set_param_result);
			   me_ptr->is_xfade_cfg_pending = -1; //failed to apply config
               return CAPI_EFAILED;
            }
         }
      }
   }
   me_ptr->is_xfade_cfg_pending = FALSE; // reset to false after successfully setting xfade param
   return CAPI_EOK;
}

/*========================================================================
  Function name: capi_fir_check_combined_crossfade
  Description : Function to check if cross-fade in library completed for all the channels
  ========================================================================*/
capi_err_t capi_fir_check_combined_crossfade(capi_fir_t *me_ptr, uint32_t chan_num)
{
   // empty structure for storing transition status flag
   fir_transition_status_struct_t fir_transition_status = { 0, 0, 0 };
   uint32_t                       filled_struct_size    = 0;
   FIR_RESULT                     lib_result            = FIR_SUCCESS;
   capi_fir_channel_lib_t *const  lib_instance          = me_ptr->fir_channel_lib;

   lib_result = fir_get_param(&(lib_instance[chan_num].fir_lib_instance),
                              FIR_PARAM_GET_TRANSITION_STATUS,
                              (int8_t *)(&fir_transition_status),
                              sizeof(fir_transition_status_struct_t),
                              &filled_struct_size);
   if ((FIR_SUCCESS != lib_result))
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed in getting transition status for channel %lu", chan_num);
      return CAPI_EFAILED;
   }
   else // received transition status then proceed
   {
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Transition status of crossfade for channel %lu : %lu ",
             chan_num,
             fir_transition_status.flag);
#endif
      // if Transition status changed from 1 to 0 then set new crossfade config if any
      if (fir_transition_status.flag)
      {
	     // update combined_crossfade_status if atleast 1 of the ch is still in transition
         if(VERSION_V1 == me_ptr->cfg_version)
    	 {
            me_ptr->combined_crossfade_status = fir_transition_status.flag;
    	 }
         else
         {
            me_ptr->capi_fir_v2_cfg.combined_crossfade_status = fir_transition_status.flag;
         }
      }
      // storing current information in capi: will check this flag in set filter config
      lib_instance[chan_num].fir_transition_status_variables.flag       = fir_transition_status.flag;
      lib_instance[chan_num].fir_transition_status_variables.coeffs_ptr = fir_transition_status.coeffs_ptr;
   }
   return CAPI_EOK;
}

/*========================================================================
  Function name: capi_fir_update_release_config
  Description : Function to update current config then free next config pointers
  ========================================================================*/
void capi_fir_update_release_config(capi_fir_t *me_ptr)
{
   if (me_ptr->cache_original_next_fir_coeff_cfg) // if new cfg pointer in not null and xfade is completed then copy c2
                                                 // to c1
   {
      me_ptr->config_type = CAPI_NEXT_CFG;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "New filter config found in process(). Freeing previous");
#endif
      posal_memory_aligned_free(me_ptr->cache_original_fir_coeff_cfg);                          // free prev config
      capi_fir_update_config(me_ptr);                   // update current filter config with new filter config
      capi_fir_release_config_pointers(me_ptr);         // release new filter config pointers (c2)
      if (me_ptr->cache_original_queue_fir_coeff_cfg) // if there is a pending config then make it as next config
      {
         me_ptr->config_type = CAPI_QUEUE_CFG;
#ifdef CAPI_FIR_DEBUG_MSG
         FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Pending filter config found in process(). Freeing previous");
#endif
         capi_fir_update_config(me_ptr);           // update next filter config with pending filter config
         capi_fir_release_config_pointers(me_ptr); // release pending filter config pointers
      }
   }
}

/*========================================================================
  Function name: capi_update_current_config
  Description : Function to update the config pointers after crossfade is completed
                current config will point to the configuration it has crossfaded
  ========================================================================*/

void capi_fir_update_config(capi_fir_t *me_ptr)
{
   if (me_ptr->config_type == CAPI_NEXT_CFG)
   {
      me_ptr->cache_original_fir_coeff_cfg = me_ptr->cache_original_next_fir_coeff_cfg;
      me_ptr->cache_fir_coeff_cfg          = me_ptr->cache_next_fir_coeff_cfg;
      me_ptr->cache_fir_coeff_cfg_size     = me_ptr->cache_next_fir_coeff_cfg_size;
      me_ptr->size_req_for_get_coeff       = me_ptr->size_req_for_get_next_coeff;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Copied next filter config to current filter config");
#endif
   }
   else if (me_ptr->config_type == CAPI_QUEUE_CFG)
   {
      me_ptr->cache_original_next_fir_coeff_cfg = me_ptr->cache_original_queue_fir_coeff_cfg;
      me_ptr->cache_next_fir_coeff_cfg          = me_ptr->cache_queue_fir_coeff_cfg;
      me_ptr->cache_next_fir_coeff_cfg_size     = me_ptr->cache_queue_fir_coeff_cfg_size;
      me_ptr->size_req_for_get_next_coeff       = me_ptr->size_req_for_get_queue_coeff;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Copied pending filter config to next filter config");
#endif
   }
}

/*========================================================================
  Function name: capi_release_config_pointers
  Description : Function to free the config pointers after crossfading is done
  ========================================================================*/

void capi_fir_release_config_pointers(capi_fir_t *me_ptr)
{
   if (me_ptr->config_type == CAPI_NEXT_CFG)
   {
      me_ptr->cache_original_next_fir_coeff_cfg = NULL;
      me_ptr->cache_next_fir_coeff_cfg          = NULL;
      me_ptr->cache_next_fir_coeff_cfg_size     = 0;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Released next config pointers");
#endif
   }
   else if (me_ptr->config_type == CAPI_QUEUE_CFG)
   {
      me_ptr->cache_original_queue_fir_coeff_cfg = NULL;
      me_ptr->cache_queue_fir_coeff_cfg          = NULL;
      me_ptr->cache_queue_fir_coeff_cfg_size     = 0;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Released pending config pointers");
#endif
   }
}
