/* ======================================================================== */
/**
  @file  capi_fir_filter_utils.cpp

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

static uint32_t   capi_fir_get_kpps(capi_fir_t *me_ptr);
static void       capi_fir_set_static_params(capi_fir_t *me_ptr);
static uint32_t   capi_fir_calculate_cache_size_for_coeff(uint32_t miid, fir_filter_cfg_t *filter_coeff_cfg_ptr, uint32_t num_cfg);
static capi_err_t capi_fir_is_supported_v2_media_type(capi_fir_t *me_ptr, const capi_media_fmt_v2_t *format_ptr);
/*------------------------------------------------------------------------
 * Function definitions
 * -----------------------------------------------------------------------*/
static inline uint32_t capi_fir_coeff_config_payload_increment_size(fir_filter_cfg_t *filter_coeff_cfg_ptr)
{
   return (sizeof(fir_filter_cfg_t) + (sizeof(uint32_t) * filter_coeff_cfg_ptr->num_taps));
}

static inline uint32_t capi_fir_coeff_config_cache_increment_size(fir_filter_cfg_t *filter_coeff_cfg_ptr)
{
   uint16_t no_of_bytes = filter_coeff_cfg_ptr->coef_width >> 3;
   uint16_t alignment_bytes;

   alignment_bytes =
      (filter_coeff_cfg_ptr->num_taps % 4)
         ? (4 - (filter_coeff_cfg_ptr->num_taps % 4)) * (no_of_bytes)
         : 0; // To make filter taps multiple of 4,so that the next set of coefficients are also aligned to 8

   return (sizeof(fir_filter_cfg_t) + (no_of_bytes * filter_coeff_cfg_ptr->num_taps) + alignment_bytes);
}

/*===========================================================================
    FUNCTION : capi_fir_init_events
    DESCRIPTION: Function to init all the events with invalid values.
===========================================================================*/
void capi_fir_init_events(capi_fir_t *const me_ptr)
{
   me_ptr->event_config.enable = TRUE;

   // Setting events to maximum(invalid) value.
   me_ptr->event_config.kpps        = 0x7FFFFFFF;
   me_ptr->event_config.code_bw     = 0x7FFFFFFF;
   me_ptr->event_config.data_bw     = 0x7FFFFFFF;
   me_ptr->event_config.delay_in_us = 0x7FFFFFFF;
}

/*===========================================================================
FUNCTION : capi_fir_raise_bw_event
DESCRIPTION: Function to raise bandwidth event
===========================================================================*/
static capi_err_t capi_fir_raise_bw_event(capi_fir_t *me_ptr)
{
   capi_err_t capi_result    = CAPI_EOK;
   uint32_t   code_bandwidth = 0;
   uint32_t   data_bandwidth = 2 * 1024 * 1024;

   // vote for double bw if usecase is voice
   // vote for double bw if usecase is audio and crossfade going on
   if ((me_ptr->is_module_in_voice_graph) || (me_ptr->combined_crossfade_status))
   {
      data_bandwidth = 2 * data_bandwidth;
   }

   if ((code_bandwidth != me_ptr->event_config.code_bw) || (data_bandwidth != me_ptr->event_config.data_bw))
   {
      me_ptr->event_config.code_bw = code_bandwidth;
      me_ptr->event_config.data_bw = data_bandwidth;

      capi_result = capi_cmn_update_bandwidth_event(&me_ptr->cb_info, code_bandwidth, data_bandwidth);
      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed to send bandwidth update event with %lu", capi_result);
      }
   }
   return capi_result;
}

/*===========================================================================
FUNCTION : capi_fir_raise_delay_event
DESCRIPTION: Function to raise delay event
===========================================================================*/
static capi_err_t capi_fir_raise_delay_event(capi_fir_t *me_ptr)
{
   capi_err_t capi_result = CAPI_EOK;
   uint32_t   delay_in_us = 0;

   if (NULL == me_ptr->cb_info.event_cb)
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Event callback is not set. Unable to raise events!");
      CAPI_SET_ERROR(capi_result, CAPI_EUNSUPPORTED);
      return capi_result;
   }

   // Updating the delay.
   delay_in_us = capi_fir_calculate_delay_of_filter(me_ptr);

   // Checking and raising the delay.
   if (delay_in_us != me_ptr->event_config.delay_in_us)
   {
      me_ptr->event_config.delay_in_us = delay_in_us;

      capi_result = capi_cmn_update_algo_delay_event(&me_ptr->cb_info, delay_in_us);
      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed to send algorithmic delay update event with %lu", capi_result);
      }
   }

   return capi_result;
}

/*===========================================================================
 FUNCTION : capi_fir_raise_kpps_event
 DESCRIPTION: Function to raise the kpps event
 ===========================================================================*/
static capi_err_t capi_fir_raise_kpps_event(capi_fir_t *me_ptr)
{
   capi_err_t capi_result = CAPI_EOK;
   uint32_t   kpps        = 0;
   if (NULL == me_ptr->cb_info.event_cb)
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Event callback is not set. Unable to raise events!");
      CAPI_SET_ERROR(capi_result, CAPI_EUNSUPPORTED);
      return capi_result;
   }

   if ((NULL != me_ptr->fir_channel_lib) && (NULL != me_ptr->cache_fir_max_tap) &&
       (NULL != me_ptr->cache_fir_coeff_cfg))
   {
      kpps = capi_fir_get_kpps(me_ptr);
   }

   // vote for double kpps if usecase is voice
   // vote for double kpps if usecase is audio and crossfade going on
   if ((me_ptr->is_module_in_voice_graph) || (me_ptr->combined_crossfade_status))
   {
      kpps = 2 * kpps;
   }

   if (kpps != me_ptr->event_config.kpps)
   {
      me_ptr->event_config.kpps = kpps;
      capi_result               = capi_cmn_update_kpps_event(&me_ptr->cb_info, me_ptr->event_config.kpps);

      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed to send KPPS update event with %lu", capi_result);
      }
   }

   return capi_result;
}

/*===========================================================================
FUNCTION : capi_fir_raise_process_event
DESCRIPTION: Function to raise the process event
===========================================================================*/

capi_err_t capi_fir_raise_process_event(capi_fir_t *me_ptr)
{
   capi_err_t capi_result = CAPI_EOK;

   if (NULL == me_ptr->cb_info.event_cb)
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Event callback is not set. Unable to raise events!");
      return CAPI_EBADPARAM;
   }

   bool_t process_check = capi_fir_filter_lib_is_enabled(me_ptr);

   if (process_check != me_ptr->event_config.enable)
   {
      capi_result = capi_cmn_update_process_check_event(&me_ptr->cb_info, process_check);
      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed to send process_check update event with %lu", capi_result);
      }
      else
      {
         me_ptr->event_config.enable = process_check;
      }
   }
   return capi_result;
}

/*===========================================================================
FUNCTION : capi_fir_raise_events
DESCRIPTION: Function to raise all the events
===========================================================================*/
capi_err_t capi_fir_raise_events(capi_fir_t *const me_ptr)
{
   capi_err_t result = CAPI_EOK;

   // raise events which are only dependent on media format
   result |= capi_fir_raise_kpps_event(me_ptr);
   result |= capi_fir_raise_bw_event(me_ptr);
   result |= capi_fir_raise_delay_event(me_ptr);

   result |= capi_fir_raise_process_event(me_ptr);
   return result;
}

/*===========================================================================
 FUNCTION : capi_fir_is_supported_v2_media_type
 DESCRIPTION: Function to check if a media type is supported by fir module
 ===========================================================================*/
static capi_err_t capi_fir_is_supported_v2_media_type(capi_fir_t *me_ptr, const capi_media_fmt_v2_t *format_ptr)
{
   if ((16 != format_ptr->format.bits_per_sample) && (32 != format_ptr->format.bits_per_sample))
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Supports 16,32 bit data. Received %lu bit data",
             format_ptr->format.bits_per_sample);
      return CAPI_EUNSUPPORTED;
   }

   if ((CAPI_DEINTERLEAVED_UNPACKED != format_ptr->format.data_interleaving)&&
       (format_ptr->format.num_channels != 1))
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Interleaving format is not as expected.");
      return CAPI_EUNSUPPORTED;
   }

   if (!format_ptr->format.data_is_signed)
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Unsigned data not supported.");
      return CAPI_EUNSUPPORTED;
   }

   if (!format_ptr->format.num_channels)
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "%lu channels not supported.", format_ptr->format.num_channels);
      return CAPI_EUNSUPPORTED;
   }

   if (!format_ptr->format.sampling_rate)
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "%lu Sampling rate is not supported.", format_ptr->format.sampling_rate);
      return CAPI_EUNSUPPORTED;
   }

   if ((format_ptr->format.q_factor != PCM_Q_FACTOR_15) && (format_ptr->format.q_factor != PCM_Q_FACTOR_27))
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Q factor %lu is not supported.", format_ptr->format.q_factor);
      return CAPI_EUNSUPPORTED;
   }

   for (uint32_t ch = 0; ch < format_ptr->format.num_channels; ch++)
   {
      if ((PCM_MAX_CHANNEL_MAP_V2 < format_ptr->channel_type[ch]) || (1 > format_ptr->channel_type[ch]))
      {
    	  FIR_MSG(me_ptr->miid,
    			  DBG_ERROR_PRIO,
                "CAPI MSIIR : channel number %lu has unsupported type %hu",
                ch,
                format_ptr->channel_type[ch]);
         return FALSE;
      }
      if(format_ptr->channel_type[ch] > PCM_MAX_CHANNEL_MAP)
      {  //If a higher channel map(>63) in the media format occurs even once in the execution history,
         //the flag will be set to true from that point onward.
   	     me_ptr->higher_channel_map_present = TRUE;
      }
   }
   return CAPI_EOK;
}

