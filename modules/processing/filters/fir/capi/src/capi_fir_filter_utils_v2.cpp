/* ======================================================================== */
/**
  @file  capi_fir_filter_utils_v2.cpp

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

/*===========================================================================
    FUNCTION : capi_fir_set_fir_filter_max_tap_length_v2
    DESCRIPTION: Function to set max tap length for FIR filter
===========================================================================*/
capi_err_t capi_fir_set_fir_filter_max_tap_length_v2(capi_fir_t *me_ptr,
                                                     uint32_t    param_id,
                                                     uint32_t    param_size,
                                                     int8_t *    data_ptr)
{
   capi_err_t capi_result = CAPI_EOK;
   if (param_size < sizeof(param_id_fir_filter_max_tap_cfg_v2_t))
   {
      FIR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "Max Fir tap length SetParam 0x%lx, invalid param size %lx ",
              param_id,
              param_size);
      capi_result = CAPI_ENEEDMORE;
      return capi_result;
   }
   uint32_t req_payload_size          = 0;
   uint32_t base_payload_size         = MAX_TAP_BASE_PAYLOAD_SIZE;
   uint32_t per_cfg_base_payload_size = MAX_TAP_PER_CFG_BASE_PAYLOAD_SIZE;
   capi_result                        = capi_fir_validate_multichannel_v2_payload(me_ptr,
                                                           data_ptr,
                                                           param_id,
                                                           param_size,
                                                           &req_payload_size,
                                                           base_payload_size,
                                                           per_cfg_base_payload_size);
   if (CAPI_FAILED(capi_result))
   {
      FIR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "Max Fir tap length V2 SetParam 0x%lx, invalid param size %lu ,required_size %lu",
              param_id,
              param_size,
              req_payload_size);
      return capi_result;
   }
   if (param_size < req_payload_size)
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Insufficient payload size %d. Req size %lu", param_size, req_payload_size);
      capi_result = CAPI_ENEEDMORE;
   }
   else
   {
      // cache the payload
      if (capi_fir_has_max_tap_v2_payload_changed(me_ptr, data_ptr, req_payload_size))
      {
         capi_result = capi_fir_cache_max_tap_v2_payload(me_ptr, data_ptr, param_size);
         if (CAPI_FAILED(capi_result))
         {
            capi_fir_clean_up_memory(me_ptr);
            return capi_result;
         }

         // check and create library instances
         capi_result |= capi_fir_check_create_lib_instance_v2(me_ptr, FALSE);
         if (CAPI_FAILED(capi_result))
         {
            return capi_result;
         }

         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Max tap length V2 Set Param set Successfully");
      }
      else
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "No change in max tap length param config");
      } // break;
   }
   // break;
   return CAPI_EOK;
}

capi_err_t capi_fir_validate_multichannel_v2_payload(capi_fir_t *me_ptr,
                                                     int8_t *    data_ptr,
                                                     uint32_t    param_id,
                                                     uint32_t    param_size,
                                                     uint32_t *  req_payload_size,
                                                     uint32_t    base_payload_size,
                                                     uint32_t    per_cfg_base_payload_size)
{
   //int8_t * temp_cfg_ptr = data_ptr;
   uint32_t capi_result      = CAPI_EOK;
   uint32_t num_cfg          = *((uint32_t *)data_ptr); //get num_cfg from arguments

#ifdef CAPI_FIR_DEBUG_MSG
   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Received num_config = %lu for PID = 0x%lx", num_cfg, param_id);
#endif

   if (num_cfg < 1 || num_cfg > PCM_MAX_CHANNEL_MAP_V2)
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Received incorrect num_config parameter - %lu", num_cfg);
      return CAPI_EBADPARAM;
   }

   capi_result = capi_fir_validate_per_channel_v2_payload(me_ptr,
                                                          num_cfg,
                                                          data_ptr,
                                                          param_size,
                                                          req_payload_size,
                                                          param_id,
                                                          base_payload_size,
                                                          per_cfg_base_payload_size);
   if (CAPI_FAILED(capi_result))
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Configuartion SetParam 0x%lx failed.", param_id);
      return capi_result;
   }
   return CAPI_EOK;
}

capi_err_t capi_fir_validate_per_channel_v2_payload(capi_fir_t *me_ptr,
                                                    uint32_t    num_cfg,
                                                    int8_t *    data_ptr,
                                                    uint32_t    param_size,
                                                    uint32_t *  required_size_ptr,
                                                    uint32_t    param_id,
                                                    uint32_t    base_payload_size,
                                                    uint32_t    per_cfg_base_payload_size)
{
   capi_err_t capi_result         = CAPI_EOK;
   int8_t *  base_payload_ptr     = data_ptr; // points to the start of the payload
   uint32_t  config_size          = 0;
   uint32_t  per_cfg_payload_size = 0;
   *required_size_ptr += base_payload_size; // 4B
   base_payload_ptr += *required_size_ptr;  // 4B
   // configuration payload
   // _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
   //| num_ | channel_type_group_mask | dynamic channel_type_mask_list[0] | payload1 |  _ _ _ _
   //|config| _ _ _ _ _ _ _ _ _ _ _ _ |_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _  |_ _ _ _ _ |_ _ _ _ _ _
   //

   for (uint32_t count = 0; count < num_cfg; count++)
   {
      if ((*required_size_ptr + per_cfg_base_payload_size) > param_size)
      {
         FIR_MSG(me_ptr->miid,
                 DBG_MED_PRIO,
                 "Failed while validating required size for config #%lu. Total config: %lu. Total size required till this point is %lu. "
				 "Param size recieved %lu",
                 count,
                 num_cfg,
				 (*required_size_ptr + per_cfg_base_payload_size),
				 param_size);
         return CAPI_ENEEDMORE;
      }
      else
      {
         uint32_t num_taps           = 0;
         uint32_t ch_type_group_mask = 0;
         base_payload_ptr            += config_size;
         ch_type_group_mask          = *((uint32_t *)base_payload_ptr);

#ifdef CAPI_FIR_DEBUG_MSG
         FIR_MSG(me_ptr->miid,
                 DBG_MED_PRIO,
                 "Channel group mask %lu received for param %#lx.",
                 ch_type_group_mask,
                 param_id);
#endif
         // check for group mask payload if only desired bits are set or not
         if (0 == (ch_type_group_mask >> CAPI_CMN_MAX_CHANNEL_MAP_GROUPS))
         {
            if (PARAM_ID_FIR_FILTER_CONFIG_V2 == param_id)
            {
               capi_fir_filter_cfg_static_params *temp_ptr = (capi_fir_filter_cfg_static_params*)(base_payload_ptr
               		                                        + CAPI_CMN_INT32_SIZE_IN_BYTES + capi_cmn_count_set_bits(ch_type_group_mask) * CAPI_CMN_INT32_SIZE_IN_BYTES);
               num_taps             = temp_ptr->num_taps;
               per_cfg_payload_size = (per_cfg_base_payload_size +
                                      (num_taps << 2)); // per cfg size excluding size for channel mask list
#ifdef CAPI_FIR_DEBUG_MSG
               FIR_MSG(me_ptr->miid,
            		   DBG_HIGH_PRIO,
					   "num_taps is %lu, base payload size for payload :%lu is %lu, "
            		   "base payload size including size for filter coeff for payload %lu is %lu",
					   num_taps,
					   count,
					   per_cfg_base_payload_size,
					   count,
					   per_cfg_payload_size);
#endif
            }
            else
            {
               per_cfg_payload_size = per_cfg_base_payload_size; // per cfg size excluding size for channel mask list
            }
            capi_result = capi_cmn_check_payload_validation(me_ptr->miid, ch_type_group_mask, per_cfg_payload_size,
           		    count, param_size, &config_size, required_size_ptr);
            if(CAPI_FAILED(capi_result))
            {
               return capi_result;
            }
         }
         else
         {
            FIR_MSG(me_ptr->miid,
                    DBG_ERROR_PRIO,
                    "Invalid configuration of channel group mask: %lu. More than maximum valid channel groups are "
                    "set, maximum valid channel groups: %lu.",
                    ch_type_group_mask,
                    CAPI_CMN_MAX_CHANNEL_MAP_GROUPS);
            return CAPI_EBADPARAM;
         }
      }
   }
#ifdef CAPI_FIR_DEBUG_MSG
   FIR_MSG(me_ptr->miid, DBG_MED_PRIO, "calculated size for param is %lu", *required_size_ptr);
#endif

   // validate the configs for duplication
   if (!capi_fir_check_multi_ch_channel_mask_v2_param(me_ptr->miid,
			                                     num_cfg,
												 param_id,
	                                             (int8_t *)data_ptr,
	                                             base_payload_size,
												 per_cfg_base_payload_size))
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Trying to set different configuration to same channel, returning");
      return CAPI_EBADPARAM;
   }
   return CAPI_EOK;
}

/* Returns FALSE if same channel is set to multiple times in different configs */
bool_t capi_fir_check_multi_ch_channel_mask_v2_param(uint32_t miid,
		                                    uint32_t    num_config,
                                            uint32_t    param_id,
                                            int8_t *    param_ptr,
                                            uint32_t    base_payload_size,
											uint32_t per_cfg_base_payload_size)
{
   uint32_t *temp_mask_list_ptr                               = NULL;
   int8_t *  data_ptr                                         = param_ptr; // points to the start of the payload
   uint32_t  check_channel_mask_arr[CAPI_CMN_MAX_CHANNEL_MAP_GROUPS]   = { 0 };
   uint32_t  current_channel_mask_arr[CAPI_CMN_MAX_CHANNEL_MAP_GROUPS] = { 0 };
   bool_t    check                                            = TRUE;
   uint32_t  offset                                           = CAPI_CMN_INT32_SIZE_IN_BYTES;
   uint32_t  per_cfg_payload_size                             = 0;

   for (uint32_t cnfg_cntr = 0; cnfg_cntr < num_config; cnfg_cntr++)
   {
      uint32_t channel_group_mask = 0;
      // Increment data ptr by calculated offset to point at next payload's group_mask
      data_ptr += offset;
      channel_group_mask          = *((uint32_t *)data_ptr);
      if (PARAM_ID_FIR_FILTER_CONFIG_V2 == param_id)
      {
         capi_fir_filter_cfg_static_params *temp_ptr = (capi_fir_filter_cfg_static_params*)(data_ptr
         		                                        + CAPI_CMN_INT32_SIZE_IN_BYTES + capi_cmn_count_set_bits(channel_group_mask) * CAPI_CMN_INT32_SIZE_IN_BYTES);


         uint32_t num_taps             = temp_ptr->num_taps;
         per_cfg_payload_size = (per_cfg_base_payload_size +
                                 (num_taps << 2)); // per cfg size excluding size for channel mask list
#ifdef CAPI_FIR_DEBUG_MSG
         FIR_MSG(miid, DBG_HIGH_PRIO, "num_taps is %lu, base payload size for payload :%lu is %lu, "
      		   "base payload size including size for filter coeff for payload %lu is %lu, channel_group_mask %#lx, offset %lu",
				   num_taps,
				   cnfg_cntr,
				   per_cfg_base_payload_size,
				   cnfg_cntr,
				   per_cfg_payload_size,
				   channel_group_mask,
				   offset);
#endif
      }
      else
      {
         per_cfg_payload_size = per_cfg_base_payload_size; // per cfg size excluding size for channel mask list
      }
      temp_mask_list_ptr = (uint32_t *)(data_ptr + CAPI_CMN_INT32_SIZE_IN_BYTES);
      if(!capi_cmn_check_v2_channel_mask_duplication(miid,
	                                                 cnfg_cntr,
    		                                         channel_group_mask,
    		                                         temp_mask_list_ptr,
													 current_channel_mask_arr,
													 check_channel_mask_arr,
													 &offset,
													 per_cfg_payload_size))
      {
    	  return FALSE;
      }
   }
   return check;
}

/*
 * Checks whether the max tap length payload has changed.
 * It will check if the new max tap v2 payload is bit exact with the cached max tap v2 payload or not.
 * The tools team always sends a max tap length config
 * along with the filter coeff config. During runtime, resetting the max tap length causes a pop noise so we
 * check if the payload is exact same with the previous config before caching and resetting the
 * library based on the new config.
 */
bool_t capi_fir_has_max_tap_v2_payload_changed(capi_fir_t *me_ptr, int8_t *const payload_ptr, uint32_t req_payload_size)
{
   bool_t result = false;
   if (NULL == me_ptr->capi_fir_v2_cfg.cache_fir_max_tap)
   {
      return true;
   }
   int8_t *new_temp_tap_cfg_ptr = (int8_t *)payload_ptr;
   int8_t *temp_tap_cfg_ptr     = (int8_t *)me_ptr->capi_fir_v2_cfg.cache_fir_max_tap;

   if (0 != memcmp(new_temp_tap_cfg_ptr, temp_tap_cfg_ptr, req_payload_size))
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Max tap length config changed.");
      return TRUE;
   }
   return result;
}

/*===============================================================================
  Function name: capi_fir_cache_max_tap_payload
  Description : Function to cache max tap length payload.
  ===============================================================================*/

capi_err_t capi_fir_cache_max_tap_v2_payload(capi_fir_t *   me_ptr,
                                             int8_t *const  payload_ptr,
                                             const uint32_t size_of_payload)
{
   if (me_ptr->capi_fir_v2_cfg.cache_fir_max_tap_size != size_of_payload)
   {
      if (NULL != me_ptr->capi_fir_v2_cfg.cache_fir_max_tap)
      {
         posal_memory_free(me_ptr->capi_fir_v2_cfg.cache_fir_max_tap);
         me_ptr->capi_fir_v2_cfg.cache_fir_max_tap      = NULL;
         me_ptr->capi_fir_v2_cfg.cache_fir_max_tap_size = 0;
      }

      int8_t *temp_max_tap_cache_ptr =
         (int8_t *)posal_memory_malloc(size_of_payload, (POSAL_HEAP_ID)me_ptr->heap_info.heap_id);
      if (NULL == temp_max_tap_cache_ptr)
      {
         FIR_MSG(me_ptr->miid,
                 DBG_FATAL_PRIO,
                 "No memory to cache fir_filter_max tap config. Requires %lu bytes",
                 size_of_payload);
         return CAPI_ENOMEMORY;
      }
      me_ptr->capi_fir_v2_cfg.cache_fir_max_tap = (param_id_fir_filter_max_tap_cfg_v2_t *)temp_max_tap_cache_ptr;
   }

   memscpy(me_ptr->capi_fir_v2_cfg.cache_fir_max_tap, size_of_payload, payload_ptr, size_of_payload);
   me_ptr->capi_fir_v2_cfg.cache_fir_max_tap_size = size_of_payload;
   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Cached fir_filter_max tap v2 config of %lu bytes", size_of_payload);

   me_ptr->cfg_version = VERSION_V2; // indicates V2 version is cached now

   return CAPI_EOK;
}

/*===========================================================================================
  Function name: capi_fir_check_create_lib_instance
  Description : Function to check the media format, static params and create the fir instance.
  ===========================================================================================*/
capi_err_t capi_fir_check_create_lib_instance_v2(capi_fir_t *me_ptr, bool_t is_num_channels_changed)
{
   capi_err_t capi_result  = CAPI_EOK;
   FIR_RESULT lib_result   = FIR_SUCCESS;
   uint32_t   num_channels = me_ptr->input_media_fmt[0].format.num_channels;
   uint32_t   chan_num;

   if (0 == num_channels)
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Not creating lib. Media format is not received yet.");
      return CAPI_EOK;
   }

   if ((is_num_channels_changed) || (NULL == me_ptr->fir_channel_lib))
   {
      // Allocate memory required per channel
      capi_result = capi_fir_allocate_per_channel_lib_struct(me_ptr, num_channels);
      if (CAPI_FAILED(capi_result))
      {
         return capi_result;
      }
   }

   if (NULL == me_ptr->capi_fir_v2_cfg.cache_fir_max_tap)
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Not creating lib. Max Tap length v2 is not received yet.");
      return CAPI_EOK;
   }

   capi_fir_set_static_params_v2(me_ptr, MAX_TAP_PER_CFG_BASE_PAYLOAD_SIZE);

   capi_fir_channel_lib_t *fir_channel_lib = me_ptr->fir_channel_lib;

   for (chan_num = 0; chan_num < num_channels; chan_num++)
   {
      // Memory structure
      fir_lib_mem_requirements_t *const lib_mem_req          = &fir_channel_lib[chan_num].lib_mem_req;
      fir_lib_t *const                  fir_lib_instance     = &fir_channel_lib[chan_num].fir_lib_instance;
      fir_static_struct_t *const        lib_static_variables = &fir_channel_lib[chan_num].fir_static_variables;
      fir_lib_mem_requirements_t        temp_lib_mem_req     = { 0, 0 };

      // check if max taps is greater than 0 or not
      if (0 == lib_static_variables->max_num_taps)
      {
         if (NULL != fir_lib_instance->lib_mem_ptr)
         {
            posal_memory_free(fir_lib_instance->lib_mem_ptr);
            fir_lib_instance->lib_mem_ptr = NULL;
            *lib_mem_req                  = capi_fir_null_mem_req;
         }
         continue;
      }

      // Get library memory requirement
      lib_result = (FIR_RESULT)fir_get_mem_req(&temp_lib_mem_req, lib_static_variables);
      if ((FIR_SUCCESS != lib_result) || (0 == temp_lib_mem_req.lib_mem_size))
      {
         FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Error while obtaining library memory size for channel %lu.", chan_num);
         capi_fir_clean_up_memory(me_ptr);
         return CAPI_EFAILED;
      }

      int8_t *temp_ptr = NULL;;
      if ((temp_lib_mem_req.lib_mem_size != lib_mem_req->lib_mem_size) || (NULL == fir_lib_instance->lib_mem_ptr))
      {
#ifdef CAPI_FIR_DEBUG_MSG
         FIR_MSG(me_ptr->miid,
                 DBG_LOW_PRIO,
                 "Allocating library memory, current size %lu, required size %lu\n",
                 lib_mem_req->lib_mem_size,
                 temp_lib_mem_req.lib_mem_size);
#endif

         // Release the allocated library memory
         if (NULL != fir_lib_instance->lib_mem_ptr)
         {
            posal_memory_free(fir_lib_instance->lib_mem_ptr);
            fir_lib_instance->lib_mem_ptr = NULL;
            *lib_mem_req                  = capi_fir_null_mem_req;
         }

         // Reallocate library memory
         temp_ptr =
            (int8_t *)posal_memory_malloc(temp_lib_mem_req.lib_mem_size, (POSAL_HEAP_ID)me_ptr->heap_info.heap_id);
         if (NULL == temp_ptr)
         {
            FIR_MSG(me_ptr->miid,
                    DBG_FATAL_PRIO,
                    "Failed creating new instance of library: Out of Memory for chan : %lu!",
                    chan_num);
            capi_fir_clean_up_memory(me_ptr);
            return CAPI_ENOMEMORY;
         }
      }
      else
      {
         temp_ptr = (int8_t *)fir_lib_instance->lib_mem_ptr;
      }

      lib_result = fir_init_memory(fir_lib_instance, lib_static_variables, temp_ptr, temp_lib_mem_req.lib_mem_size);
      if (lib_result != FIR_SUCCESS)
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed initializing the library memory for chan %lu!", chan_num);
         capi_fir_clean_up_memory(me_ptr);
         return CAPI_EFAILED;
      }

      *lib_mem_req = temp_lib_mem_req;
   }

   // set param for enable
   capi_result |= capi_fir_lib_set_enable_param(me_ptr);
   if (CAPI_FAILED(capi_result))
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed setting enable param.");
      capi_fir_clean_up_memory(me_ptr);
      return CAPI_EFAILED;
   }

   // 1.set cached pending filter coefficients if present (c3)
   if (NULL != me_ptr->capi_fir_v2_cfg.cache_original_queue_fir_coeff_cfg)
   {
      me_ptr->capi_fir_v2_cfg.config_type = CAPI_QUEUE_CFG;
      capi_result |= capi_fir_set_cached_filter_coeff_v2(me_ptr);
      // free prev configs
      capi_fir_update_release_config_v2(me_ptr); // this will delete 1st config
      capi_fir_update_release_config_v2(me_ptr); // this will delete the 2nd config
      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed setting coeff config param after creating new library instance.");
         capi_fir_clean_up_cached_coeff_memory_v2(me_ptr); // clean all configs present
         return CAPI_EFAILED;
      }
   }
   // 2.set cached new filter coefficients if present (c2)
   else if (NULL != me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg)
   {
      me_ptr->capi_fir_v2_cfg.config_type = CAPI_NEXT_CFG;
      capi_result |= capi_fir_set_cached_filter_coeff_v2(me_ptr);
      capi_fir_update_release_config_v2(me_ptr); // free prev config
      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed setting coeff config param after creating new library instance.");
         capi_fir_clean_up_cached_coeff_memory_v2(me_ptr); // clean all configs present
         return CAPI_EFAILED;
      }
   }
   // 3.set cached current filter coefficients if present (c1)
   else if (NULL != me_ptr->capi_fir_v2_cfg.cache_original_fir_coeff_cfg)
   {
      me_ptr->capi_fir_v2_cfg.config_type = CAPI_CURR_CFG;
      capi_result |= capi_fir_set_cached_filter_coeff_v2(me_ptr);
      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed setting coeff config param after creating new library instance.");
         capi_fir_clean_up_cached_coeff_memory_v2(me_ptr); // clean all configs present
         return CAPI_EFAILED;
      }
   }
   else
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Filter config not present in create lib instance");
   }

   // set cached crossfade config if present
   capi_result |= capi_fir_set_cached_filter_crossfade_config_v2(me_ptr);
   if (CAPI_FAILED(capi_result))
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed setting filter crossfade config param.");
      return CAPI_EFAILED;
   }
   capi_fir_raise_events(me_ptr);
   return capi_result;
}

/*=================================================================================
  Function name: capi_fir_set_static_params
  Description : Function to populate static parameters.
 ==================================================================================*/