/*===========================================================================================
  Function name: capi_fir_check_create_lib_instance
  Description : Function to check the media format, static params and create the fir instance.
  ===========================================================================================*/
capi_err_t capi_fir_check_create_lib_instance(capi_fir_t *me_ptr, bool_t is_num_channels_changed)
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

   if (NULL == me_ptr->cache_fir_max_tap)
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Not creating lib. Max Tap length is not received yet.");
      return CAPI_EOK;
   }

   capi_fir_set_static_params(me_ptr);

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

      int8_t *temp_ptr;
      if ((temp_lib_mem_req.lib_mem_size != lib_mem_req->lib_mem_size) || (NULL == fir_lib_instance->lib_mem_ptr))
      {
#if CAPI_FIR_DEBUG_MSG
         FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Allocating library memory, current size %lu, required size %lu\n",
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
            FIR_MSG(me_ptr->miid, DBG_FATAL_PRIO, "Failed creating new instance of library: Out of Memory for chan : %lu!",
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
   if (NULL != me_ptr->cache_original_queue_fir_coeff_cfg)
   {
      me_ptr->config_type = CAPI_QUEUE_CFG;
      capi_result |= capi_fir_set_cached_filter_coeff(me_ptr);
	  //free prev configs 
	  capi_fir_update_release_config(me_ptr); // this will just delete 1 config
      capi_fir_update_release_config(me_ptr); // this will delete 2nd config
      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed setting coeff config param after creating new library instance.");
         capi_fir_clean_up_cached_coeff_memory(me_ptr); // clean all configs present
         return CAPI_EFAILED;
      }
   }
   // 2.set cached new filter coefficients if present (c2)
   else if (NULL != me_ptr->cache_original_next_fir_coeff_cfg)
   {
      me_ptr->config_type = CAPI_NEXT_CFG;
      capi_result |= capi_fir_set_cached_filter_coeff(me_ptr);
	  capi_fir_update_release_config(me_ptr); //free prev config
      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed setting coeff config param after creating new library instance.");
         capi_fir_clean_up_cached_coeff_memory(me_ptr); // clean all configs present
         return CAPI_EFAILED;
      }
   }
   // 3.set cached current filter coefficients if present (c1)
   else if (NULL != me_ptr->cache_original_fir_coeff_cfg)
   {
      me_ptr->config_type = CAPI_CURR_CFG;
      capi_result |= capi_fir_set_cached_filter_coeff(me_ptr);
      if (CAPI_FAILED(capi_result))
      {
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed setting coeff config param after creating new library instance.");
         capi_fir_clean_up_cached_coeff_memory(me_ptr); // clean all configs present
         return CAPI_EFAILED;
      }
   }
   else
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Filter config not present in create lib instance");
   }

   // set cached crossfade config if present
   capi_result |= capi_fir_set_cached_filter_crossfade_config(me_ptr);
   if (CAPI_FAILED(capi_result))
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Failed setting filter crossfade config param.");
      return CAPI_EFAILED;
   }
   capi_fir_raise_events(me_ptr);
   return capi_result;
}

/*===========================================================================
  Function name: capi_fir_free_lib_memory
  DESCRIPTION : This function frees the memory allocated for the fir library
                and the cached parameters
  ===========================================================================*/

void capi_fir_free_memory(capi_fir_t *me_ptr)
{
   capi_fir_channel_lib_t *fir_channel_lib    = me_ptr->fir_channel_lib;
   uint32_t                channels_allocated = me_ptr->input_media_fmt[0].format.num_channels;

   if (NULL != me_ptr->fir_channel_lib)
   {
      for (uint32_t chan_num = 0; chan_num < channels_allocated; chan_num++)
      {
         fir_lib_t *fir_lib_instance = &(fir_channel_lib[chan_num].fir_lib_instance);
         if (NULL != fir_lib_instance->lib_mem_ptr)
         {
            posal_memory_free(fir_lib_instance->lib_mem_ptr);
            fir_lib_instance->lib_mem_ptr = NULL;
         }
      }
      posal_memory_free(me_ptr->fir_channel_lib);
      me_ptr->fir_channel_lib = NULL;
   }
   if (NULL != me_ptr->cache_fir_max_tap) // free max tap len config
   {
      posal_memory_free(me_ptr->cache_fir_max_tap);
      me_ptr->cache_fir_max_tap      = NULL;
      me_ptr->cache_fir_max_tap_size = 0;
   }

   if (NULL != me_ptr->cache_original_fir_coeff_cfg) // free current config
   {
      posal_memory_aligned_free(me_ptr->cache_original_fir_coeff_cfg);
      me_ptr->cache_original_fir_coeff_cfg = NULL;
      me_ptr->cache_fir_coeff_cfg          = NULL;
      me_ptr->cache_fir_coeff_cfg_size     = 0;
      me_ptr->size_req_for_get_coeff	   = 0;
   }

   if (NULL != me_ptr->cache_original_next_fir_coeff_cfg) // free next config if present
   {
      posal_memory_aligned_free(me_ptr->cache_original_next_fir_coeff_cfg);
      me_ptr->cache_original_next_fir_coeff_cfg = NULL;
      me_ptr->cache_next_fir_coeff_cfg          = NULL;
      me_ptr->cache_next_fir_coeff_cfg_size     = 0;
      me_ptr->size_req_for_get_next_coeff	    = 0;
   }

   if (NULL != me_ptr->cache_original_queue_fir_coeff_cfg) // free pending config if any
   {
      posal_memory_aligned_free(me_ptr->cache_original_queue_fir_coeff_cfg);
      me_ptr->cache_original_queue_fir_coeff_cfg = NULL;
      me_ptr->cache_queue_fir_coeff_cfg          = NULL;
      me_ptr->cache_queue_fir_coeff_cfg_size     = 0;
      me_ptr->size_req_for_get_queue_coeff       = 0;
   }
   if (NULL != me_ptr->cache_fir_crossfade_cfg) // free crossfade config
   {
      posal_memory_free(me_ptr->cache_fir_crossfade_cfg);
      me_ptr->cache_fir_crossfade_cfg      = NULL;
      me_ptr->cache_fir_crossfade_cfg_size = 0;
   }
   //V2 configs structure
   if (NULL != me_ptr->capi_fir_v2_cfg.cache_fir_max_tap) // free max tap len config
   {
      posal_memory_free(me_ptr->capi_fir_v2_cfg.cache_fir_max_tap);
      me_ptr->capi_fir_v2_cfg.cache_fir_max_tap      = NULL;
      me_ptr->capi_fir_v2_cfg.cache_fir_max_tap_size = 0;
   }

   if (NULL != me_ptr->capi_fir_v2_cfg.cache_original_fir_coeff_cfg) // free current config
   {
      posal_memory_aligned_free(me_ptr->capi_fir_v2_cfg.cache_original_fir_coeff_cfg);
      me_ptr->capi_fir_v2_cfg.cache_original_fir_coeff_cfg = NULL;
      me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg          = NULL;
      me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg_size     = 0;
      me_ptr->capi_fir_v2_cfg.size_req_for_get_coeff	   = 0;
   }

   if (NULL != me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg) // free next config if present
   {
      posal_memory_aligned_free(me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg);
      me_ptr->capi_fir_v2_cfg.cache_original_next_fir_coeff_cfg = NULL;
      me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg          = NULL;
      me_ptr->capi_fir_v2_cfg.cache_next_fir_coeff_cfg_size     = 0;
      me_ptr->capi_fir_v2_cfg.size_req_for_get_next_coeff	    = 0;
   }

   if (NULL != me_ptr->capi_fir_v2_cfg.cache_original_queue_fir_coeff_cfg) // free pending config if any
   {
      posal_memory_aligned_free(me_ptr->capi_fir_v2_cfg.cache_original_queue_fir_coeff_cfg);
      me_ptr->capi_fir_v2_cfg.cache_original_queue_fir_coeff_cfg = NULL;
      me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg          = NULL;
      me_ptr->capi_fir_v2_cfg.cache_queue_fir_coeff_cfg_size     = 0;
      me_ptr->capi_fir_v2_cfg.size_req_for_get_queue_coeff       = 0;
   }
   if (NULL != me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg) // free crossfade config
   {
      posal_memory_free(me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg);
      me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg      = NULL;
      me_ptr->capi_fir_v2_cfg.cache_fir_crossfade_cfg_size = 0;
   }
}