void capi_fir_set_static_params_v2(capi_fir_t *me_ptr, uint32_t per_cfg_base_payload_size)
{
   uint32_t *                            temp_cfg_ptr       = NULL;
   uint32_t *                            temp_tap_len_ptr   = NULL;
   uint32_t                              channel_type       = 0;
   param_id_fir_filter_max_tap_cfg_v2_t *fir_max_tap_len    = me_ptr->capi_fir_v2_cfg.cache_fir_max_tap;
   uint32_t                              num_cfg            = fir_max_tap_len->num_config;
   capi_fir_channel_lib_t *              fir_channel_lib    = me_ptr->fir_channel_lib;
   uint32_t                              channels_allocated = me_ptr->input_media_fmt[0].format.num_channels;

   for (uint32_t chan_num = 0; (chan_num < channels_allocated) && (chan_num < CAPI_MAX_CHANNELS_V2); chan_num++)
   {
      int8_t * temp_tap_cfg_ptr                                   = (int8_t *)me_ptr->capi_fir_v2_cfg.cache_fir_max_tap;
      uint32_t offset                                             = sizeof(param_id_fir_filter_max_tap_cfg_v2_t);
      fir_channel_lib[chan_num].fir_static_variables.max_num_taps = 0;
      fir_channel_lib[chan_num].fir_static_variables.data_width   = me_ptr->input_media_fmt[0].format.bits_per_sample;
      fir_channel_lib[chan_num].fir_static_variables.sampling_rate = me_ptr->input_media_fmt[0].format.sampling_rate;
      fir_channel_lib[chan_num].fir_static_variables.frame_size    = CAPI_MAX_PROCESS_FRAME_SIZE;

      channel_type = me_ptr->input_media_fmt[0].channel_type[chan_num];
      for (uint32_t count = 0; count < num_cfg; count++)
      {
         uint32_t channel_group_mask = 0;
         uint32_t ch_mask_list_size  = 0;
         uint32_t curr_ch_mask       = 0;
         uint32_t ch_index_grp_no    = 0;
         // Increment data ptr by calculated offset to point at next payload's group_mask
         temp_tap_cfg_ptr += offset;
         fir_filter_max_tap_length_cfg_v2_t *fir_max_tap_cfg = (fir_filter_max_tap_length_cfg_v2_t *)temp_tap_cfg_ptr;
         channel_group_mask                                  = fir_max_tap_cfg->channel_type_group_mask;
         temp_cfg_ptr             = (uint32_t *)(temp_tap_cfg_ptr + CAPI_CMN_INT32_SIZE_IN_BYTES); // points to mask array
         ch_mask_list_size        = capi_cmn_count_set_bits(channel_group_mask);
         temp_tap_len_ptr         = temp_cfg_ptr + ch_mask_list_size;
         curr_ch_mask             = (1 << CAPI_CMN_MOD_WITH_32(channel_type));
         ch_index_grp_no          = CAPI_CMN_DIVIDE_WITH_32(channel_type);

         // check if a group is configured. If yes, update ch_index_mask_to_log_ptr
         if (CAPI_CMN_IS_BIT_SET_AT_POS_IN_32B_VAL(channel_group_mask, ch_index_grp_no))
         {
            uint32_t set_index = capi_cmn_count_set_bits_in_lower_n_bits(channel_group_mask, ch_index_grp_no);
#ifdef CAPI_FIR_DEBUG_MSG
            FIR_MSG(me_ptr->miid,
                    DBG_HIGH_PRIO,
                    "curr_ch_mask %#lx, temp_cfg_ptr[%lu]: %#lx",
                    curr_ch_mask,
                    set_index,
                    temp_cfg_ptr[set_index]);
#endif
            if (curr_ch_mask & temp_cfg_ptr[set_index])
            {

               fir_channel_lib[chan_num].fir_static_variables.max_num_taps = *temp_tap_len_ptr;
#ifdef CAPI_FIR_DEBUG_MSG
               FIR_MSG(me_ptr->miid,
                       DBG_HIGH_PRIO,
                       "chan_num %lu, channel_type: %lu, max_num_taps: %lu",
                       chan_num,
                       channel_type,
                       *temp_tap_len_ptr);
#endif
               break;
            }
         }
         offset = per_cfg_base_payload_size + (ch_mask_list_size * CAPI_CMN_INT32_SIZE_IN_BYTES);
      }
   }
}

capi_err_t capi_fir_get_filter_max_tap_length_v2(capi_fir_t *me_ptr,
                                                 capi_buf_t *params_ptr,
                                                 uint32_t    param_id,
                                                 uint32_t    miid)
{
   capi_err_t capi_result = CAPI_EOK;
   if (NULL != me_ptr->capi_fir_v2_cfg.cache_fir_max_tap)
   {
      params_ptr->actual_data_len = 0;
      if (params_ptr->max_data_len >= me_ptr->capi_fir_v2_cfg.cache_fir_max_tap_size)
      {
         params_ptr->actual_data_len = 0;
         param_id_fir_filter_max_tap_cfg_v2_t *pfir_max_tap_length =
            (param_id_fir_filter_max_tap_cfg_v2_t *)(params_ptr->data_ptr);
         param_id_fir_filter_max_tap_cfg_v2_t *cached_max_tap_param =
            (param_id_fir_filter_max_tap_cfg_v2_t *)me_ptr->capi_fir_v2_cfg.cache_fir_max_tap;
         params_ptr->actual_data_len = memscpy(pfir_max_tap_length,
                                               params_ptr->max_data_len,
                                               cached_max_tap_param,
                                               me_ptr->capi_fir_v2_cfg.cache_fir_max_tap_size);
      }
      else
      {
         params_ptr->actual_data_len = me_ptr->capi_fir_v2_cfg.cache_fir_max_tap_size;

         FIR_MSG(miid,
                 DBG_ERROR_PRIO,
                 "Get Max tap length, Bad param size %lu, Req size: %lu",
                 params_ptr->max_data_len,
                 me_ptr->capi_fir_v2_cfg.cache_fir_max_tap_size);

         capi_result |= CAPI_ENEEDMORE;
      }
   }
   else
   {
      FIR_MSG(miid, DBG_ERROR_PRIO, "Did not receive any set param for parameter 0x%lx", param_id);
      capi_result |= CAPI_EFAILED;
   }
   return CAPI_EOK;
}

// crossfade fucntions
capi_err_t capi_fir_set_fir_filter_crossfade_v2(capi_fir_t *me_ptr,
                                                uint32_t    param_id,
                                                uint32_t    param_size,
                                                int8_t *    data_ptr)
{
   capi_err_t capi_result = CAPI_EOK;
   if (param_size < sizeof(param_id_fir_filter_crossfade_cfg_v2_t))
   {
      FIR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "configuration SetParam 0x%lx, invalid param size %lx ",
              param_id,
              param_size);
      capi_result = CAPI_ENEEDMORE;
      return capi_result;
   }
   uint32_t req_payload_size          = 0;
   uint32_t base_payload_size         = CROSSFADE_BASE_PAYLOAD_SIZE;
   uint32_t per_cfg_base_payload_size = CROSSFADE_PER_CFG_BASE_PAYLOAD_SIZE;
   capi_result                        = capi_fir_validate_multichannel_v2_payload(me_ptr,
                                                           data_ptr,
                                                           param_id,
                                                           param_size,
                                                           &req_payload_size,
                                                           base_payload_size,
                                                           per_cfg_base_payload_size);
   if (CAPI_FAILED(capi_result))
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Crossfade V2 SetParam 0x%lx failed", param_id);
      return capi_result;
   }
   if (param_size < req_payload_size)
   {
      FIR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "Crossfade SetParam 0x%lx, invalid param size %lu ,required_size %lu",
              param_id,
              param_size,
              req_payload_size);
      return CAPI_ENEEDMORE;
   }
   int8_t *source_ptr = data_ptr;
   // cache the payload
   capi_result = capi_fir_cache_crossfade_v2_payload(me_ptr, source_ptr, param_size);
   if (capi_result)
   {
      return capi_result;
   }
   // set param if library is created AND no active cross-fade in any of the channels
   if (NULL != me_ptr->fir_channel_lib)
   {
      if (0 == me_ptr->capi_fir_v2_cfg.combined_crossfade_status)
      {
         capi_result = capi_fir_set_cached_filter_crossfade_config_v2(me_ptr);
      }
      else
      {
         me_ptr->capi_fir_v2_cfg.is_xfade_cfg_pending = TRUE;
      }
      if (CAPI_FAILED(capi_result))
      {
         return capi_result;
      }
   }
   else
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Library not created yet.");
   }

   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Set/cache param of crossfade config param is set Successfully ");

   me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg_size = param_size;

   return CAPI_EOK;
}

/*========================================================================
  Function name: capi_fir_cache_crossfade_v2_payload
  Description : Function to cache the V2 crossfade payload.
  ========================================================================*/

capi_err_t capi_fir_cache_crossfade_v2_payload(capi_fir_t *   me_ptr,
                                               int8_t *const  payload_ptr,
                                               const uint32_t size_of_payload)
{
   if (me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg_size != size_of_payload)
   {
      // allocate memory for config
      if (NULL != me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg)
      {
         posal_memory_free(me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg);
         me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg      = NULL;
         me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg_size = 0;
      }

      int8_t *temp_crossfade_cache_ptr =
         (int8_t *)posal_memory_malloc(size_of_payload, (POSAL_HEAP_ID)me_ptr->heap_info.heap_id);
      if (NULL == temp_crossfade_cache_ptr)
      {
         FIR_MSG(me_ptr->miid,
                 DBG_FATAL_PRIO,
                 "No memory to cache fir_filter_crossfade config. Requires %lu bytes",
                 size_of_payload);
         return CAPI_ENOMEMORY;
      }
      me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg =
         (param_id_fir_filter_crossfade_cfg_v2_t *)temp_crossfade_cache_ptr;
   }

   memscpy(me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg, size_of_payload, payload_ptr, size_of_payload);
   me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg_size = size_of_payload;

   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Cached crossfade v2 param of %lu bytes", size_of_payload);

   me_ptr->cfg_version = VERSION_V2; // indicates V2 version is cached now

   return CAPI_EOK;
}

/*========================================================================
  Function name: capi_fir_set_cached_filter_crossfade_config_v2
  Description : Function to set the cached crossfade payload.
                      This is called only when none of the channels are in transition.
  ========================================================================*/

capi_err_t capi_fir_set_cached_filter_crossfade_config_v2(capi_fir_t *me_ptr)
{
   param_id_fir_filter_crossfade_cfg_v2_t *fir_crossfade_cfg       = me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg;
   uint32_t *                              temp_cfg_ptr            = NULL;
   uint32_t *                              temp_xfade_mode_ptr     = NULL;
   uint32_t *                              temp_xfade_duration_ptr = NULL;
   uint32_t                                channel_type            = 0;

   if (NULL == fir_crossfade_cfg)
   {
      if (TRUE == me_ptr->capi_fir_v2_cfg.is_xfade_cfg_pending)
      {
         FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Invalid state for crossfade param");
         return CAPI_EFAILED;
      }
      return CAPI_EOK;
   }

   uint32_t num_cfg = fir_crossfade_cfg->num_config;

   capi_fir_channel_lib_t *fir_channel_lib = me_ptr->fir_channel_lib;

   const uint32_t channels_allocated  = me_ptr->input_media_fmt[0].format.num_channels;
   FIR_RESULT     set_param_result    = FIR_SUCCESS;
   me_ptr->capi_fir_v2_cfg.xfade_flag = FALSE; // Initially False everytime we come to set param: cross-fade disabled

   for (uint32_t chan_num = 0; (chan_num < channels_allocated) && (chan_num < CAPI_MAX_CHANNELS_V2); chan_num++)
   {
      int8_t * temp_crossfade_cfg_ptr = (int8_t *)me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg;
      uint32_t offset                 = sizeof(param_id_fir_filter_crossfade_cfg_v2_t); // 4B
      bool_t   temp_flag              = FALSE;
      if (NULL != fir_channel_lib[chan_num].fir_lib_instance.lib_mem_ptr)
      {
         channel_type = me_ptr->input_media_fmt[0].channel_type[chan_num];
         for (uint32_t count = 0; count < num_cfg; count++)
         {
            uint32_t channel_group_mask = 0;
            uint32_t ch_mask_list_size  = 0;
            // Increment data ptr by calculated offset to point at next payload's group_mask
            temp_crossfade_cfg_ptr += offset;
            fir_filter_crossfade_cfg_v2_t *fir_xfade_cfg_ptr = (fir_filter_crossfade_cfg_v2_t *)temp_crossfade_cfg_ptr;
            channel_group_mask                               = fir_xfade_cfg_ptr->channel_type_group_mask;
            temp_cfg_ptr        = (uint32_t *)(temp_crossfade_cfg_ptr + CAPI_CMN_INT32_SIZE_IN_BYTES); // points to mask array
            ch_mask_list_size   = capi_cmn_count_set_bits(channel_group_mask);
            temp_xfade_mode_ptr = temp_cfg_ptr + ch_mask_list_size;
            temp_xfade_duration_ptr  = temp_xfade_mode_ptr + 1;
            uint32_t curr_ch_mask    = (1 << CAPI_CMN_MOD_WITH_32(channel_type));
            uint32_t ch_index_grp_no = CAPI_CMN_DIVIDE_WITH_32(channel_type);
            // check if a group is configured. If yes, update mask to log
            if (CAPI_CMN_IS_BIT_SET_AT_POS_IN_32B_VAL(channel_group_mask, ch_index_grp_no))
            {
               uint32_t set_index = capi_cmn_count_set_bits_in_lower_n_bits(channel_group_mask,
                                          ch_index_grp_no); // set_index would tell the exact index for this group in payload
               if (curr_ch_mask & temp_cfg_ptr[set_index])
               {
                  temp_flag = TRUE;
                  // storing library info
                  fir_channel_lib[chan_num].fir_crossfade_variables.fir_cross_fading_mode = *temp_xfade_mode_ptr;
                  fir_channel_lib[chan_num].fir_crossfade_variables.transition_period_ms  = *temp_xfade_duration_ptr;
#ifdef CAPI_FIR_DEBUG_MSG
                  FIR_MSG(me_ptr->miid,
                          DBG_HIGH_PRIO,
                          "cross-fade mode: %d, cross-fade period: %d, for channel %lu",
                          *temp_xfade_mode_ptr,
						  *temp_xfade_duration_ptr,
                          chan_num);
#endif
                  set_param_result = fir_set_param(&(fir_channel_lib[chan_num].fir_lib_instance),
                                                   FIR_PARAM_CROSS_FADING_MODE,
                                                   (int8_t *)&fir_channel_lib[chan_num].fir_crossfade_variables,
                                                   sizeof(fir_cross_fading_struct_t));
                  if (FIR_SUCCESS != set_param_result)
                  {
                     FIR_MSG(me_ptr->miid,
                             DBG_ERROR_PRIO,
                             "Crossfade Set param failed for channel %lu with lib error %u",
                             chan_num,
                             set_param_result);
                     me_ptr->capi_fir_v2_cfg.is_xfade_cfg_pending = -1; // failed to apply config
                     return CAPI_EFAILED;
                  }
                  // if any ch has xfade enabled, xfade_flag would be true
                  me_ptr->capi_fir_v2_cfg.xfade_flag |= *temp_xfade_mode_ptr;
                  break;
               }
            }
            offset = sizeof(fir_filter_crossfade_cfg_v2_t) + (ch_mask_list_size << 2);
         }
         // did not get config for this channel but library exists; set default xfade values for this channel
         if (!temp_flag) // temp_flag is still False, did not received xfade config for this channel, do set param for
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
               FIR_MSG(me_ptr->miid,
                       DBG_ERROR_PRIO,
                       "Setting default values for ch %d failed for crossfade config with result %d",
                       chan_num,
                       set_param_result);
               me_ptr->capi_fir_v2_cfg.is_xfade_cfg_pending = -1; // failed to apply config
               return CAPI_EFAILED;
            }
         }
      }
   }
   me_ptr->capi_fir_v2_cfg.is_xfade_cfg_pending = FALSE; // reset to false after successfully setting xfade param
   return CAPI_EOK;
}

/*========================================================================
  Function name: capi_fir_check_combined_crossfade_v2
  Description : Function to check if cross-fade in library completed for all the channels
  ========================================================================*/
capi_err_t capi_fir_check_combined_crossfade_v2(capi_fir_t *me_ptr, uint32_t chan_num)
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
      FIR_MSG(me_ptr->miid,
              DBG_HIGH_PRIO,
              "Transition status of crossfade for channel %lu : %lu ",
              chan_num,
              fir_transition_status.flag);
#endif
      // if Transition status changed from 1 to 0 then set new crossfade config if any
      if (fir_transition_status.flag)
      {
         // update combined_crossfade_status if atleast 1 of the ch is still in transition
         me_ptr->capi_fir_v2_cfg.combined_crossfade_status = fir_transition_status.flag;
      }
      // storing current information in capi: will check this flag in set filter config
      lib_instance[chan_num].fir_transition_status_variables.flag       = fir_transition_status.flag;
      lib_instance[chan_num].fir_transition_status_variables.coeffs_ptr = fir_transition_status.coeffs_ptr;
   }
   return CAPI_EOK;
}

/*========================================================================
  Function name: capi_fir_update_release_config_v2
  Description : Function to update current config then free next config pointers
  ========================================================================*/
void capi_fir_update_release_config_v2(capi_fir_t *me_ptr)
{
   if (me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg) // if new cfg pointer in not null and xfade is
                                                                  // completed then copy c2 to c1
   {
      me_ptr->capi_fir_v2_cfg.config_type = CAPI_NEXT_CFG;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "New filter config found in process(). Freeing previous");
#endif
      posal_memory_aligned_free(me_ptr->capi_fir_v2_cfg.cache_original_fir_coeff_cfg); // free prev config
      capi_fir_update_config_v2(me_ptr);           // update current filter config with new filter config
      capi_fir_release_config_pointers_v2(me_ptr); // release new filter config pointers (c2)
      if (me_ptr->capi_fir_v2_cfg
             .cache_original_queue_fir_coeff_cfg) // if there is a pending config then make it as next config
      {
         me_ptr->capi_fir_v2_cfg.config_type = CAPI_QUEUE_CFG;
#ifdef CAPI_FIR_DEBUG_MSG
         FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Pending filter config found in process(). Freeing previous");
#endif
         capi_fir_update_config_v2(me_ptr);           // update next filter config with pending filter config
         capi_fir_release_config_pointers_v2(me_ptr); // release pending filter config pointers
      }
   }
}

/*========================================================================
  Function name: capi_update_current_config_v2
  Description : Function to update the config pointers after crossfade is completed
                current config will point to the configuration it has crossfaded
  ========================================================================*/

void capi_fir_update_config_v2(capi_fir_t *me_ptr)
{
   if (me_ptr->capi_fir_v2_cfg.config_type == CAPI_NEXT_CFG)
   {
      me_ptr->capi_fir_v2_cfg.cache_original_fir_coeff_cfg = me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg;
      me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg          = me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg;
      me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg_size     = me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg_size;
      me_ptr->capi_fir_v2_cfg.size_req_for_get_coeff       = me_ptr->capi_fir_v2_cfg.size_req_for_get_next_coeff;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Copied next filter config to current filter config");
#endif
   }
   else if (me_ptr->capi_fir_v2_cfg.config_type == CAPI_QUEUE_CFG)
   {
      me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg =
         me_ptr->capi_fir_v2_cfg.cache_original_queue_fir_coeff_cfg;
      me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg      = me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg;
      me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg_size = me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg_size;
      me_ptr->capi_fir_v2_cfg.size_req_for_get_next_coeff   = me_ptr->capi_fir_v2_cfg.size_req_for_get_queue_coeff;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Copied pending filter config to next filter config");
#endif
   }
}

/*========================================================================
  Function name: capi_release_config_pointers_v2
  Description : Function to free the config pointers after crossfading is done
  ========================================================================*/

void capi_fir_release_config_pointers_v2(capi_fir_t *me_ptr)
{
   if (me_ptr->capi_fir_v2_cfg.config_type == CAPI_NEXT_CFG)
   {
      me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg = NULL;
      me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg          = NULL;
      me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg_size     = 0;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Released next config pointers");
#endif
   }
   else if (me_ptr->capi_fir_v2_cfg.config_type == CAPI_QUEUE_CFG)
   {
      me_ptr->capi_fir_v2_cfg.cache_original_queue_fir_coeff_cfg = NULL;
      me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg          = NULL;
      me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg_size     = 0;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Released pending config pointers");
#endif
   }
}

/*========================================================================
  Function name: capi_fir_clean_up_cached_coeff_memory_v2
  Description : Function to free the cached filter coefficient memory
  ========================================================================*/
void capi_fir_clean_up_cached_coeff_memory_v2(capi_fir_t *me_ptr)
{
   if ((CAPI_CURR_CFG == me_ptr->capi_fir_v2_cfg.config_type) &&
       (NULL != me_ptr->capi_fir_v2_cfg.cache_original_fir_coeff_cfg))
   {
      posal_memory_aligned_free(me_ptr->capi_fir_v2_cfg.cache_original_fir_coeff_cfg);
      me_ptr->capi_fir_v2_cfg.cache_original_fir_coeff_cfg = NULL;
      me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg          = NULL;
      me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg_size     = 0;
   }
   else if ((CAPI_NEXT_CFG == me_ptr->capi_fir_v2_cfg.config_type) &&
            (NULL != me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg))
   {
      posal_memory_aligned_free(me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg);
      me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg = NULL;
      me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg          = NULL;
      me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg_size     = 0;
   }
   else if ((CAPI_QUEUE_CFG == me_ptr->capi_fir_v2_cfg.config_type) &&
            (NULL != me_ptr->capi_fir_v2_cfg.cache_original_queue_fir_coeff_cfg))
   {
      posal_memory_aligned_free(me_ptr->capi_fir_v2_cfg.cache_original_queue_fir_coeff_cfg);
      me_ptr->capi_fir_v2_cfg.cache_original_queue_fir_coeff_cfg = NULL;
      me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg          = NULL;
      me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg_size     = 0;
   }
   if (NULL != me_ptr->fir_channel_lib)
   {
      uint32_t channels_allocated = me_ptr->input_media_fmt[0].format.num_channels;
      for (uint32_t chan = 0; chan < channels_allocated; chan++)
      {
         memset(&me_ptr->fir_channel_lib[chan].fir_config_variables, 0, sizeof(fir_config_struct_t));
      }
   }
}