/*===========================================================================
FUNCTION : capi_fir_process_get_properties
DESCRIPTION: Fir get properties
===========================================================================*/
capi_err_t capi_fir_process_get_properties(capi_fir_t *me_ptr, capi_proplist_t *proplist_ptr)
{
   capi_err_t capi_result = CAPI_EOK;

   capi_prop_t *const prop_array = proplist_ptr->prop_ptr;
   uint32_t miid = me_ptr ? me_ptr->miid : MIID_UNKNOWN;
   
   if (NULL == prop_array)
   {
      FIR_MSG(miid, DBG_ERROR_PRIO, "Get property received null property array.");
      return CAPI_EBADPARAM;
   }

   capi_basic_prop_t mod_prop;
   mod_prop.init_memory_req    = sizeof(capi_fir_t);
   mod_prop.stack_size         = CAPI_FIR_STACK_SIZE;
   mod_prop.num_fwk_extns      = 0;     // NA
   mod_prop.fwk_extn_ids_arr   = NULL;  // NA
   mod_prop.is_inplace         = FALSE; // not capable of in-place processing of data
   mod_prop.req_data_buffering = FALSE;
   mod_prop.max_metadata_size  = 0; // NA

   capi_result = capi_cmn_get_basic_properties(proplist_ptr, &mod_prop);
   if (CAPI_EOK != capi_result)
   {
	  FIR_MSG(miid, DBG_ERROR_PRIO, "Get common basic properties failed with result %lu", capi_result);
      return capi_result;
   }

   for (uint32_t i = 0; i < proplist_ptr->props_num; i++)
   {
      capi_buf_t *const payload_ptr          = &(prop_array[i].payload);
      const uint32_t    payload_max_data_len = payload_ptr->max_data_len;
      const uint32_t    param_id             = prop_array[i].id;
	  miid = me_ptr ? me_ptr->miid : MIID_UNKNOWN;
      if (NULL == payload_ptr->data_ptr)
      {
         FIR_MSG(miid, DBG_ERROR_PRIO, "Get property id 0x%lx, received null buffer.", param_id);
         CAPI_SET_ERROR(capi_result, CAPI_EBADPARAM);
         continue;
      }

      capi_result = CAPI_EOK;
      switch (param_id)
      {
         case CAPI_INIT_MEMORY_REQUIREMENT:
         case CAPI_IS_INPLACE:
         case CAPI_REQUIRES_DATA_BUFFERING:
         case CAPI_STACK_SIZE:
         case CAPI_PORT_DATA_THRESHOLD:
         case CAPI_MAX_METADATA_SIZE:
         {
            break;
         }

         case CAPI_OUTPUT_MEDIA_FORMAT_SIZE:
         {
            if (payload_max_data_len >= sizeof(capi_output_media_format_size_t))
            {
               capi_output_media_format_size_t *data_ptr = (capi_output_media_format_size_t *)payload_ptr->data_ptr;
               data_ptr->size_in_bytes                   = sizeof(capi_standard_data_format_t);
               payload_ptr->actual_data_len              = sizeof(capi_output_media_format_size_t);
            }
            else
            {
               FIR_MSG(miid, DBG_ERROR_PRIO, "Get property id 0x%lx, Bad param size %lu",
                      param_id,
                      payload_max_data_len);
               CAPI_SET_ERROR(capi_result, CAPI_ENEEDMORE);
            }
            break;
         }

         case CAPI_OUTPUT_MEDIA_FORMAT_V2:
         {
            if (NULL == me_ptr)
            {
               FIR_MSG(miid, DBG_ERROR_PRIO, "Get property id 0x%lx, module is not allocated", param_id);
               CAPI_SET_ERROR(capi_result, CAPI_EBADPARAM);
               break;
            }
            uint32_t required_size = sizeof(me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].header) +
                                     sizeof(me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].format);

            if (payload_max_data_len >= required_size)
            {
               capi_media_fmt_v2_t *data_ptr = (capi_media_fmt_v2_t *)payload_ptr->data_ptr;
               capi_channel_type_t *channel_type;
               uint32_t             channel_type_size =
                  sizeof(capi_channel_type_t) * me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].format.num_channels;
               channel_type_size = CAPI_FIR_ALIGN_4_BYTE(channel_type_size);
               required_size += channel_type_size;

               if (payload_max_data_len < required_size)
               {
                  FIR_MSG(miid, DBG_ERROR_PRIO, "Get property id 0x%lx, Bad param size %lu",
                         param_id,
                         payload_max_data_len);
                  CAPI_SET_ERROR(capi_result, CAPI_ENEEDMORE);
                  break;
               }

               // sanity check for valid and output port
               if ((!prop_array[i].port_info.is_valid) || (prop_array[i].port_info.is_input_port))
               {
                  FIR_MSG(miid, DBG_ERROR_PRIO, "Get property id 0x%lx, invalid port info.", param_id);
                  CAPI_SET_ERROR(capi_result, CAPI_EBADPARAM);
                  break;
               }

               // sanity check for valid output port index
               if (CAPI_FIR_DEFAULT_PORT != prop_array[i].port_info.port_index)
               {
                  FIR_MSG(miid, DBG_ERROR_PRIO, "Get property id 0x%lx, invalid output port index.", param_id);
                  CAPI_SET_ERROR(capi_result, CAPI_EBADPARAM);
                  break;
               }

               data_ptr->header = me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].header;
               data_ptr->format = me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].format;
               channel_type =
                  (capi_channel_type_t *)(((int8_t *)data_ptr) + sizeof(data_ptr->header) + sizeof(data_ptr->format));
               memscpy(channel_type,
                       sizeof(capi_channel_type_t) * data_ptr->format.num_channels,
                       me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].channel_type,
                       sizeof(me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].channel_type));
               payload_ptr->actual_data_len = required_size;
            }
            else
            {
               FIR_MSG(miid, DBG_ERROR_PRIO, "Get property id 0x%lx, Bad param size %lu",
                      param_id,
                      payload_max_data_len);
               CAPI_SET_ERROR(capi_result, CAPI_ENEEDMORE);
            }
            break;
         }

         case CAPI_INTERFACE_EXTENSIONS:
         {
            if (payload_ptr->max_data_len >= sizeof(capi_interface_extns_list_t))
            {
               capi_interface_extns_list_t *intf_ext_list = (capi_interface_extns_list_t *)payload_ptr->data_ptr;
               if (payload_ptr->max_data_len < (sizeof(capi_interface_extns_list_t) +
                                                (intf_ext_list->num_extensions * sizeof(capi_interface_extn_desc_t))))
               {
                  FIR_MSG(miid, DBG_ERROR_PRIO, "CAPI_INTERFACE_EXTENSIONS invalid param size %lu",
                         payload_ptr->max_data_len);
                  CAPI_SET_ERROR(capi_result, CAPI_ENEEDMORE);
               }
               else
               {
                  capi_interface_extn_desc_t *curr_intf_extn_desc_ptr =
                     (capi_interface_extn_desc_t *)(payload_ptr->data_ptr + sizeof(capi_interface_extns_list_t));

                  for (uint32_t i = 0; i < intf_ext_list->num_extensions; i++)
                  {
                     switch (curr_intf_extn_desc_ptr->id)
                     {
                        case INTF_EXTN_PERIOD:
                        {
                           curr_intf_extn_desc_ptr->is_supported = TRUE;
                           break;
                        }
                        default:
                        {
                           curr_intf_extn_desc_ptr->is_supported = FALSE;
                           break;
                        }
                     }
                     FIR_MSG(miid, DBG_HIGH_PRIO, "CAPI_INTERFACE_EXTENSIONS intf_ext = 0x%lx, is_supported = %d",
                            curr_intf_extn_desc_ptr->id,
                            (int)curr_intf_extn_desc_ptr->is_supported);
                     curr_intf_extn_desc_ptr++;
                  }
               }
            }
            else
            {
               FIR_MSG(miid, DBG_ERROR_PRIO, "CAPI_INTERFACE_EXTENSIONS Bad param size %lu",
                      payload_ptr->max_data_len);
               CAPI_SET_ERROR(capi_result, CAPI_ENEEDMORE);
            }
            break;
         }

         default:
         {
            FIR_MSG(miid, DBG_ERROR_PRIO, "Get property id 0x%lx, Not Supported.", param_id);
            CAPI_SET_ERROR(capi_result, CAPI_EUNSUPPORTED);
         }
         break;
      }
   }
   return capi_result;
}

/*===========================================================================
FUNCTION : capi_fir_process_set_properties
DESCRIPTION: FIR set properties
===========================================================================*/

capi_err_t capi_fir_process_set_properties(capi_fir_t *me_ptr, capi_proplist_t *proplist_ptr)
{
   capi_err_t               capi_result = CAPI_EOK;
   if (NULL == me_ptr)
   {
      FIR_MSG(MIID_UNKNOWN, DBG_ERROR_PRIO, "Set common property received null ptr");
      return CAPI_EBADPARAM;
   }
   const capi_prop_t *const prop_array  = proplist_ptr->prop_ptr;
   uint32_t miid = me_ptr ? me_ptr->miid : MIID_UNKNOWN;

   if (NULL == prop_array)
   {
      FIR_MSG(miid, DBG_ERROR_PRIO, "Set property received null property array.");
      return CAPI_EBADPARAM;
   }

   capi_result = capi_cmn_set_basic_properties(proplist_ptr, &me_ptr->heap_info, &me_ptr->cb_info, TRUE);
   if (CAPI_EOK != capi_result)
   {
      FIR_MSG(miid, DBG_ERROR_PRIO, "Get basic properties failed with result %lu", capi_result);
      return capi_result;
   }

   for (uint8_t i = 0; i < proplist_ptr->props_num; i++)
   {
      const capi_buf_t *const payload_ptr             = &(prop_array[i].payload);
      const uint32_t          payload_actual_data_len = payload_ptr->actual_data_len;
      const uint32            param_id                = prop_array[i].id;
	  miid = me_ptr ? me_ptr->miid : MIID_UNKNOWN;
      capi_result = CAPI_EOK;
      switch (param_id)
      {
         case CAPI_PORT_NUM_INFO:
         case CAPI_HEAP_ID:
         case CAPI_EVENT_CALLBACK_INFO:
         {
            break;
         }
         case CAPI_ALGORITHMIC_RESET:
         {
            FIR_MSG(miid, DBG_HIGH_PRIO, "received RESET");
            capi_result |= capi_fir_reset(me_ptr);
         }
         break;
		 case CAPI_MODULE_INSTANCE_ID:
         {
            if (payload_ptr->actual_data_len >= sizeof(capi_module_instance_id_t))
            {
               capi_module_instance_id_t *data_ptr = (capi_module_instance_id_t *)payload_ptr->data_ptr;
               me_ptr->miid                        = data_ptr->module_instance_id;
               FIR_MSG(miid, DBG_LOW_PRIO,
                      "This module-id 0x%08lX, instance-id 0x%08lX",
                      data_ptr->module_id,
                      me_ptr->miid);
            }
            else
            {
               FIR_MSG(miid, DBG_ERROR_PRIO,
                      "Set property id 0x%lx, Bad param size %lu",
                      prop_array[i].id,
                      payload_ptr->max_data_len);
               capi_result |= CAPI_ENEEDMORE;
            }
            break;
         } // CAPI_MODULE_INSTANCE_ID	 
         case CAPI_INPUT_MEDIA_FORMAT_V2:
         {
            if (NULL == payload_ptr->data_ptr)
            {
               FIR_MSG(miid, DBG_ERROR_PRIO, "Set property id 0x%lx, received null buffer.", param_id);
               CAPI_SET_ERROR(capi_result, CAPI_EBADPARAM);
               break;
            }
            uint32_t required_size = sizeof(capi_set_get_media_format_t) + sizeof(capi_standard_data_format_v2_t);
            if (payload_actual_data_len >= required_size)
            {

               // sanity check for valid and input port
               if ((!prop_array[i].port_info.is_valid) || (!prop_array[i].port_info.is_input_port))
               {
                  FIR_MSG(miid, DBG_ERROR_PRIO, "Set property id 0x%lx, invalid port info.", param_id);
                  CAPI_SET_ERROR(capi_result, CAPI_EBADPARAM);
                  break;
               }

               // sanity check for valid input port index
               if (CAPI_FIR_DEFAULT_PORT != prop_array[i].port_info.port_index)
               {
                  FIR_MSG(miid, DBG_ERROR_PRIO, "Set property id 0x%lx, invalid input port index %lu",
                         param_id,
                         prop_array[i].port_info.port_index);
                  CAPI_SET_ERROR(capi_result, CAPI_EBADPARAM);
                  break;
               }

               const capi_media_fmt_v2_t *const in_data_ptr = (capi_media_fmt_v2_t *)(payload_ptr->data_ptr);

               if (in_data_ptr->format.minor_version >= CAPI_MEDIA_FORMAT_MINOR_VERSION)
               {
                  capi_err_t result = capi_fir_is_supported_v2_media_type(me_ptr, in_data_ptr);
                  required_size += in_data_ptr->format.num_channels * sizeof(capi_channel_type_t);
                  if (payload_actual_data_len < required_size)
                  {
                     FIR_MSG(miid, DBG_ERROR_PRIO, "Set property id 0x%lx, Bad param size %lu",
                            param_id,
                            payload_actual_data_len);
                     CAPI_SET_ERROR(capi_result, CAPI_ENEEDMORE);
                     break;
                  }

                  if (CAPI_EOK != result)
                  {
                     CAPI_SET_ERROR(capi_result, CAPI_EUNSUPPORTED);
                     memset(&me_ptr->input_media_fmt[0], 0, sizeof(me_ptr->input_media_fmt[0]));
                     break;
                  }
                  bool_t is_num_channels_changed = 0;
                  if (in_data_ptr->format.num_channels !=
                      me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].format.num_channels)
                  {
                     is_num_channels_changed = 1;
                     capi_fir_clean_up_lib_memory(me_ptr);
                  }

                  me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].header.format_header =
                     in_data_ptr->header.format_header;
                  me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].format               = in_data_ptr->format;
                  me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].format.minor_version = CAPI_MEDIA_FORMAT_MINOR_VERSION;
                  memscpy(me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].channel_type,
                          sizeof(me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT].channel_type),
                          in_data_ptr->channel_type,
                          in_data_ptr->format.num_channels * sizeof(in_data_ptr->channel_type[0]));

                  capi_cmn_output_media_fmt_event_v2(&me_ptr->cb_info,
                                                     &me_ptr->input_media_fmt[CAPI_FIR_DEFAULT_PORT],
                                                     FALSE,
                                                     CAPI_FIR_DEFAULT_PORT);

				  // check and create library instances
                  if((VERSION_V1 == me_ptr->cfg_version) && (FALSE == me_ptr->higher_channel_map_present))
				  {
                     capi_result |= capi_fir_check_create_lib_instance(me_ptr, is_num_channels_changed);
				  }
                  else if (VERSION_V2 == me_ptr->cfg_version)
                  {
                	 capi_result |= capi_fir_check_create_lib_instance_v2(me_ptr, is_num_channels_changed);
                  }
				  if (CAPI_FAILED(capi_result))
				  {
					 return capi_result;
				  }
               }
               else
               {
                  FIR_MSG(miid, DBG_ERROR_PRIO, "Invalid minor version %lu", in_data_ptr->format.minor_version);
                  CAPI_SET_ERROR(capi_result, CAPI_EBADPARAM);
               }
            }
            else
            {
               FIR_MSG(miid, DBG_ERROR_PRIO, "Set property id 0x%lx, Bad param size %lu",
                      param_id,
                      payload_actual_data_len);
               CAPI_SET_ERROR(capi_result, CAPI_ENEEDMORE);
            }
         }
         break;

         default:
         {
#if CAPI_FIR_DEBUG_MSG
            FIR_MSG(miid, DBG_ERROR_PRIO, "Set property id 0x%lx, Not supported.", param_id);
#endif
            CAPI_SET_ERROR(capi_result, CAPI_EUNSUPPORTED);
         }
         break;
      }
   }
   return capi_result;
}

/*==============================================================
 * Function : capi_fir_get_kpps
 * Description : Function to get kpps
 ===============================================================*/

uint32_t capi_fir_get_kpps(capi_fir_t *me_ptr)
{
   uint32_t kpps_value    = 0;
   uint32_t num_chan      = me_ptr->input_media_fmt[0].format.num_channels;
   uint32_t data_width    = me_ptr->input_media_fmt[0].format.bits_per_sample;
   uint32_t coeff_width   = 0;
   uint32_t num_taps      = 0;
   uint32_t sampling_freq = me_ptr->input_media_fmt[0].format.sampling_rate;
   for (uint32_t ch = 0; ch < num_chan; ch++)
   {
      coeff_width = me_ptr->fir_channel_lib[ch].fir_config_variables.coef_width;
      num_taps    = me_ptr->fir_channel_lib[ch].fir_config_variables.num_taps;
      if ((16 == data_width) && (16 == coeff_width))
      {
         kpps_value += (0.00025) * num_taps * sampling_freq;
      }
      else if ((32 == data_width) && (32 == coeff_width))
      {
         kpps_value += (0.00057) * num_taps * sampling_freq;
      }
      else
      {
         kpps_value += (0.000453) * num_taps * sampling_freq;
      }
   }
   return kpps_value;
}

/*=====================================================================
  Function : capi_fir_check_channel_map_max_tap_cfg
  Description : Function to validate channel maps for max tap parameter
 =======================================================================*/

capi_err_t capi_fir_check_channel_map_max_tap_cfg(fir_filter_max_tap_length_cfg_t *cfg_ptr, uint32_t num_config)
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

/*=======================================================================
  Function name: capi_fir_check_channel_map_coeff_cfg
Description : Function to validate channel map for filter coeff cfg.
  =======================================================================*/
capi_err_t capi_fir_check_channel_map_coeff_cfg(uint32_t miid, fir_filter_cfg_t *cfg_ptr, uint32_t num_config)
{
   uint64_t          channel_mask = 0;
   uint64_t          channel_map  = 0;
   fir_filter_cfg_t *temp_cfg_ptr;
   int8_t *          temp_ptr = (int8_t *)cfg_ptr;
   for (uint32_t count = 0; count < num_config; count++)
   {
      temp_cfg_ptr = (fir_filter_cfg_t *)temp_ptr;
      channel_map  = capi_fir_calculate_channel_mask(temp_cfg_ptr->channel_mask_lsb, temp_cfg_ptr->channel_mask_msb);
      if (channel_mask & channel_map)
      {
         FIR_MSG(miid, DBG_HIGH_PRIO, "Received incorrect channel map for filter coefficients for cfg #%lu",
                count);
         return CAPI_EBADPARAM;
      }
      channel_mask |= channel_map;
      temp_ptr += capi_fir_coeff_config_payload_increment_size(temp_cfg_ptr);
   }
   return CAPI_EOK;
}