capi_err_t capi_fir_get_filter_crossfade_cfg_v2(capi_fir_t *me_ptr,
                                                capi_buf_t *params_ptr,
                                                uint32_t    param_id,
                                                uint32_t    miid)
{
   capi_err_t capi_result = CAPI_EOK;
   if (NULL != me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg)
   {
      if (params_ptr->max_data_len >= me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg_size)
      {
         params_ptr->actual_data_len = 0;
         param_id_fir_filter_crossfade_cfg_v2_t *pfir_crossfade_cfg =
            (param_id_fir_filter_crossfade_cfg_v2_t *)(params_ptr->data_ptr);
         param_id_fir_filter_crossfade_cfg_v2_t *cached_crossfade_param =
            (param_id_fir_filter_crossfade_cfg_v2_t *)me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg;
         params_ptr->actual_data_len = memscpy(pfir_crossfade_cfg,
                                               params_ptr->max_data_len,
                                               cached_crossfade_param,
                                               me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg_size);
      }
      else
      {
         params_ptr->actual_data_len = me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg_size;
         FIR_MSG(miid,
                 DBG_ERROR_PRIO,
                 "Get Cross-fade param, Bad param size %lu, Req size: %lu",
                 params_ptr->max_data_len,
                 me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg_size);

         capi_result |= CAPI_ENEEDMORE;
      }
   }
   else
   {
      FIR_MSG(miid, DBG_ERROR_PRIO, "Did not receive any set param for parameter 0x%lx", param_id);
      capi_result |= CAPI_EFAILED;
      params_ptr->actual_data_len = 0;
   }
   return CAPI_EOK;
}

capi_err_t capi_fir_set_fir_filter_config_v2(capi_fir_t *me_ptr,
                                             uint32_t    param_id,
                                             uint32_t    param_size,
                                             int8_t *    data_ptr)
{
   capi_err_t capi_result = CAPI_EOK;
   if (param_size < sizeof(param_id_fir_filter_config_v2_t))
   {
      FIR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "configuration SetParam 0x%lx, invalid param size %lx ",
              param_id,
              param_size);
      capi_result = CAPI_ENEEDMORE;
      return capi_result;
   }
   // validate received payload size
   uint32_t req_payload_size          = 0;
   uint32_t base_payload_size         = FILTER_CFG_BASE_PAYLOAD_SIZE;
   uint32_t per_cfg_base_payload_size = FILTER_CFG_PER_CFG_BASE_PAYLOAD_SIZE;

   capi_result = capi_fir_validate_multichannel_v2_payload(me_ptr,
                                                           data_ptr,
                                                           param_id,
                                                           param_size,
                                                           &req_payload_size,
                                                           base_payload_size,
                                                           per_cfg_base_payload_size); //put comment

   if (CAPI_FAILED(capi_result))
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Fir config V2 SetParam 0x%lx failed", param_id);
      return capi_result;
   }
   //  caching the configuration received
   int8_t *source_ptr = data_ptr;

   // if current cfg pointer is null : rcvd cfg for first time
   // OR lib not created OR crossfade is disabled for all channels then store in 1st config only
   if ((NULL == me_ptr->capi_fir_v2_cfg.cache_original_fir_coeff_cfg) || (NULL == me_ptr->fir_channel_lib) ||
       (FALSE == me_ptr->capi_fir_v2_cfg.xfade_flag))
   {
      me_ptr->capi_fir_v2_cfg.config_type = CAPI_CURR_CFG; // do this check create lib instance
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Updating the current filter config with the new configuration");

      capi_result = capi_fir_cache_filter_coeff_payload_v2(me_ptr, source_ptr);
      if (CAPI_EBADPARAM == capi_result)
      {
         return capi_result;
      }

      if (CAPI_ENOMEMORY == capi_result)
      {
         capi_fir_clean_up_memory(me_ptr);
         return capi_result;
      }

      me_ptr->capi_fir_v2_cfg.size_req_for_get_coeff = param_size;
   }
   // if current cfg pointer is valid and new config pointer is Null : rcvd 2nd config
   // There is no active cross-fade at this point
   else if (NULL == me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg)
   {
      me_ptr->capi_fir_v2_cfg.config_type = CAPI_NEXT_CFG;
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Caching the configuration in the NEXT filter config ");

      capi_result = capi_fir_cache_filter_coeff_payload_v2(me_ptr, source_ptr);
      if (CAPI_EBADPARAM == capi_result)
      {
         return capi_result;
      }

      if (CAPI_ENOMEMORY == capi_result)
      {
         capi_fir_clean_up_memory(me_ptr);          // clearing the entire lib memory
         capi_fir_update_release_config_v2(me_ptr); // clean up the CURR config
         return capi_result;
      }
      me_ptr->capi_fir_v2_cfg.size_req_for_get_next_coeff = param_size;
   }
   else // if the configurations comes during an active configuration, we need to queue the config
   {
      me_ptr->capi_fir_v2_cfg.config_type = CAPI_QUEUE_CFG;
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Caching the configuration in the QUEUE filter config");

      capi_result = capi_fir_cache_filter_coeff_payload_v2(me_ptr, source_ptr);

      if (CAPI_EBADPARAM == capi_result)
      {
         return capi_result;
      }

      if (CAPI_ENOMEMORY == capi_result)
      {
         capi_fir_clean_up_memory(me_ptr);
         capi_fir_update_release_config_v2(me_ptr); //
         capi_fir_update_release_config_v2(me_ptr); // clean up CURR & NEXT config
         return capi_result;
      }
      // set param will happen from process if all ch's crossfade is completed
      me_ptr->capi_fir_v2_cfg.size_req_for_get_queue_coeff = param_size;
   }

   // set param if library is created
   if ((NULL != me_ptr->fir_channel_lib) && (me_ptr->capi_fir_v2_cfg.config_type != CAPI_QUEUE_CFG))
   {

      capi_result = capi_fir_set_cached_filter_coeff_v2(me_ptr);
      if (CAPI_FAILED(capi_result))
      {
         return capi_result;
      }
      else
      {
         capi_fir_raise_events(me_ptr);
      }
   }

   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Set/cache param of filter coeffs is set Successfully ");
   return CAPI_EOK;
}
/*===============================================================
  Function name: capi_fir_cache_filter_coeff_payload_v2
  Description : Function to cache filter coefficients payload.
  ===============================================================*/
capi_err_t capi_fir_cache_filter_coeff_payload_v2(capi_fir_t *me_ptr, int8_t *const payload_ptr)
{
   int8_t **                         fir_cfg_ptr      = NULL;
   param_id_fir_filter_config_v2_t **cached_coeff_ptr = NULL;
   uint32_t *                        cfg_size         = NULL;
   param_id_fir_filter_config_v2_t * fir_cfg_state    = (param_id_fir_filter_config_v2_t *)payload_ptr;
   int8_t *                          temp_fir_cfg     = payload_ptr + sizeof(param_id_fir_filter_config_v2_t);
   fir_filter_cfg_v2_t *const        fir_coeff_cfg    = (fir_filter_cfg_v2_t *)temp_fir_cfg;
   capi_err_t                        capi_result      = CAPI_EOK;
   const uint32_t                    num_cfg          = fir_cfg_state->num_config;

   if (CAPI_CURR_CFG == me_ptr->capi_fir_v2_cfg.config_type)
   {
      fir_cfg_ptr      = &me_ptr->capi_fir_v2_cfg.cache_original_fir_coeff_cfg;
      cached_coeff_ptr = &me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg;
      cfg_size         = &me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg_size;
   }
   else if (CAPI_NEXT_CFG == me_ptr->capi_fir_v2_cfg.config_type)
   {
      fir_cfg_ptr      = &me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg;
      cached_coeff_ptr = &me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg;
      cfg_size         = &me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg_size;
   }
   else if (CAPI_QUEUE_CFG == me_ptr->capi_fir_v2_cfg.config_type)
   {
      fir_cfg_ptr      = &me_ptr->capi_fir_v2_cfg.cache_original_queue_fir_coeff_cfg;
      cached_coeff_ptr = &me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg;
      cfg_size         = &me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg_size;
   }
   else
   {
      FIR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "Did not receive config type while caching filter coeff. Received %lu",
              me_ptr->capi_fir_v2_cfg.config_type);
      return CAPI_EUNSUPPORTED;
   }
   // calculate memory_required to cache
   uint32_t required_cache_size = capi_fir_calculate_cache_size_for_coeff_v2(me_ptr->miid, fir_coeff_cfg, num_cfg);

   if (0 == required_cache_size)
   {
      return CAPI_EBADPARAM;
   }
   // allocate memory required
   if (required_cache_size != *cfg_size)
   {
      if (NULL != *fir_cfg_ptr)
      {
         posal_memory_aligned_free(*fir_cfg_ptr);
         *fir_cfg_ptr      = NULL;
         *cached_coeff_ptr = NULL;
         *cfg_size         = 0;
      }
      int8_t *temp_ptr;
      temp_ptr =
         (int8_t *)posal_memory_aligned_malloc(required_cache_size + 7,
                                               ALIGN_8_BYTE,
                                               (POSAL_HEAP_ID)me_ptr->heap_info.heap_id); // requires 8 byte alignment

      if (NULL == temp_ptr)
      {
         FIR_MSG(me_ptr->miid,
                 DBG_HIGH_PRIO,
                 "No memory to cache the filter coefficients cfg of %lu bytes",
                 required_cache_size);
         return CAPI_ENOMEMORY;
      }
      *fir_cfg_ptr      = temp_ptr;
      *cached_coeff_ptr = (param_id_fir_filter_config_v2_t *)temp_ptr;
      *cfg_size         = required_cache_size;

   }
   capi_result |= capi_fir_copy_fir_coeff_cfg_v2(me_ptr, payload_ptr);
   if (CAPI_FAILED(capi_result))
   {
      return capi_result;
   }
   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Cached filter coeff config of %lu bytes", *cfg_size);

   me_ptr->cfg_version = VERSION_V2; // indicates V2 version is set now and can only allow V2 version
                                      // configs for upcoming set params
   return CAPI_EOK;
}

/*===============================================================================
  Function name: capi_fir_calculate_cache_size_for_coeff_v2
  Description : Function to calculate size required to cache filter coefficients.
  ===============================================================================*/
uint32_t capi_fir_calculate_cache_size_for_coeff_v2(uint32_t             miid,
                                                    fir_filter_cfg_v2_t *filter_coeff_cfg_ptr,
                                                    uint32_t             num_cfg)
{
   uint32_t  required_size   = CAPI_FIR_ALIGN_8_BYTE(sizeof(param_id_fir_filter_config_v2_t));
   int8_t *  temp_ptr        = (int8_t *)filter_coeff_cfg_ptr;
   uint32_t *ch_mask_arr_ptr = NULL;
   uint32_t  alignment_bytes = 0;
   uint32_t  offset          = 0;
   for (uint32_t count = 0; count < num_cfg; count++)
   {
      uint32_t ch_type_group_mask = *((uint32_t *)temp_ptr);
      ch_mask_arr_ptr             = (uint32_t *)(temp_ptr + CAPI_CMN_INT32_SIZE_IN_BYTES); // points to mask array
      uint32_t ch_mask_list_size  = capi_cmn_count_set_bits(ch_type_group_mask);

      capi_fir_filter_cfg_static_params *temp_ptr_1 = (capi_fir_filter_cfg_static_params*)(ch_mask_arr_ptr + ch_mask_list_size);

      uint32_t coeff_width        = temp_ptr_1->coef_width >> 3; // converting to bytes;
      uint16_t num_taps           = temp_ptr_1->num_taps;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(miid, DBG_MED_PRIO,"ch_mask_list_size %lu, coeff_width %lu, num_taps %lu"
              "Channel group mask %#lx received.",
               ch_mask_list_size,
			   coeff_width,
			   num_taps,
			   ch_type_group_mask);
#endif
      // uint32_t coeff_width                   = (temp_coeff_cfg_ptr->coef_width) >> 3; //converting to bytes
      if ((sizeof(uint16_t) != coeff_width) && (sizeof(uint32_t) != coeff_width))
      {
         FIR_MSG(miid, DBG_HIGH_PRIO, "Received incorrect bit width of %lu for coefficients ", coeff_width * 8);
         return 0;
      }

         if (ch_mask_list_size & 0x1)
         { // filters taps to multiple of 4 and one extra 4 bytes(if ch_mask_list size is odd) to make
           // coefficients 8 byte align
            alignment_bytes = ((num_taps % 4) ? (4 - (num_taps % 4)) * (coeff_width) : 0) + 4;
         }
         else
         {
            alignment_bytes = ((num_taps % 4) ? (4 - (num_taps % 4)) * (coeff_width) : 0);
         }
      offset = ((capi_cmn_multi_ch_per_config_increment_size(ch_type_group_mask, sizeof(fir_filter_cfg_v2_t))) +
                (coeff_width * (num_taps)) + alignment_bytes);
      required_size += offset;
      temp_ptr += (capi_cmn_multi_ch_per_config_increment_size(ch_type_group_mask, sizeof(fir_filter_cfg_v2_t)) + (CAPI_CMN_INT32_SIZE_IN_BYTES * num_taps));
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(miid,
              DBG_ERROR_PRIO,
              "calculated size for config %lu is %lu, incr_offset = %lu",
              count,
              required_size,
              offset);
#endif
   }
   return required_size;
}

/*========================================================================
  Function name: capi_fir_copy_fir_coeff_cfg_v2
  Description : Function to cache fir coefficients cfg.
  ========================================================================*/
capi_err_t capi_fir_copy_fir_coeff_cfg_v2(capi_fir_t *me_ptr, int8_t *payload_ptr)
{
   //uint32_t *temp_cfg_ptr     = NULL;
   int8_t *  ptr_cached_coeff = NULL;
   uint32_t  destination_size = 0;
   if (CAPI_CURR_CFG == me_ptr->capi_fir_v2_cfg.config_type)
   {
      ptr_cached_coeff = (int8_t *)me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg;
      destination_size = me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg_size;
   }
   else if (CAPI_NEXT_CFG == me_ptr->capi_fir_v2_cfg.config_type)
   {
      ptr_cached_coeff = (int8_t *)me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg;
      destination_size = me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg_size;
   }
   else if (CAPI_QUEUE_CFG == me_ptr->capi_fir_v2_cfg.config_type)
   {
      ptr_cached_coeff = (int8_t *)me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg;
      destination_size = me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg_size;
   }
   else
   {
      FIR_MSG(me_ptr->miid,
              DBG_HIGH_PRIO,
              "Did not receive expected config type while copying coeff config. Recieved %lu",
              me_ptr->capi_fir_v2_cfg.config_type);
      return CAPI_EUNSUPPORTED;
   }

   const uint32_t num_cfg     = ((param_id_fir_filter_config_v2_t *)payload_ptr)->num_config;
   uint32_t       copied_size = 0;
   // Copy param_id_fir_filter_config_v2_t
   uint32_t size = sizeof(param_id_fir_filter_config_v2_t);
   copied_size   = memscpy(ptr_cached_coeff, destination_size, payload_ptr, size);
   destination_size -= copied_size;

   ptr_cached_coeff += CAPI_FIR_ALIGN_8_BYTE(size);
   payload_ptr += size;

   int32_t i = 0;

   for (uint16_t count = 0; count < num_cfg; count++)
   {
      fir_filter_cfg_t *fir_coef_cfg = (fir_filter_cfg_t *)payload_ptr;
      // Copy fir_filter_cfg_v2_t
      uint32_t channel_group_mask = *((uint32_t *)payload_ptr);
      uint32_t *ch_mask_list_ptr  = (uint32_t *)(payload_ptr + CAPI_CMN_INT32_SIZE_IN_BYTES); // points to mask array
      uint32_t ch_mask_list_size  = capi_cmn_count_set_bits(channel_group_mask);

      capi_fir_filter_cfg_static_params *temp_ptr_1 = (capi_fir_filter_cfg_static_params*)(ch_mask_list_ptr + ch_mask_list_size);

      uint32_t coef_width = temp_ptr_1->coef_width;
      uint16_t num_taps   = temp_ptr_1->num_taps;
#ifdef CAPI_FIR_DEBUG_MSG
      FIR_MSG(me_ptr->miid,
              DBG_HIGH_PRIO,
              "channel_group_mask: %lu ,num_taps: %lu, coeff_width: %lu",
              channel_group_mask,
              num_taps,
              coef_width);
#endif
      size = capi_cmn_multi_ch_per_config_increment_size(channel_group_mask, sizeof(fir_filter_cfg_v2_t));

      copied_size = memscpy(ptr_cached_coeff, destination_size, payload_ptr, size);
      destination_size -= copied_size;

      ptr_cached_coeff += size;
      payload_ptr += size; // pointing to filter coeff

      // Copy Coefficient
      if (16 == coef_width)
      {
         int16_t *dest_ptr   = (int16_t *)ptr_cached_coeff;
         int32_t *src_ptr    = (int32_t *)payload_ptr;

         uint32_t alignBytes = 0;

         if (ch_mask_list_size & 1) // if size of channel mask list size is odd, we need to append 4 B of alignment bytes
         {                          // to make filter coefficients 8 bytes aligned
            alignBytes = 4;
            for (i = 2; i > 0; i--)
            {
               *dest_ptr++ = 0;
            }
            destination_size -= (sizeof(int16_t) * 2);
         }
         ptr_cached_coeff +=
            (num_taps * sizeof(uint16_t)) + alignBytes; 
#ifdef QDSP6_ASM_OPT_FIR_FILTER
		 ////reversing the coefficients and storing in provided memory
         dest_ptr = (int16_t *)ptr_cached_coeff;
         dest_ptr--;
#endif
         for (uint16_t tap_count = 0; tap_count < num_taps; tap_count++)
         {
            *dest_ptr = (int16_t)(*src_ptr);

#ifdef QDSP6_ASM_OPT_FIR_FILTER
            dest_ptr--;
#else
            dest_ptr++;
#endif
            src_ptr++;
         }
         destination_size -= num_taps * sizeof(int16_t);

         dest_ptr = (int16_t *)ptr_cached_coeff;

         for (i = ((num_taps % 4) ? (4 - (num_taps % 4)) : 0); i > 0; i--)
         {
            *dest_ptr++ = 0; // zero padding the extra coefficients to make filter taps as multiple of 4
            destination_size -= 2;
         }

         ptr_cached_coeff = (int8_t *)dest_ptr;
         payload_ptr      = (int8_t *)src_ptr;

      }
      else
      {
#ifdef QDSP6_ASM_OPT_FIR_FILTER
         int32_t *dest_ptr   = (int32_t *)ptr_cached_coeff;
         int32_t *src_temp   = (int32_t *)payload_ptr;
         uint32_t alignBytes = 0;
         if (ch_mask_list_size & 1) // if size of channel mask list is odd, we need to append 4 B of alignment bytes
         {                          // to make filter coefficients 8 bytes aligned
            *dest_ptr++ = 0;
            alignBytes  = 4;
         }
         size = num_taps * sizeof(int32_t);
         ptr_cached_coeff += size + alignBytes; // TBR amith
         dest_ptr = (int32_t *)ptr_cached_coeff;
         dest_ptr--;
         for (uint16_t tap_count = 0; tap_count < num_taps; tap_count++)
         {
            *dest_ptr = (*src_temp); // reversing the coefficients and storing in provided memory
            dest_ptr--;
            src_temp++;
         }
         destination_size -= (size + alignBytes); // TBR Amith

         dest_ptr = (int32_t *)ptr_cached_coeff;
         for (i = ((num_taps % 4) ? (4 - (num_taps % 4)) : 0); i > 0; i--)
         {
            *dest_ptr++ = 0; // zero padding the extra coefficients to make filter taps as multiple of 4
            destination_size -= 4;
         }
         ptr_cached_coeff = (int8_t *)dest_ptr;
         payload_ptr      = (int8_t *)src_temp;
#else
         int32_t *dest_ptr = (int32_t *)ptr_cached_coeff;
         uint32_t alignBytes = 0;

         if (ch_mask_list_size & 1) // if size of channel mask list is odd, we need to append 4 B of alignment bytes
         {                          // to make filter coefficients 8 bytes aligned
            *dest_ptr++      = 0;
            alignBytes       = 4;
            destination_size -= alignBytes;
            ptr_cached_coeff += alignBytes;
         }
         size        = num_taps * CAPI_CMN_INT32_SIZE_IN_BYTES;
         copied_size = memscpy(ptr_cached_coeff, destination_size, payload_ptr, size);
         destination_size -= copied_size;
         ptr_cached_coeff += size;
         payload_ptr += size;
		 
         for (i = ((fir_coef_cfg->num_taps % 4) ? (4 - (fir_coef_cfg->num_taps % 4)) : 0); i > 0; i--)
         {
            *dest_ptr++ = 0; // zero padding the extra coefficients to make filter taps as multiple of 4
            destination_size -= 4;
         }
         ptr_cached_coeff = (int8_t *)dest_ptr;
#endif
      }
   }
   return CAPI_EOK;
}

/*===========================================================================
  Function : capi_fir_set_cached_filter_coeff
  Description : Function to set filter config params using the cached params
 ============================================================================*/