/*===========================================================================
  Function : capi_fir_set_cached_filter_coeff
  Description : Function to set filter config params using the cached params
 ============================================================================*/
capi_err_t capi_fir_set_cached_filter_coeff(capi_fir_t *me_ptr)
{
   param_id_fir_filter_config_t *fir_cfg_state;
   int8_t *                      fir_cfg_ptr;
   if (CAPI_CURR_CFG == me_ptr->config_type)
   {
      fir_cfg_state = me_ptr->cache_fir_coeff_cfg;
      fir_cfg_ptr   = (int8_t *)me_ptr->cache_fir_coeff_cfg;
   }
   else if (CAPI_NEXT_CFG == me_ptr->config_type)
   {
      fir_cfg_state = me_ptr->cache_next_fir_coeff_cfg;
      fir_cfg_ptr   = (int8_t *)me_ptr->cache_next_fir_coeff_cfg;
   }
   else if (CAPI_QUEUE_CFG == me_ptr->config_type)
   {
      fir_cfg_state = me_ptr->cache_queue_fir_coeff_cfg;
      fir_cfg_ptr   = (int8_t *)me_ptr->cache_queue_fir_coeff_cfg;
   }
   else
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Did not receive expected config type while set filter config params using the cached "
             "params. "
             "Recieved %lu",
             me_ptr->config_type);
      return CAPI_EFAILED;
   }

   if (NULL == fir_cfg_state)
   {
      return CAPI_EOK;
   }

   uint32_t num_cfg = fir_cfg_state->num_config;
   fir_cfg_ptr +=
      CAPI_FIR_ALIGN_8_BYTE(sizeof(param_id_fir_filter_config_t)); // this is aligned to 8 because we want the
                                                                   // coefficients always at 8 byte boundary.
   fir_filter_cfg_t *fir_coeff_cfg = (fir_filter_cfg_t *)fir_cfg_ptr;

   capi_fir_channel_lib_t *       fir_channel_lib       = me_ptr->fir_channel_lib;
   fir_filter_cfg_t *const        coeff_cfg_ptr         = fir_coeff_cfg;
   fir_filter_cfg_t *             temp_coeff_cfg_ptr    = fir_coeff_cfg;
   const uint32_t                 channels_allocated    = me_ptr->input_media_fmt[0].format.num_channels;
   FIR_RESULT                     set_param_result      = FIR_SUCCESS;
   uint64_t                       channel_map           = 0;
   uint64_t                       channel_mask          = 0;
   uint32_t                       increment_size        = 0;
   int8_t *                       temp_increment_ptr    = (int8_t *)temp_coeff_cfg_ptr;
   fir_transition_status_struct_t fir_transition_status = { 0, 0, 0 };
   uint32_t                       filled_struct_size    = 0;
   FIR_RESULT                     lib_result;
   uint32_t is_in_transition = 0; // introducing this to identify if by applying new filter config
                                  // crossfade happened or not so that we can free the prev config
   for (uint32_t chan_num = 0; chan_num < channels_allocated; chan_num++)
   {
      temp_coeff_cfg_ptr = coeff_cfg_ptr;
      temp_increment_ptr = (int8_t *)temp_coeff_cfg_ptr;
      channel_map        = ((uint64_t)1) << me_ptr->input_media_fmt[0].channel_type[chan_num];
      for (uint32_t count = 0; count < num_cfg; count++)
      {
         channel_mask =
            capi_fir_calculate_channel_mask(temp_coeff_cfg_ptr->channel_mask_lsb, temp_coeff_cfg_ptr->channel_mask_msb);
         if (channel_map & channel_mask)
         {
            if (fir_channel_lib[chan_num].fir_static_variables.max_num_taps < temp_coeff_cfg_ptr->num_taps)
            {
               FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Received invalid tap length of %u for chan - %lu , Max value set - %lu",
                      temp_coeff_cfg_ptr->num_taps,
                      chan_num,
                      fir_channel_lib[chan_num].fir_static_variables.max_num_taps);
               return CAPI_EBADPARAM;
            }
            else
            {
               fir_channel_lib[chan_num].fir_config_variables.coefQFactor = temp_coeff_cfg_ptr->coef_q_factor;
               fir_channel_lib[chan_num].fir_config_variables.coef_width  = temp_coeff_cfg_ptr->coef_width;
               fir_channel_lib[chan_num].fir_config_variables.num_taps    = temp_coeff_cfg_ptr->num_taps;
               fir_channel_lib[chan_num].fir_config_variables.coeffs_ptr =
                  (uint64)&temp_coeff_cfg_ptr->filter_coefficients[0];

               // Incrementing by 4 bytes because coefficients are stored in internal cached structure after 4 bytes to
               // make it 8 bytes align
               fir_channel_lib[chan_num].fir_config_variables.coeffs_ptr += 4;


               if (NULL != fir_channel_lib[chan_num].fir_lib_instance.lib_mem_ptr)
               {
                  set_param_result = fir_set_param(&(fir_channel_lib[chan_num].fir_lib_instance),
                                                   FIR_PARAM_CONFIG,
                                                   (int8_t *)&fir_channel_lib[chan_num].fir_config_variables,
                                                   sizeof(fir_config_struct_t));
                  if (FIR_SUCCESS != set_param_result)
                  {
                     FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Filter coeff Set param failed for channel %lu with lib error %u",
                            chan_num,
                            set_param_result);
                     return CAPI_EFAILED;
                  }

                  // getting transition status to store info in capi so that we can: 1. check in process(), 2. free prev
                  // config
                  lib_result = fir_get_param(&fir_channel_lib[chan_num].fir_lib_instance,
                                             FIR_PARAM_GET_TRANSITION_STATUS,
                                             (int8_t *)(&fir_transition_status),
                                             sizeof(fir_transition_status_struct_t),
                                             &filled_struct_size);
                  if ((FIR_SUCCESS != lib_result))
                  {
                     FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed while getting transition status in set cached filter config");
                     return CAPI_EFAILED;
                  }
                  else
                  {
                     fir_channel_lib[chan_num].fir_transition_status_variables.flag = fir_transition_status.flag;
                     fir_channel_lib[chan_num].fir_transition_status_variables.coeffs_ptr =
                        fir_transition_status.coeffs_ptr;
                     FIR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Crossfade status flag for chan %lu after set filter config param: %lu",
                            chan_num,
                            fir_transition_status.flag);
					 is_in_transition |= fir_transition_status.flag; // is_in_transition will be set to 1 if any of the ch 
					                                                 // is in cross-fade
                  }
               }
            }
            break;
         }

         increment_size = capi_fir_coeff_config_cache_increment_size(temp_coeff_cfg_ptr);

         increment_size +=
            4; // Incrementing by 4 bytes because coefficients are stored after 4 bytes to make it 8 bytes allign

         temp_increment_ptr += increment_size;
         temp_coeff_cfg_ptr = (fir_filter_cfg_t *)temp_increment_ptr;
      }
   }
   me_ptr->combined_crossfade_status = is_in_transition; // this is updated in process when crossfade is completed
   if (!is_in_transition)                                // no ch is in transition after applying filter config
   {
      capi_fir_update_release_config(me_ptr); // update the new config and release prev config
                                              // this would ensure if crossfade not recieved previously for this
                                              // channel, filter config would store in 1 config only
   }
   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Filter coeff are set");
   return CAPI_EOK;
}

/*==================================================================
  Function : capi_fir_coefficient_convert_to_32bit
  Description : Function to convert filter coeff to 32 bit from 16bit
 ====================================================================*/
uint32_t capi_fir_coefficient_convert_to_32bit(capi_fir_t *me_ptr,
                                               int8_t     *filter_cfg,
                                               uint16_t    num_of_taps,
                                               int8_t     *destination_ptr,
                                               uint32_t    max_dest_size)
{
   uint32_t size_copied   = 0;
   int8_t  *temp_dest_ptr = destination_ptr;
   size_copied            = memscpy(destination_ptr, max_dest_size, filter_cfg, sizeof(fir_filter_cfg_t));
   temp_dest_ptr += size_copied;
   uint32_t *buffer_ptr = (uint32_t *)temp_dest_ptr;
   int16_t  *temp_ptr   = (int16_t *)(filter_cfg + size_copied);

   temp_ptr += 2; // this is added because we store coefficients after 4 bytes of gap to ensure 8 byte alignment

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
   size_copied += (num_of_taps * sizeof(uint32_t));
   return size_copied;
}

/*==========================================================================
  Function name: capi_fir_clean_up_memory
  Description : Function to clear up the lib memory and raise event
  ==========================================================================*/
void capi_fir_clean_up_memory(capi_fir_t *me_ptr)
{
   uint32_t channels_allocated = me_ptr->input_media_fmt[0].format.num_channels;
   if (NULL != me_ptr->fir_channel_lib)
   {
      for (uint32_t chan_num = 0; chan_num < channels_allocated; chan_num++)
      {
         if (NULL != me_ptr->fir_channel_lib[chan_num].fir_lib_instance.lib_mem_ptr)
         {
            posal_memory_free(me_ptr->fir_channel_lib[chan_num].fir_lib_instance.lib_mem_ptr);
            me_ptr->fir_channel_lib[chan_num].fir_lib_instance.lib_mem_ptr = NULL;
         }
      }
      posal_memory_free(me_ptr->fir_channel_lib);
      me_ptr->fir_channel_lib = NULL;
   }
   capi_fir_raise_process_event(me_ptr);
}

/*==========================================================================
  Function name: capi_fir_reset
  Description :  Function to reset lib memory
  ==========================================================================*/
capi_err_t capi_fir_reset(capi_fir_t *me_ptr)
{
   capi_err_t              capi_result        = CAPI_EOK;
   capi_fir_channel_lib_t *fir_channel_lib    = me_ptr->fir_channel_lib;
   uint16_t                dummy_var          = 0;
   uint32_t                channels_allocated = me_ptr->input_media_fmt[0].format.num_channels;
   if (NULL != fir_channel_lib)
   {
      for (uint32_t chan_num = 0; chan_num < channels_allocated; chan_num++)
      {
         if (NULL != fir_channel_lib[chan_num].fir_lib_instance.lib_mem_ptr)
         {
            if (FIR_SUCCESS != fir_set_param(&(fir_channel_lib[chan_num].fir_lib_instance),
                                             FIR_PARAM_RESET,
                                             (int8 *)&(dummy_var),
                                             sizeof(fir_config_struct_t)))
            {
               FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Reset failed for chan %lu", chan_num);
               CAPI_SET_ERROR(capi_result, CAPI_EFAILED);
            }
         }
      }
   }
   return capi_result;
}

/*=========================================================================
   Function name: capi_fir_allocate_req_config_size
   Description : Function to allocate memory required for
                 structures of lib,memory,static param,config params.  .
   ========================================================================*/
capi_err_t capi_fir_allocate_per_channel_lib_struct(capi_fir_t *me_ptr, uint32_t num_chan)
{
   uint32_t total_size_required = sizeof(capi_fir_channel_lib_t) * num_chan;

   int8_t *temp_ptr;

   // Allocate the memory required
   temp_ptr = (int8_t *)posal_memory_malloc(total_size_required, (POSAL_HEAP_ID)me_ptr->heap_info.heap_id);

   if (NULL == temp_ptr)
   {
      FIR_MSG(me_ptr->miid, DBG_FATAL_PRIO, "No memory to create lib structures.Required memory %lu bytes\n",
             total_size_required);
      return CAPI_ENOMEMORY;
   }

   memset(temp_ptr, 0, total_size_required);

   // for lib instance
   me_ptr->fir_channel_lib = (capi_fir_channel_lib_t *)temp_ptr;
   return CAPI_EOK;
}

/*=========================================================================
  Function name: capi_fir_lib_set_enable_param
  Description : Function to set enable/disable param to the library.
 ==========================================================================*/

capi_err_t capi_fir_lib_set_enable_param(capi_fir_t *me_ptr)
{
   capi_err_t capi_result = CAPI_EOK;
   if (NULL != me_ptr->fir_channel_lib)
   {
      capi_fir_channel_lib_t *const fir_channel_lib = me_ptr->fir_channel_lib;
      FIR_RESULT                    lib_result;
      fir_feature_mode_t            enable_flag;
      uint32_t                      channels_allocated = me_ptr->input_media_fmt[0].format.num_channels;
      for (uint32_t chan_num = 0; chan_num < channels_allocated; chan_num++)
      {
         fir_lib_t lib_instance = fir_channel_lib[chan_num].fir_lib_instance;
         if (NULL != lib_instance.lib_mem_ptr)
         {
            enable_flag = (me_ptr->is_fir_enabled) ? 1 : 0;
            lib_result =
               fir_set_param(&lib_instance, FIR_PARAM_FEATURE_MODE, (int8 *)&enable_flag, sizeof(fir_feature_mode_t));
            if (FIR_SUCCESS != lib_result)
            {
               FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Enable SetParam failed for chan - %lu with error code %u",
                      chan_num,
                      lib_result);
               CAPI_SET_ERROR(capi_result, CAPI_EFAILED);
            }
         }
      }
   }
   return capi_result;
}

/*=================================================================================
  Function name: capi_fir_set_static_params
  Description : Function to populate static parameters.
 ==================================================================================*/
void capi_fir_set_static_params(capi_fir_t *me_ptr)
{
   uint64_t                           channel_map;
   param_id_fir_filter_max_tap_cfg_t *fir_max_tap_len  = me_ptr->cache_fir_max_tap;
   int8_t *                           temp_tap_cfg_ptr = (int8_t *)me_ptr->cache_fir_max_tap;
   uint32_t                           num_cfg          = fir_max_tap_len->num_config;
   temp_tap_cfg_ptr += sizeof(param_id_fir_filter_max_tap_cfg_t);

   fir_filter_max_tap_length_cfg_t *const fir_max_tap_cfg = (fir_filter_max_tap_length_cfg_t *)temp_tap_cfg_ptr;

   capi_fir_channel_lib_t *fir_channel_lib    = me_ptr->fir_channel_lib;
   uint32_t                channels_allocated = me_ptr->input_media_fmt[0].format.num_channels;

   for (uint32_t chan_num = 0; (chan_num < channels_allocated) && (chan_num < CAPI_MAX_CHANNELS_V2); chan_num++)
   {
      fir_channel_lib[chan_num].fir_static_variables.max_num_taps  = 0;
      fir_channel_lib[chan_num].fir_static_variables.data_width    = me_ptr->input_media_fmt[0].format.bits_per_sample;
      fir_channel_lib[chan_num].fir_static_variables.sampling_rate = me_ptr->input_media_fmt[0].format.sampling_rate;
      fir_channel_lib[chan_num].fir_static_variables.frame_size    = CAPI_MAX_PROCESS_FRAME_SIZE;

      channel_map = ((uint64_t)1) << me_ptr->input_media_fmt[0].channel_type[chan_num];

      for (uint32_t count = 0; count < num_cfg; count++)
      {
         if (channel_map & (capi_fir_calculate_channel_mask(fir_max_tap_cfg[count].channel_mask_lsb,
                                                            fir_max_tap_cfg[count].channel_mask_msb)))
         {

            fir_channel_lib[chan_num].fir_static_variables.max_num_taps = fir_max_tap_cfg[count].fir_max_tap_length;
            break;
         }
      }
   }
}

/*===============================================================================
  Function name: capi_fir_cache_max_tap_payload
  Description : Function to cache max tap length payload.
  ===============================================================================*/

capi_err_t capi_fir_cache_max_tap_payload(capi_fir_t *me_ptr, int8_t *const payload_ptr, const uint32_t size_of_payload)
{
   if (me_ptr->cache_fir_max_tap_size != size_of_payload)
   {
      if (NULL != me_ptr->cache_fir_max_tap)
      {
         posal_memory_free(me_ptr->cache_fir_max_tap);
         me_ptr->cache_fir_max_tap      = NULL;
         me_ptr->cache_fir_max_tap_size = 0;
      }

      int8_t *temp_max_tap_cache_ptr =
         (int8_t *)posal_memory_malloc(size_of_payload, (POSAL_HEAP_ID)me_ptr->heap_info.heap_id);
      if (NULL == temp_max_tap_cache_ptr)
      {
         FIR_MSG(me_ptr->miid, DBG_FATAL_PRIO, "No memory to cache fir_filter_max tap config.Requires %lu bytes",
                size_of_payload);
         return CAPI_ENOMEMORY;
      }
      me_ptr->cache_fir_max_tap = (param_id_fir_filter_max_tap_cfg_t *)temp_max_tap_cache_ptr;
   }

   memscpy(me_ptr->cache_fir_max_tap, size_of_payload, payload_ptr, size_of_payload);
   me_ptr->cache_fir_max_tap_size = size_of_payload;
   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Cached fir_filter_max tap config of %lu bytes", size_of_payload);
   return CAPI_EOK;
}

/**
 * Checks whether the max tap length payload has changed. The tools team always sends a max tap length config
 * along with the filter coeff config. During runtime, resetting the max tap length causes a pop noise so we
 * check whether there has been a genuine change in the max tap length config prior to caching and resetting the
 * library based on the new config.
 */