capi_err_t capi_fir_set_cached_filter_coeff_v2(capi_fir_t *me_ptr)
{
   param_id_fir_filter_config_v2_t *fir_cfg_state;
   int8_t *                         fir_cfg_ptr;
   if (CAPI_CURR_CFG == me_ptr->capi_fir_v2_cfg.config_type)
   {
      fir_cfg_state = me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg;
      fir_cfg_ptr   = (int8_t *)me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg;
   }
   else if (CAPI_NEXT_CFG == me_ptr->capi_fir_v2_cfg.config_type)
   {
      fir_cfg_state = me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg;
      fir_cfg_ptr   = (int8_t *)me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg;
   }
   else if (CAPI_QUEUE_CFG == me_ptr->capi_fir_v2_cfg.config_type)
   {
      fir_cfg_state = me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg;
      fir_cfg_ptr   = (int8_t *)me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg;
   }
   else
   {
      FIR_MSG(me_ptr->miid,
              DBG_HIGH_PRIO,
              "Did not receive expected config type while set filter config params using the cached "
              "params. "
              "Recieved %lu",
              me_ptr->capi_fir_v2_cfg.config_type);
      return CAPI_EFAILED;
   }

   if (NULL == fir_cfg_state)
   {
      return CAPI_EOK;
   }

   uint32_t num_cfg = fir_cfg_state->num_config;
   fir_cfg_ptr +=
      CAPI_FIR_ALIGN_8_BYTE(sizeof(param_id_fir_filter_config_v2_t));       // this is aligned to 8 because we want the
                                                                            // coefficients always at 8 byte boundary.
   fir_filter_cfg_v2_t *fir_coeff_cfg = (fir_filter_cfg_v2_t *)fir_cfg_ptr; // base payload1

   capi_fir_channel_lib_t *       fir_channel_lib       = me_ptr->fir_channel_lib;
   fir_filter_cfg_v2_t *const     coeff_cfg_ptr         = fir_coeff_cfg; // base payload1
   fir_filter_cfg_v2_t *          temp_coeff_cfg_ptr    = fir_coeff_cfg; // base payload1
   uint32_t *                     ch_mask_arr_ptr       = NULL;
   const uint32_t                 channels_allocated    = me_ptr->input_media_fmt[0].format.num_channels;
   FIR_RESULT                     set_param_result      = FIR_SUCCESS;
   uint32_t                       channel_type          = 0;
   int8_t *                       data_ptr              = (int8_t *)temp_coeff_cfg_ptr; // base payload1
   fir_transition_status_struct_t fir_transition_status = { 0, 0, 0 };
   uint32_t                       filled_struct_size    = 0;
   FIR_RESULT                     lib_result;
   uint32_t                       is_in_transition = 0; // introducing this to identify if by applying new filter config
                                  // cross-fade happened or not so that we can free the prev config
   for (uint32_t chan_num = 0; chan_num < channels_allocated; chan_num++)
   {
      uint32_t offset    = 0;
      temp_coeff_cfg_ptr = coeff_cfg_ptr;                // base payload1
      data_ptr           = (int8_t *)temp_coeff_cfg_ptr; // base payload1
      channel_type       = me_ptr->input_media_fmt[0].channel_type[chan_num];
      for (uint32_t count = 0; count < num_cfg; count++)
      {
         uint32_t channel_group_mask = 0;
         uint32_t ch_mask_list_size  = 0;
         // Increment data ptr by calculated offset to point at next payload's group_mask
         data_ptr += offset;
         //fir_filter_cfg_v2_t *temp_cfg_ptr = (fir_filter_cfg_v2_t *)data_ptr; //payload #count
         channel_group_mask                  = *((uint32_t *)data_ptr);
         ch_mask_arr_ptr                     = (uint32_t *)(data_ptr + CAPI_CMN_INT32_SIZE_IN_BYTES); // points to mask array
         ch_mask_list_size                   = capi_cmn_count_set_bits(channel_group_mask);
         capi_fir_filter_cfg_static_params *temp_ptr = (capi_fir_filter_cfg_static_params*)(ch_mask_arr_ptr + ch_mask_list_size);
         uint32_t coef_width                 = temp_ptr->coef_width;
         uint16_t coefQFactor                = temp_ptr->coef_q_factor;
         uint16_t num_taps                   = temp_ptr->num_taps;

#ifdef CAPI_FIR_DEBUG_MSG
         FIR_MSG(me_ptr->miid,
                 DBG_HIGH_PRIO,
                 "chan: %lu, channel_type:%lu, channel_group_mask: %lu ,num_taps: %lu, coeff_width: %lu, "
                 "coefQFactor:%lu",
                 chan_num,
                 channel_type,
                 channel_group_mask,
                 num_taps,
                 coef_width,
                 coefQFactor);
#endif
         uint32_t curr_ch_mask    = (1 << CAPI_CMN_MOD_WITH_32(channel_type));
         uint32_t ch_index_grp_no = CAPI_CMN_DIVIDE_WITH_32(channel_type);
         // check if a group is configured. If yes, get its set_index in the actual cached payload
         if (CAPI_CMN_IS_BIT_SET_AT_POS_IN_32B_VAL(channel_group_mask, ch_index_grp_no))
         {
            uint32_t set_index = capi_cmn_count_set_bits_in_lower_n_bits(channel_group_mask, ch_index_grp_no);
            if (curr_ch_mask & ch_mask_arr_ptr[set_index]) // check if this chan_type is set in the payload
            {
               if (fir_channel_lib[chan_num].fir_static_variables.max_num_taps < num_taps)
               {
                  FIR_MSG(me_ptr->miid,
                          DBG_HIGH_PRIO,
                          "Received invalid tap length of %u for chan - %lu , Max value set - %lu",
                          num_taps,
                          chan_num,
                          fir_channel_lib[chan_num].fir_static_variables.max_num_taps);
                  return CAPI_EBADPARAM;
               }
               else
               {
                  fir_channel_lib[chan_num].fir_config_variables.coef_width  = coef_width;
                  fir_channel_lib[chan_num].fir_config_variables.coefQFactor = coefQFactor;
                  fir_channel_lib[chan_num].fir_config_variables.num_taps    = num_taps;
                  fir_channel_lib[chan_num].fir_config_variables.coeffs_ptr  = (uint64)(ch_mask_arr_ptr + ch_mask_list_size
                		   + (sizeof(capi_fir_filter_cfg_static_params) >> 2)); // incrementing the offset to point to the starting address of the filter coeff

                  // Incrementing by 4 bytes because coefficients are stored in internal cached structure after
                  // 4 bytes(if the channel mask list size is not a multiple of 2) to make it 8 bytes align
                  uint32_t alignBytes = (ch_mask_list_size & 1) ? 4 : 0;
                  fir_channel_lib[chan_num].fir_config_variables.coeffs_ptr += alignBytes;

                  if (NULL != fir_channel_lib[chan_num].fir_lib_instance.lib_mem_ptr)
                  {
                     set_param_result = fir_set_param(&(fir_channel_lib[chan_num].fir_lib_instance),
                                                      FIR_PARAM_CONFIG,
                                                      (int8_t *)&fir_channel_lib[chan_num].fir_config_variables,
                                                      sizeof(fir_config_struct_t));

                     if (FIR_SUCCESS != set_param_result)
                     {
                        FIR_MSG(me_ptr->miid,
                                DBG_ERROR_PRIO,
                                "Filter coeff Set param failed for channel %lu with lib error %u",
                                chan_num,
                                set_param_result);
                        return CAPI_EFAILED;
                     }

                     // getting transition status to store info in capi so that we can: 1. check in process(), 2. free
                     // prev config
                     lib_result = fir_get_param(&fir_channel_lib[chan_num].fir_lib_instance,
                                                FIR_PARAM_GET_TRANSITION_STATUS,
                                                (int8_t *)(&fir_transition_status),
                                                sizeof(fir_transition_status_struct_t),
                                                &filled_struct_size);
                     if ((FIR_SUCCESS != lib_result))
                     {
                        FIR_MSG(me_ptr->miid,
                                DBG_ERROR_PRIO,
                                "Failed while getting transition status in set cached filter config");
                        return CAPI_EFAILED;
                     }
                     else
                     {
                        fir_channel_lib[chan_num].fir_transition_status_variables.flag = fir_transition_status.flag;
                        fir_channel_lib[chan_num].fir_transition_status_variables.coeffs_ptr =
                           fir_transition_status.coeffs_ptr;
                        FIR_MSG(me_ptr->miid,
                                DBG_LOW_PRIO,
                                "Crossfade status flag for chan %lu after set filter config param: %lu",
                                chan_num,
                                fir_transition_status.flag);
                        is_in_transition |= fir_transition_status.flag; // is_in_transition will be set to 1 if any of
                                                                        // the ch is in cross-fade
                     }
                  }
               }
               break;
            }
         }
         offset = capi_fir_coeff_config_cache_increment_size_v2(ch_mask_list_size, coef_width, num_taps);
#ifdef CAPI_FIR_DEBUG_MSG
         FIR_MSG(me_ptr->miid,
                 DBG_HIGH_PRIO,
                 "Increment offset: %lu",
                 offset);
#endif

         // Incrementing by 4 bytes because coefficients are stored in internal cached structure after
         // 4 bytes(if the channel mask list size is not a multiple of 2) to make it 8 bytes align
         uint32_t alignBytes = (ch_mask_list_size & 1) ? 4 : 0;
         offset += alignBytes;
      }
   }
   me_ptr->capi_fir_v2_cfg.combined_crossfade_status = is_in_transition; // this is updated in process when crossfade is completed
   if (!is_in_transition)                                // no ch is in transition after applying filter config
   {
      capi_fir_update_release_config_v2(me_ptr); // update the new config and release prev config
                                                 // this would ensure if crossfade not recieved previously for this
                                                 // channel, filter config would store in 1 config only
   }
   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Filter coeff v2 are set");
   return CAPI_EOK;
}

uint32_t capi_fir_coeff_config_cache_increment_size_v2(uint32_t ch_mask_list_size,
                                                       uint32_t coef_width,
                                                       uint16_t num_taps)
{
   uint16_t no_of_bytes = coef_width >> 3;
   uint16_t alignment_bytes;
   alignment_bytes =
      (num_taps % 4)
         ? (4 - (num_taps % 4)) * (no_of_bytes)
         : 0; // To make filter taps multiple of 4,so that the next set of coefficients are also aligned to 8
   return (sizeof(fir_filter_cfg_v2_t) + (ch_mask_list_size << 2) + (no_of_bytes * num_taps) + alignment_bytes);
}

void capi_fir_process_set_pending_crossfade_config(capi_fir_t *me_ptr, uint32_t is_crossfade_flag_updated)
{
   if (VERSION_V1 == me_ptr->cfg_version)
   {
      capi_fir_set_pending_xfade_cfg(me_ptr, is_crossfade_flag_updated);
   }
   else
   {
      capi_fir_set_pending_xfade_v2_cfg(me_ptr, is_crossfade_flag_updated);
   }
}

void capi_fir_set_pending_xfade_cfg(capi_fir_t *me_ptr, uint32_t is_crossfade_flag_updated)
{
   capi_err_t capi_result = CAPI_EOK;
   // combined_crossfade_status = FALSE implies none of the channel is in transition
   // is_crossfade_flag_updated set: successfully got transition status (basically entered the condition to check)
   if (!me_ptr->combined_crossfade_status && is_crossfade_flag_updated)
   {
   // setting new crossfade configuration after crossfade completes
   // if there is an active crossfade going on, change in xfade config can lead to glitch
   if (me_ptr->is_xfade_cfg_pending)
   {
      capi_result = capi_fir_set_cached_filter_crossfade_config(me_ptr);
      if (CAPI_EOK != capi_result)
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed to set pending filter cross-fade param in process()");
      }
   }
   if (me_ptr->cache_original_queue_fir_coeff_cfg) // set if any filter config is pending
   {
      me_ptr->config_type = CAPI_QUEUE_CFG;
      capi_result |= capi_fir_set_cached_filter_coeff(me_ptr);
      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed to set pending filter coeff param in process()");
         capi_fir_clean_up_cached_coeff_memory(me_ptr); // deleting configs and library
      }
   }
   capi_fir_update_release_config(me_ptr); // update the new config and release prev config
   capi_fir_raise_events(me_ptr);          // re-calculate votes everytime crossfade is completed
   }
}

void capi_fir_set_pending_xfade_v2_cfg(capi_fir_t *me_ptr, uint32_t is_crossfade_flag_updated)
{
   capi_err_t capi_result = CAPI_EOK;
   // combined_crossfade_status = FALSE implies none of the channel is in transition
   // is_crossfade_flag_updated set: successfully got transition status (basically entered the condition to check)
   if (!me_ptr->capi_fir_v2_cfg.combined_crossfade_status && is_crossfade_flag_updated)
   {
   // setting new crossfade configuration after crossfade completes
   // if there is an active crossfade going on, change in xfade config can lead to glitch
   if (me_ptr->capi_fir_v2_cfg.is_xfade_cfg_pending)
   {
      capi_result = capi_fir_set_cached_filter_crossfade_config_v2(me_ptr);
      if (CAPI_EOK != capi_result)
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed to set pending filter cross-fade param in process()");
      }
   }
   if (me_ptr->capi_fir_v2_cfg.cache_original_queue_fir_coeff_cfg) // set if any filter config is pending
   {
      me_ptr->capi_fir_v2_cfg.config_type = CAPI_QUEUE_CFG;
      capi_result |= capi_fir_set_cached_filter_coeff_v2(me_ptr);
      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed to set pending filter coeff param in process()");
         capi_fir_clean_up_cached_coeff_memory_v2(me_ptr); // deleting configs and library
      }
   }
   capi_fir_update_release_config_v2(me_ptr); // update the new config and release prev config
   capi_fir_raise_events(me_ptr);             // re-calculate votes everytime crossfade is completed
   }
}

capi_err_t capi_fir_get_filter_cfg_v2(capi_fir_t *me_ptr, capi_buf_t *params_ptr, uint32_t param_id, uint32_t miid)
{
   param_id_fir_filter_config_v2_t *cached_coeff_ptr = NULL;
   int8_t *                         fir_cfg_ptr      = NULL;
   uint32_t                         get_coeff_size   = 0;
   capi_err_t                       capi_result      = CAPI_EOK;
   // if 2 or more configs are present then return next cfg bcs crossfading going on from cfg1 to cfg2
   if (NULL != me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg)
   {
      FIR_MSG(miid, DBG_LOW_PRIO, "active crossfade going on, return next config");
      get_coeff_size   = me_ptr->capi_fir_v2_cfg.size_req_for_get_next_coeff;
      cached_coeff_ptr = (param_id_fir_filter_config_v2_t *)me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg;
      fir_cfg_ptr      = (int8_t *)me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg;
   }
   else if (NULL != me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg) // only 1 config present
   {
      FIR_MSG(miid, DBG_LOW_PRIO, "no active crossfade, return curr config");
      get_coeff_size = me_ptr->capi_fir_v2_cfg.size_req_for_get_coeff;
      cached_coeff_ptr =
         (param_id_fir_filter_config_v2_t *)me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg;
      fir_cfg_ptr = (int8_t *)me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg;
   }
   else
   {
      FIR_MSG(miid, DBG_ERROR_PRIO, "Did not receive any set param for parameter 0x%lx", param_id);
      params_ptr->actual_data_len = 0;
      return CAPI_EFAILED;
   }

   if (params_ptr->max_data_len >= get_coeff_size)
   {
      params_ptr->actual_data_len = 0;
      fir_cfg_ptr +=
         CAPI_FIR_ALIGN_8_BYTE(sizeof(param_id_fir_filter_config_v2_t)); // this is done to ensure the coefficients
                                                                         // are always at 8 byte alignment.

      uint32_t size_to_copy = 0;

      uint32_t increment_size_of_ptr;
      uint32_t total_size_copied = 0;
      int8_t * dest_ptr          = params_ptr->data_ptr;
      uint32_t dest_max_size     = params_ptr->max_data_len;

      size_to_copy = sizeof(param_id_fir_filter_config_v2_t);
      total_size_copied += memscpy(dest_ptr, dest_max_size, cached_coeff_ptr, size_to_copy);
      dest_ptr += total_size_copied;
      dest_max_size -= total_size_copied;
      fir_filter_cfg_v2_t *temp_coeff_cfg_ptr = (fir_filter_cfg_v2_t *)fir_cfg_ptr;
      int8_t *             temp_increment_ptr = (int8_t *)temp_coeff_cfg_ptr;

      for (uint32_t count = 0; count < cached_coeff_ptr->num_config; count++)
      {
         uint32_t ch_mask_list_size          = capi_cmn_count_set_bits(temp_coeff_cfg_ptr->channel_type_group_mask);
         uint32_t ch_mask_list_size_in_bytes = ch_mask_list_size << 2;

         // incrementing offset to point to the address next to channel_type_mask_list
         capi_fir_filter_cfg_static_params *temp_ptr = (capi_fir_filter_cfg_static_params*)
        		                            (temp_increment_ptr + CAPI_CMN_INT32_SIZE_IN_BYTES + ch_mask_list_size_in_bytes);
         uint32_t coef_width = temp_ptr->coef_width;
         uint16_t num_taps   = temp_ptr->num_taps;
#ifdef CAPI_FIR_DEBUG_MSG
         FIR_MSG(me_ptr->miid,
                 DBG_HIGH_PRIO,
                 "channel_group_mask: %lu ,num_taps: %lu, coeff_width: %lu",
                 temp_coeff_cfg_ptr->channel_type_group_mask,
                 num_taps,
                 coef_width);
#endif
         increment_size_of_ptr = 0;
         if (16 == coef_width)
         {
            size_to_copy = capi_fir_coefficient_convert_to_32bit_v2(me_ptr,
                                                                    (int8_t *)temp_coeff_cfg_ptr,
                                                                    num_taps,
                                                                    dest_ptr,
                                                                    dest_max_size);
            total_size_copied += size_to_copy;
            increment_size_of_ptr =
               (num_taps * sizeof(int16_t)) + (ch_mask_list_size << 2) + sizeof(fir_filter_cfg_v2_t);
#ifdef CAPI_FIR_DEBUG_MSG
            FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "increment_size_of_ptr: %lu", increment_size_of_ptr);
#endif
            if (num_taps % 4)
            {
               increment_size_of_ptr += sizeof(int16_t) * (4 - (num_taps % 4));
            }
            // Incrementing by 4 bytes because coefficients are stored in internal cached structure after
            // 4 bytes(if the channel mask list size is not a multiple of 2) to make it 8 bytes align
            uint32_t alignBytes = (ch_mask_list_size & 1) ? 4 : 0;
            increment_size_of_ptr += alignBytes;

            dest_ptr += size_to_copy; //does not include alignment
            dest_max_size -= size_to_copy;
            temp_increment_ptr += increment_size_of_ptr; //includes alignment
            temp_coeff_cfg_ptr = (fir_filter_cfg_v2_t *)temp_increment_ptr;
         }
         else
         {

            size_to_copy      = sizeof(fir_filter_cfg_v2_t) + (ch_mask_list_size << 2);
            total_size_copied += memscpy(dest_ptr, dest_max_size, temp_coeff_cfg_ptr, size_to_copy);
            dest_ptr      += size_to_copy;
		    dest_max_size -= size_to_copy;
            temp_increment_ptr += size_to_copy;
            FIR_MSG(miid, DBG_HIGH_PRIO,"shreya:total_size_copied %lu, size_to_copy %lu",total_size_copied, size_to_copy);

		    uint32_t alignBytes = (ch_mask_list_size & 1) ? 4 : 0;
            FIR_MSG(miid, DBG_HIGH_PRIO,"shreya:alignBytes %lu",alignBytes);

            temp_increment_ptr += alignBytes; // this is done because we are storing coefficients in internal cached structure after
                                              // gap of 4 bytes to make them 8 byte aligned

            // copy the coefficients
            size_to_copy = sizeof(int32_t) * num_taps;
            //size_to_copy_1      = (sizeof(int32_t) * num_taps);
     
            //temp_increment_ptr += size_to_copy + size_to_copy_1 + alignBytes; // amith: TBR +4
#ifdef QDSP6_ASM_OPT_FIR_FILTER
            int32_t *src_temp = (int32_t *)(temp_increment_ptr + size_to_copy); // Because coefficients stored in reverse order
            int32_t *dest_temp = (int32_t *)dest_ptr;
            src_temp--;
            for (uint16_t tap_count = 0; tap_count < num_taps; tap_count++)
            {
               *dest_temp = (*src_temp); // reversing the coefficients and storing it in provided memory ,no.
                                         // of cofficients = original number of taps
               src_temp--;
               dest_temp++;
            }
            total_size_copied += size_to_copy;
#else
            total_size_copied += memscpy(dest_ptr, dest_max_size, temp_increment_ptr, size_to_copy);
#endif
            dest_ptr += size_to_copy;
            dest_max_size -= size_to_copy;
			temp_increment_ptr += size_to_copy;
            FIR_MSG(miid, DBG_HIGH_PRIO,"shreya: size_to_copy %lu",size_to_copy);
            if (num_taps % 4)
            {
               increment_size_of_ptr = sizeof(int32_t) * (4 - (num_taps % 4));
            }
            temp_increment_ptr += increment_size_of_ptr;
#ifdef CAPI_FIR_DEBUG_MSG
            FIR_MSG(miid, DBG_HIGH_PRIO,"increment_size_of_ptr %lu",increment_size_of_ptr);
#endif
            temp_coeff_cfg_ptr = (fir_filter_cfg_v2_t *)temp_increment_ptr;

         }
      }
      params_ptr->actual_data_len = total_size_copied;
   }
   else
   {
      params_ptr->actual_data_len = get_coeff_size;
      FIR_MSG(miid,
              DBG_ERROR_PRIO,
              "Get Filter coeff cfg, Bad param size %lu, Req size = %lu",
              params_ptr->max_data_len,
              get_coeff_size);
      capi_result |= CAPI_ENEEDMORE;
   }
   return CAPI_EOK;
}

/*==================================================================
  Function : capi_fir_coefficient_convert_to_32bit_v2
  Description : Function to convert filter coeff to 32 bit from 16bit
 ====================================================================*/
uint32_t capi_fir_coefficient_convert_to_32bit_v2(capi_fir_t *me_ptr,
                                                  int8_t *    filter_cfg,
                                                  uint16_t    num_of_taps,
                                                  int8_t *    destination_ptr,
                                                  uint32_t    max_dest_size)
{
   uint32_t size_copied   = 0;
   int8_t * temp_dest_ptr = destination_ptr;
   uint32_t ch_group_mask = *((uint32_t *)filter_cfg);
   uint32_t ch_mask_list_size = capi_cmn_count_set_bits(ch_group_mask);
   uint32_t cfg_size          = sizeof(fir_filter_cfg_v2_t) + (ch_mask_list_size << 2);

   size_copied = memscpy(destination_ptr, max_dest_size, filter_cfg, cfg_size);
   temp_dest_ptr += size_copied;
   uint32_t *buffer_ptr = (uint32_t *)temp_dest_ptr;
   int16_t * temp_ptr   = (int16_t *)(filter_cfg + size_copied);

   // Incrementing by 4 bytes because coefficients are stored in internal cached structure after
   // 4 bytes(if the channel mask list size is odd) to make coefficients 8 bytes align
   uint32_t alignBytes = (ch_mask_list_size & 1) ? 2 : 0;
   temp_ptr += alignBytes;
#ifdef QDSP6_ASM_OPT_FIR_FILTER
   temp_ptr += num_of_taps; // temp_ptr is pointing to end of coefficients
   temp_ptr--;
#endif
   for (uint32_t count = 0; count < num_of_taps; count++)
   {
#ifdef QDSP6_ASM_OPT_FIR_FILTER
      *buffer_ptr++ = *temp_ptr--; // reversing the coefficients and storing it in provided memory
#else
      *buffer_ptr++ = *temp_ptr++;
#endif
   }
   size_copied += (num_of_taps * CAPI_CMN_INT32_SIZE_IN_BYTES);
#ifdef CAPI_FIR_DEBUG_MSG
   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "ch_group_mask, %#lx, cfg_size: %lu, size_copied %lu",
		   ch_group_mask,
		   cfg_size,
		   size_copied);
#endif
   return size_copied;
}