bool_t capi_fir_has_max_tap_payload_changed(capi_fir_t *me_ptr, int8_t *const payload_ptr)
{
   bool_t result = false;

   if (NULL == me_ptr->cache_fir_max_tap)
   {
      return true;
   }

   param_id_fir_filter_max_tap_cfg_t *new_fir_max_tap_len  = (param_id_fir_filter_max_tap_cfg_t *)payload_ptr;
   int8_t *                           new_temp_tap_cfg_ptr = (int8_t *)payload_ptr;
   uint32_t                           new_num_cfg          = new_fir_max_tap_len->num_config;
   new_temp_tap_cfg_ptr += sizeof(param_id_fir_filter_max_tap_cfg_t);

   fir_filter_max_tap_length_cfg_t *const new_fir_max_tap_cfg = (fir_filter_max_tap_length_cfg_t *)new_temp_tap_cfg_ptr;

   uint64_t                           channel_map;
   param_id_fir_filter_max_tap_cfg_t *fir_max_tap_len  = me_ptr->cache_fir_max_tap;
   int8_t *                           temp_tap_cfg_ptr = (int8_t *)me_ptr->cache_fir_max_tap;
   uint32_t                           num_cfg          = fir_max_tap_len->num_config;
   temp_tap_cfg_ptr += sizeof(param_id_fir_filter_max_tap_cfg_t);

   fir_filter_max_tap_length_cfg_t *const fir_max_tap_cfg = (fir_filter_max_tap_length_cfg_t *)temp_tap_cfg_ptr;

   if (num_cfg != new_num_cfg)
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Max tap length config change: cached num cfg: %d  new num cfg: %d",
             num_cfg,
             new_num_cfg);
      return true;
   }

   for (uint8_t i = 0; i < new_num_cfg; ++i)
   {
      bool_t flag = false;
      channel_map = capi_fir_calculate_channel_mask(new_fir_max_tap_cfg[i].channel_mask_lsb,
                                                    new_fir_max_tap_cfg[i].channel_mask_msb);

      for (uint8_t j = 0; j < num_cfg; ++j)
      {
         if (channel_map ==
             capi_fir_calculate_channel_mask(fir_max_tap_cfg[j].channel_mask_lsb, fir_max_tap_cfg[j].channel_mask_msb))
         {
            if (new_fir_max_tap_cfg[i].fir_max_tap_length == fir_max_tap_cfg[j].fir_max_tap_length)
            {
               // match found, so we can stop searching the existing config
               flag = true;
               break;
            }
            else
            {
               // the channel map is the same but the config is different
               FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Max tap length config change: channel map: 0x%llx current tap length: %d new tap "
                      "length: %d",
                      channel_map,
                      fir_max_tap_cfg[j].fir_max_tap_length,
                      new_fir_max_tap_cfg[i].fir_max_tap_length);
               return true;
            }
         }
      }

      if (!flag)
      {
         // flag is never set, so the config hasn't been seen before
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Max tap length config change: new channel map configured: 0x%llx  tap length: %d ",
                channel_map,
                new_fir_max_tap_cfg[i].fir_max_tap_length);
         result = true;
         break;
      }
   }

   return result;
}

/*===============================================================
  Function name: capi_fir_cache_filter_coeff_payload
  Description : Function to cache filter coefficients payload.
  ===============================================================*/
capi_err_t capi_fir_cache_filter_coeff_payload(capi_fir_t *me_ptr, int8_t *const payload_ptr)
{
   capi_err_t               capi_result            = CAPI_EOK;
   param_id_fir_filter_config_t  *fir_cfg_state    = (param_id_fir_filter_config_t *)payload_ptr;
   const uint32_t                 num_cfg          = fir_cfg_state->num_config;
   int8_t                        *temp_fir_cfg     = payload_ptr + sizeof(param_id_fir_filter_config_t);
   fir_filter_cfg_t *const        fir_coeff_cfg    = (fir_filter_cfg_t *)temp_fir_cfg;
   int8_t                       **fir_cfg_ptr      = NULL;
   param_id_fir_filter_config_t **cached_coeff_ptr = NULL;
   uint32_t                      *cfg_size		   = NULL;

   if (CAPI_CURR_CFG == me_ptr->config_type)
   {
      fir_cfg_ptr      = &me_ptr->cache_original_fir_coeff_cfg;
      cached_coeff_ptr = &me_ptr->cache_fir_coeff_cfg;
      cfg_size         = &me_ptr->cache_fir_coeff_cfg_size;
   }
   else if (CAPI_NEXT_CFG == me_ptr->config_type)
   {
      fir_cfg_ptr      = &me_ptr->cache_original_next_fir_coeff_cfg;
      cached_coeff_ptr = &me_ptr->cache_next_fir_coeff_cfg;
      cfg_size         = &me_ptr->cache_next_fir_coeff_cfg_size;
   }
   else if (CAPI_QUEUE_CFG == me_ptr->config_type)
   {
      fir_cfg_ptr      = &me_ptr->cache_original_queue_fir_coeff_cfg;
      cached_coeff_ptr = &me_ptr->cache_queue_fir_coeff_cfg;
      cfg_size         = &me_ptr->cache_queue_fir_coeff_cfg_size;
   }
   else
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Did not receive config type while caching filter coeff. Received %lu",
    		  me_ptr->config_type);
      return CAPI_EUNSUPPORTED;
   }
   // calculate memory_required to cache
   uint32_t required_cache_size = capi_fir_calculate_cache_size_for_coeff(me_ptr->miid, fir_coeff_cfg, num_cfg);

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
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "No memory to cache the filter coefficients cfg of %lu bytes",
                required_cache_size);
         return CAPI_ENOMEMORY;
      }
      *fir_cfg_ptr      = temp_ptr;
      *cached_coeff_ptr = (param_id_fir_filter_config_t *)temp_ptr;
      *cfg_size         = required_cache_size;
   }
   capi_result |= capi_fir_copy_fir_coeff_cfg(me_ptr, payload_ptr);
   if (CAPI_FAILED(capi_result))
   {
      return capi_result;
   }
   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Cached filter coeff config of %lu bytes", *cfg_size);
   return CAPI_EOK;
}

/*==============================================================================
  Function name: capi_fir_validate_fir_coeff_payload_size
  Description : Function to validate filter coefficient payload size.
  ==============================================================================*/
capi_err_t capi_fir_validate_fir_coeff_payload_size(uint32_t          miid,
													fir_filter_cfg_t *filter_coeff_cfg_ptr,
                                                    uint32_t          num_cfg,
                                                    uint32_t          param_size,
                                                    uint32_t         *required_size)
{
   *required_size = sizeof(param_id_fir_filter_config_t);
   if (param_size < *required_size)
   {
      return CAPI_EBADPARAM;
   }

   int8_t *temp_ptr = (int8_t *)filter_coeff_cfg_ptr;
   for (uint32_t count = 0; count < num_cfg; count++)
   {
      if ((*required_size + sizeof(fir_filter_cfg_t)) > param_size)
      {
         FIR_MSG(miid, DBG_ERROR_PRIO, "Failed while validating required size for config #%lu", count);
         return CAPI_EBADPARAM;
      }
      else
      {
         fir_filter_cfg_t *temp_coeff_cfg_ptr = (fir_filter_cfg_t *)temp_ptr;
         uint32_t          config_size        = capi_fir_coeff_config_payload_increment_size(temp_coeff_cfg_ptr);
         temp_ptr += config_size;
         *required_size += config_size;
      }
   }

   if (*required_size > param_size)
   {
      return CAPI_EBADPARAM;
   }
   return CAPI_EOK;
}

/*===============================================================================
  Function name: capi_fir_calculate_cache_size_for_coeff
  Description : Function to calculate size required to cache filter coefficients.
  ===============================================================================*/
uint32_t capi_fir_calculate_cache_size_for_coeff(uint32_t miid, fir_filter_cfg_t *filter_coeff_cfg_ptr, uint32_t num_cfg)
{
   uint32_t required_size   = CAPI_FIR_ALIGN_8_BYTE(sizeof(param_id_fir_filter_config_t));
   int8_t  *temp_ptr        = (int8_t *)filter_coeff_cfg_ptr;
   uint32_t alignment_bytes = 0;
   for (uint32_t count = 0; count < num_cfg; count++)
   {

      fir_filter_cfg_t *temp_coeff_cfg_ptr = (fir_filter_cfg_t *)temp_ptr;
      uint32_t          coeff_width        = (temp_coeff_cfg_ptr->coef_width) >> 3;
      if ((sizeof(uint16_t) != coeff_width) && (sizeof(uint32_t) != coeff_width))
      {
         FIR_MSG(miid, DBG_HIGH_PRIO, "Received incorrect bit width of %lu for coefficients ", coeff_width * 8);
         return 0;
      }

      alignment_bytes =
         ((temp_coeff_cfg_ptr->num_taps % 4) ? (4 - (temp_coeff_cfg_ptr->num_taps % 4)) * (coeff_width) : 0) + 4;
      // filters taps to multiple of 4 and one extra 4 bytes to make coefficients to 8 byte allign


      required_size += (sizeof(fir_filter_cfg_t) + (coeff_width * temp_coeff_cfg_ptr->num_taps)) + alignment_bytes;
      temp_ptr += capi_fir_coeff_config_payload_increment_size(temp_coeff_cfg_ptr);
   }
   return required_size;
}

/*========================================================================
  Function name: capi_fir_copy_fir_coeff_cfg
  Description : Function to cache fir coefficients cfg.
  ========================================================================*/
capi_err_t capi_fir_copy_fir_coeff_cfg(capi_fir_t *me_ptr, int8_t *payload_ptr)
{
   int8_t  *ptr_cached_coeff = NULL;
   uint32_t destination_size = 0;
   if (CAPI_CURR_CFG == me_ptr->config_type)
   {
      ptr_cached_coeff = (int8_t *)me_ptr->cache_fir_coeff_cfg;
      destination_size = me_ptr->cache_fir_coeff_cfg_size;
   }
   else if (CAPI_NEXT_CFG == me_ptr->config_type)
   {
      ptr_cached_coeff = (int8_t *)me_ptr->cache_next_fir_coeff_cfg;
      destination_size = me_ptr->cache_next_fir_coeff_cfg_size;
   }
   else if (CAPI_QUEUE_CFG == me_ptr->config_type)
   {
      ptr_cached_coeff = (int8_t *)me_ptr->cache_queue_fir_coeff_cfg;
      destination_size = me_ptr->cache_queue_fir_coeff_cfg_size;
   }
   else
   {
      FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Did not receive expected config type while copying coeff config. Recieved %lu",
    		 me_ptr->config_type);
	  return CAPI_EUNSUPPORTED;
   }
   const uint32_t num_cfg = ((param_id_fir_filter_config_t *)payload_ptr)->num_config;
   uint32_t copied_size = 0;
   // Copy param_id_fir_filter_config_t
   uint32_t size = sizeof(param_id_fir_filter_config_t);
   copied_size   = memscpy(ptr_cached_coeff, destination_size, payload_ptr, size);
   destination_size -= copied_size;

   ptr_cached_coeff += CAPI_FIR_ALIGN_8_BYTE(size);
   payload_ptr += size;
   int32_t i = 0;

   for (uint16_t count = 0; count < num_cfg; count++)
   {
      fir_filter_cfg_t *fir_coef_cfg = (fir_filter_cfg_t *)payload_ptr;
      // Copy fir_filter_cfg_t
      size        = sizeof(fir_filter_cfg_t);
      copied_size = memscpy(ptr_cached_coeff, destination_size, payload_ptr, size);
      destination_size -= copied_size;
      ptr_cached_coeff += size;
      payload_ptr += size;
      // Copy Coefficient
      if (16 == fir_coef_cfg->coef_width)
      {
         int16_t *dest_ptr = (int16_t *)ptr_cached_coeff;
         int32_t *src_ptr  = (int32_t *)payload_ptr;

         for (i = 2; i > 0; i--)
         {
            *dest_ptr++ = 0;
         }
         destination_size -= (sizeof(int16_t) * 2);

         ptr_cached_coeff += (fir_coef_cfg->num_taps * sizeof(uint16_t)) +
                             4;
#ifdef QDSP6_ASM_OPT_FIR_FILTER
         ////reversing the coefficients and storing in provided memory
         dest_ptr = (int16_t *)ptr_cached_coeff;
         dest_ptr--;
#endif
         for (uint16_t tap_count = 0; tap_count < fir_coef_cfg->num_taps; tap_count++)
         {
            *dest_ptr = (int16_t)(*src_ptr);
#ifdef QDSP6_ASM_OPT_FIR_FILTER
            dest_ptr--;
#else
            dest_ptr++;
#endif
            src_ptr++;
         }
         destination_size -= fir_coef_cfg->num_taps * sizeof(int16_t);

         dest_ptr = (int16_t *)ptr_cached_coeff;
         for (i = ((fir_coef_cfg->num_taps % 4) ? (4 - (fir_coef_cfg->num_taps % 4)) : 0); i > 0; i--)
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
         int32_t *dest_ptr = (int32_t *)ptr_cached_coeff;
         int32_t *src_temp = (int32_t *)payload_ptr;
         *dest_ptr++       = 0;
         size              = fir_coef_cfg->num_taps * sizeof(int32_t);
         ptr_cached_coeff += size + 4; // TBR amith
         dest_ptr = (int32_t *)ptr_cached_coeff;
         dest_ptr--;
         for (uint16_t tap_count = 0; tap_count < fir_coef_cfg->num_taps; tap_count++)
         {
            *dest_ptr = (*src_temp); // reversing the coefficients and storing in provided memory
            dest_ptr--;
            src_temp++;
         }
         destination_size -= size + 4; // TBR Amith
         dest_ptr = (int32_t *)ptr_cached_coeff;
         for (i = ((fir_coef_cfg->num_taps % 4) ? (4 - (fir_coef_cfg->num_taps % 4)) : 0); i > 0; i--)
         {
            *dest_ptr++ = 0; // zero padding the extra coefficients to make filter taps as multiple of 4
            destination_size--;
         }
         ptr_cached_coeff = (int8_t *)dest_ptr;
         payload_ptr      = (int8_t *)src_temp;
#else
         size        = fir_coef_cfg->num_taps * sizeof(uint32_t);
         copied_size = memscpy(ptr_cached_coeff, destination_size, payload_ptr, size);
         destination_size -= copied_size;
         ptr_cached_coeff += size;
         payload_ptr += size;

         destination_size -= size + 4;
         int32_t *dest_ptr = (int32_t *)ptr_cached_coeff;
         for (i = ((fir_coef_cfg->num_taps % 4) ? (4 - (fir_coef_cfg->num_taps % 4)) : 0); i > 0; i--)
         {
            *dest_ptr++ = 0; // zero padding the extra coefficients to make filter taps as multiple of 4
            destination_size--;
         }
         ptr_cached_coeff = (int8_t *)dest_ptr;
#endif
      }
   }
   return CAPI_EOK;
}

/*========================================================================
  Function name: capi_fir_clean_up_cached_coeff_memory
  Description : Function to free the cached filter coefficient memory
  ========================================================================*/
void capi_fir_clean_up_cached_coeff_memory(capi_fir_t *me_ptr)
{
   if ((CAPI_CURR_CFG == me_ptr->config_type) && (NULL != me_ptr->cache_original_fir_coeff_cfg))
   {
      posal_memory_aligned_free(me_ptr->cache_original_fir_coeff_cfg);
      me_ptr->cache_original_fir_coeff_cfg = NULL;
      me_ptr->cache_fir_coeff_cfg          = NULL;
      me_ptr->cache_fir_coeff_cfg_size     = 0;
   }
   else if ((CAPI_NEXT_CFG == me_ptr->config_type) && (NULL != me_ptr->cache_original_next_fir_coeff_cfg))
   {
      posal_memory_aligned_free(me_ptr->cache_original_next_fir_coeff_cfg);
      me_ptr->cache_original_next_fir_coeff_cfg = NULL;
      me_ptr->cache_next_fir_coeff_cfg          = NULL;
      me_ptr->cache_next_fir_coeff_cfg_size     = 0;
   }
   else if ((CAPI_QUEUE_CFG == me_ptr->config_type) && (NULL != me_ptr->cache_original_queue_fir_coeff_cfg))
   {
      posal_memory_aligned_free(me_ptr->cache_original_queue_fir_coeff_cfg);
      me_ptr->cache_original_queue_fir_coeff_cfg = NULL;
      me_ptr->cache_queue_fir_coeff_cfg          = NULL;
      me_ptr->cache_queue_fir_coeff_cfg_size     = 0;
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

/*========================================================================
  Function name: capi_fir_calculate_delay_of_filter
  Description : Function to calculate the delay of filter in us.
  ========================================================================*/
uint32_t capi_fir_calculate_delay_of_filter(capi_fir_t *me_ptr)
{
   uint32_t delay_in_us      = 0;
   uint32_t delay_in_samples = 0;
   if (NULL == me_ptr->cache_fir_coeff_cfg)
   {
      return delay_in_us;
   }

   param_id_fir_filter_config_t *temp_ptr         = me_ptr->cache_fir_coeff_cfg;
   int8_t                       *ptr_to_coeff_cfg = (int8_t *)me_ptr->cache_fir_coeff_cfg;

   ptr_to_coeff_cfg += CAPI_FIR_ALIGN_8_BYTE(sizeof(param_id_fir_filter_config_t));
   uint32_t          num_cfg          = temp_ptr->num_config;
   fir_filter_cfg_t *filter_cfg_ptr   = (fir_filter_cfg_t *)ptr_to_coeff_cfg;
   uint32_t          calculated_delay = 0;
   for (uint32_t count = 0; count < num_cfg; count++)
   {
      calculated_delay = (0xFFFFFFFF == filter_cfg_ptr->filter_delay_in_samples)
                            ? (filter_cfg_ptr->num_taps >> 1)
                            : filter_cfg_ptr->filter_delay_in_samples;
      delay_in_samples = (delay_in_samples > calculated_delay) ? delay_in_samples : calculated_delay;
      ptr_to_coeff_cfg += capi_fir_coeff_config_cache_increment_size(filter_cfg_ptr);
      filter_cfg_ptr = (fir_filter_cfg_t *)ptr_to_coeff_cfg;
   }
   delay_in_us = (delay_in_samples)*1000000 / (me_ptr->input_media_fmt[0].format.sampling_rate);
   return delay_in_us;
}

/*========================================================================
  Function name: capi_fir_clean_up_lib_memory
  Description : Function to clean up the library memory.
  ========================================================================*/
void capi_fir_clean_up_lib_memory(capi_fir_t *me_ptr)
{
   uint32_t channels_allocated = me_ptr->input_media_fmt[0].format.num_channels;
   if (NULL != me_ptr->fir_channel_lib)
   {
      for (uint32_t chan_num = 0; chan_num < channels_allocated; chan_num++)
      {
         if (NULL != me_ptr->fir_channel_lib[chan_num].fir_lib_instance.lib_mem_ptr)
         {
            posal_memory_free(me_ptr->fir_channel_lib[chan_num].fir_lib_instance.lib_mem_ptr);
            me_ptr->fir_channel_lib[chan_num].fir_lib_instance.lib_mem_ptr = NULL;
         }
      }
      posal_memory_free(me_ptr->fir_channel_lib);
      me_ptr->fir_channel_lib = NULL;
   }
}
