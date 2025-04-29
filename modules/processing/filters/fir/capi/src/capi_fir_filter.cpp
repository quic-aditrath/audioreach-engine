/* ======================================================================== */
/**
  @file capi_fir_filter.cpp

  C++ source file to implement the functions for
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
#include "audio_basic_op.h"

/*------------------------------------------------------------------------
 * Macro definitions and Structure definitions
 * -----------------------------------------------------------------------*/

/*------------------------------------------------------------------------
 * Static declarations
 * -----------------------------------------------------------------------*/

static capi_err_t capi_fir_process(capi_t *            _pif,
                                         capi_stream_data_t *input[],
                                         capi_stream_data_t *output[]);

static capi_err_t capi_fir_end(capi_t *_pif);

static capi_err_t capi_fir_set_param(capi_t *                _pif,
                                           uint32_t                   param_id,
                                           const capi_port_info_t *port_info_ptr,
                                           capi_buf_t *            params_ptr);

static capi_err_t capi_fir_get_param(capi_t *                _pif,
                                           uint32_t                   param_id,
                                           const capi_port_info_t *port_info_ptr,
                                           capi_buf_t *            params_ptr);

static capi_err_t capi_fir_set_properties(capi_t *_pif, capi_proplist_t *props_ptr);

static capi_err_t capi_fir_get_properties(capi_t *_pif, capi_proplist_t *props_ptr);

static const capi_vtbl_t fir_vtbl = { capi_fir_process,        capi_fir_end,
                                         capi_fir_set_param,      capi_fir_get_param,
                                         capi_fir_set_properties, capi_fir_get_properties };

/*=====================================================================
  Function name: capi_fir_get_static_properties
  DESCRIPTION: Function to get the static properties of FIR module
 =====================================================================*/
capi_err_t capi_fir_get_static_properties(capi_proplist_t *init_set_properties,
                                                capi_proplist_t *static_properties)
{
   capi_err_t capi_result = CAPI_EOK;

   if (NULL != static_properties)
   {
      capi_result |= capi_fir_process_get_properties((capi_fir_t *)NULL, static_properties);
      }
   else
   {
      FIR_MSG(MIID_UNKNOWN, DBG_ERROR_PRIO, "Get static properties received NULL pointer, 0x%p",
             static_properties);
   }

   return capi_result;
}

/*=====================================================================
  Function name: capi_fir_init
  DESCRIPTION: Function to init the FIR module
 =====================================================================*/

capi_err_t capi_fir_init(capi_t *_pif, capi_proplist_t *init_set_properties)
{
   capi_err_t capi_result = CAPI_EOK;

   if (NULL == _pif || NULL == init_set_properties)
   {
      FIR_MSG(MIID_UNKNOWN, DBG_ERROR_PRIO, "Init received bad pointer, %p, %p", _pif, init_set_properties);
      return CAPI_EBADPARAM;
   }

   capi_fir_t *me_ptr = (capi_fir_t *)_pif;
   memset(me_ptr, 0, sizeof(capi_fir_t));

   me_ptr->vtbl.vtbl_ptr             = &fir_vtbl;
   me_ptr->heap_info.heap_id         = POSAL_HEAP_DEFAULT;
   me_ptr->config_type               = CAPI_CURR_CFG;
   me_ptr->xfade_flag                = FALSE;
   me_ptr->is_xfade_cfg_pending      = FALSE;
   me_ptr->combined_crossfade_status = 0;
   me_ptr->is_module_in_voice_graph  = FALSE; // default to Audio graph
   me_ptr->frame_size_in_samples     = CAPI_MAX_PROCESS_FRAME_SIZE;
   me_ptr->cfg_version               = DEFAULT;
   capi_fir_init_events(me_ptr);

   capi_result = capi_fir_process_set_properties(me_ptr, init_set_properties);

   // suppress unsupported errors as module might not support some properties
   // which might get added in future
   capi_result ^= (capi_result & CAPI_EUNSUPPORTED);

   if (CAPI_FAILED(capi_result))
   {
      FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Init Set Property Failed");
      return capi_result;
   }

   capi_fir_raise_process_event(me_ptr);

   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Init done!");
   return capi_result;
}

/*=====================================================================
  Function name: capi_fir_get_properties
  DESCRIPTION: Function to get properties of the FIR module
 =====================================================================*/
static capi_err_t capi_fir_get_properties(capi_t *_pif, capi_proplist_t *props_ptr)
{
   capi_err_t capi_result = CAPI_EOK;
   if (NULL == _pif || NULL == props_ptr)
   {
      FIR_MSG(MIID_UNKNOWN, DBG_ERROR_PRIO, "Get properties received NULL pointer");
      return CAPI_EBADPARAM;
   }
   capi_fir_t *me_ptr = (capi_fir_t *)_pif;
   capi_result        = capi_fir_process_get_properties(me_ptr, props_ptr);

   return capi_result;
}

/*=====================================================================
  Function name: capi_fir_set_properties
  DESCRIPTION: Function to set properties to the FIR module
  =====================================================================*/
static capi_err_t capi_fir_set_properties(capi_t *_pif, capi_proplist_t *props_ptr)
{
   capi_err_t capi_result = CAPI_EOK;
   if (NULL == _pif || NULL == props_ptr)
   {
      FIR_MSG(MIID_UNKNOWN, DBG_ERROR_PRIO, "Set properties received NULL pointer");
      return CAPI_EBADPARAM;
   }
   capi_fir_t *me_ptr = (capi_fir_t *)_pif;
   capi_result |= capi_fir_process_set_properties(me_ptr, props_ptr);

   return capi_result;
}

/*=====================================================================
  Function name: capi_fir_process
  Description :  Data Process function to process an input buffer
                 and generate an output buffer.
 =====================================================================*/
capi_err_t capi_fir_process(capi_t *_pif, capi_stream_data_t *input[], capi_stream_data_t *output[])
{
   FIR_RESULT lib_result  = FIR_SUCCESS;
   capi_err_t capi_result = CAPI_EOK;

   capi_fir_t *                  me_ptr       = (capi_fir_t *)_pif;
   uint32_t                      num_channels = me_ptr->input_media_fmt[0].format.num_channels;
   capi_fir_channel_lib_t *const lib_instance = me_ptr->fir_channel_lib;

   POSAL_ASSERT(me_ptr);
   POSAL_ASSERT(input[0]);
   POSAL_ASSERT(output[0]);
   POSAL_ASSERT(num_channels <= (uint16)(input[0]->bufs_num));
   POSAL_ASSERT(num_channels <= (uint16)(output[0]->bufs_num));

   int32_t byte_sample_convert = (16 == me_ptr->input_media_fmt[0].format.bits_per_sample) ? 1 : 2;
   // one port only, one input and one output
   uint32_t port                       = 0;
   uint32_t sample_offset              = 0;
   uint32_t data_offset                = 0;
   uint32_t max_lib_samples_to_process = 0;

   int32_t  input_num_samples         = input[port]->buf_ptr[0].actual_data_len >> (byte_sample_convert);
   int32_t  output_num_samples        = output[port]->buf_ptr[0].max_data_len >> (byte_sample_convert);
   int32_t  samples_to_process        = s32_min_s32_s32(input_num_samples, output_num_samples);
   uint32_t is_crossfade_flag_updated = 0;
   bool_t   is_lib_enabled            = capi_fir_filter_lib_is_enabled(me_ptr);
   me_ptr->combined_crossfade_status  = 0;
   me_ptr->capi_fir_v2_cfg.combined_crossfade_status  = 0;

   for (uint32_t chan_num = 0; chan_num < num_channels; chan_num++)
   {
      sample_offset = 0;
      data_offset   = 0;

      if ((is_lib_enabled) && (NULL != lib_instance[chan_num].fir_lib_instance.lib_mem_ptr))
      {
         while (sample_offset < samples_to_process)
         {
            max_lib_samples_to_process = MIN((samples_to_process - sample_offset), me_ptr->frame_size_in_samples);

            lib_result = fir_module_process(&(lib_instance[chan_num].fir_lib_instance),
                                            output[port]->buf_ptr[chan_num].data_ptr + data_offset,
                                            input[port]->buf_ptr[chan_num].data_ptr + data_offset,
                                            max_lib_samples_to_process);

            if (FIR_SUCCESS != lib_result)
            {
               FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Library process failed for chan %lu", chan_num);
               CAPI_SET_ERROR(capi_result, CAPI_EFAILED);
               for (uint32_t ch = 0; ch < chan_num; ch++)
               {
                  output[port]->buf_ptr[ch].actual_data_len = 0;
                  input[port]->buf_ptr[ch].actual_data_len  = 0;
               }
               return capi_result;
            }
            sample_offset += max_lib_samples_to_process;
            data_offset += ((max_lib_samples_to_process) << (byte_sample_convert));
         }

         output[port]->buf_ptr[chan_num].actual_data_len = (samples_to_process << byte_sample_convert);
         input[port]->buf_ptr[chan_num].actual_data_len  = (samples_to_process << byte_sample_convert);

         // if there is an active transition for this ch in capi then only check if crossfade completed
         if (1 == lib_instance[chan_num].fir_transition_status_variables.flag)
         {
            // check if xfade completed for the specific channel
            capi_result |= capi_fir_check_combined_crossfade(me_ptr, chan_num);
            if (CAPI_FAILED(capi_result))
            {
               FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed to check crossfade status for channel %lu in process() with result %lu",chan_num, capi_result);
            }
            else
            {
               is_crossfade_flag_updated = 1; // this is to ensure we successfully got crossfade status
            }
         }
      }
      else
      {
         uint32_t bytes_copied = memscpy(output[port]->buf_ptr[chan_num].data_ptr,
                                         output[port]->buf_ptr[chan_num].max_data_len,
                                         input[port]->buf_ptr[chan_num].data_ptr,
                                         input[port]->buf_ptr[chan_num].actual_data_len);

         output[port]->buf_ptr[chan_num].actual_data_len = bytes_copied;
         input[port]->buf_ptr[chan_num].actual_data_len  = bytes_copied;
      }
   }

   capi_fir_process_set_pending_crossfade_config(me_ptr, is_crossfade_flag_updated);

   output[0]->flags.is_timestamp_valid = input[0]->flags.is_timestamp_valid;
   if (input[0]->flags.is_timestamp_valid)
   {
      output[0]->timestamp = input[0]->timestamp - (int64_t)me_ptr->event_config.delay_in_us;
   }

   return capi_result;
}

/*=====================================================================
  Function name: capi_fir_end
  DESCRIPTION: Free the library and cache memory
               This function also frees the virtual function table.
 =======================================================================*/
static capi_err_t capi_fir_end(capi_t *_pif)
{
   if (NULL == _pif)
   {
      return CAPI_EOK;
   }

   capi_fir_t *me_ptr = (capi_fir_t *)(_pif);

   capi_fir_free_memory(me_ptr);

   me_ptr->vtbl.vtbl_ptr = NULL;

   FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "End done");
   return CAPI_EOK;
}

/*=====================================================================
  Function name: capi_fir_set_param
  DESCRIPTION: Sets the corresponding parameters based on the PARAM ID
  =====================================================================*/
capi_err_t capi_fir_set_param(capi_t *                _pif,
                              uint32_t                param_id,
                              const capi_port_info_t *port_info_ptr,
                              capi_buf_t *            params_ptr)
{
   capi_err_t        capi_result = CAPI_EOK;
   capi_fir_t *      me_ptr      = (capi_fir_t *)(_pif);
   const void *const param_ptr   = (void *)params_ptr->data_ptr;
   const uint32_t    param_size  = params_ptr->actual_data_len;

   if (NULL == param_ptr || NULL == me_ptr)
   {
      FIR_MSG(MIID_UNKNOWN, DBG_ERROR_PRIO, "Set param received bad pointer");
      return CAPI_EBADPARAM;
   }
   switch (param_id)
   {
      case PARAM_ID_MODULE_ENABLE:
      case INTF_EXTN_PARAM_ID_PERIOD:
	     break;
      case PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH:
      case PARAM_ID_FIR_FILTER_CONFIG:
      case PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG:
      {
    	 if((VERSION_V2 == me_ptr->cfg_version) || (TRUE == me_ptr->higher_channel_map_present))
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO,"SetParam 0x%lx failed as V2 config is already configured for the module. "
                   "Cannot set both V1 and V2 configs simultaneously for the module OR higher than 63 channel map present in IMF (0/1): %lu",
				   param_id,
				   me_ptr->higher_channel_map_present);
            return CAPI_EBADPARAM;
         }
         break;
      }
      case PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH_V2:
      case PARAM_ID_FIR_FILTER_CONFIG_V2:
      case PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG_V2:
      {
         if(VERSION_V1 == me_ptr->cfg_version)
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "SetParam 0x%lx failed as V1 config is already configured for the module. "
                   "Cannot set both V1 and V2 configs simultaneously for the module",param_id);
            return CAPI_EBADPARAM;
         }
         break;
      }
      default:
      {
         FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "SetParam, unsupported param ID 0x%x", (int)param_id);
         return CAPI_EUNSUPPORTED;
         break;
      }
   }
   switch (param_id)
   {
      case PARAM_ID_MODULE_ENABLE:
      {
         if (param_size < sizeof(param_id_module_enable_t))
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Enable SetParam 0x%lx, invalid param size %lx ",
                   param_id,
                   params_ptr->actual_data_len);
            capi_result = CAPI_ENEEDMORE;
            break;
         }
         param_id_module_enable_t fir_state = *((param_id_module_enable_t *)params_ptr->data_ptr);

         // Reset if module moves from enable to disable state
         if ((0 == fir_state.enable) && (1 == me_ptr->is_fir_enabled))
         {
            capi_fir_reset(me_ptr);
         }

         me_ptr->is_fir_enabled = fir_state.enable;
         capi_result            = capi_fir_lib_set_enable_param(me_ptr);
         capi_fir_raise_process_event(me_ptr);
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Enable Set Param Set Sucessfully : %u", me_ptr->is_fir_enabled);
         break;
      }

      case PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH:
      {
         if (param_size < sizeof(param_id_fir_filter_max_tap_cfg_t))
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Max Fir tap length SetParam 0x%lx, invalid param size %lx ",
                   param_id,
                   params_ptr->actual_data_len);
            capi_result = CAPI_ENEEDMORE;

            break;
         }
         param_id_fir_filter_max_tap_cfg_t *const fir_max_tap_len =
            ((param_id_fir_filter_max_tap_cfg_t *)params_ptr->data_ptr);
         int8_t *temp_tap_cfg_ptr = params_ptr->data_ptr;

         // check no of cfg and cross check with size
         const uint32_t num_cfg = fir_max_tap_len->num_config;
         if (num_cfg < 1 || num_cfg > 63)
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Received incorrect num_config parameter - %lu", num_cfg);
            return CAPI_EBADPARAM;
         }
         temp_tap_cfg_ptr += sizeof(param_id_fir_filter_max_tap_cfg_t);

         uint32_t req_payload_size =
            sizeof(param_id_fir_filter_max_tap_cfg_t) + (sizeof(fir_filter_max_tap_length_cfg_t) * num_cfg);
         if (param_size < req_payload_size)
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Max Fir tap length SetParam 0x%lx, invalid param size %lu ,required_size %lu",
                   param_id,
                   params_ptr->actual_data_len,
                   req_payload_size);
            return CAPI_EBADPARAM;
         }

         fir_filter_max_tap_length_cfg_t *const fir_max_tap_cfg = (fir_filter_max_tap_length_cfg_t *)temp_tap_cfg_ptr;

         // check if the channel masks received is proper or not
         capi_result = capi_fir_check_channel_map_max_tap_cfg(fir_max_tap_cfg, num_cfg);
         if (CAPI_FAILED(capi_result))
         {
            FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Received incorrect channel map for parameter id 0x%lx", param_id);
            return capi_result;
         }

         // cache the payload
         if (capi_fir_has_max_tap_payload_changed(me_ptr, params_ptr->data_ptr))
         {
            capi_result = capi_fir_cache_max_tap_payload(me_ptr, params_ptr->data_ptr, param_size);
            if (CAPI_FAILED(capi_result))
            {
               capi_fir_clean_up_memory(me_ptr);
               return capi_result;
            }
            me_ptr->cfg_version = VERSION_V1; // indicates V1 version is set now and can only allow V1 version
                                               // configs for upcoming set params

            // check and create library instances
            capi_result |= capi_fir_check_create_lib_instance(me_ptr, FALSE);
            if (CAPI_FAILED(capi_result))
            {
               return capi_result;
            }

            FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Max tap length Set Param set Successfully");
         }
         else
         {
            FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "No change in max tap length param config");
         }

         break;
      }
      case PARAM_ID_FIR_FILTER_CONFIG:
      {
         if (param_size < sizeof(param_id_fir_filter_config_t))
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "configuration SetParam 0x%lx, invalid param size %lx ",
                   param_id,
                   params_ptr->actual_data_len);
            capi_result |= CAPI_ENEEDMORE;
            break;
         }
         param_id_fir_filter_config_t *fir_cfg_state = ((param_id_fir_filter_config_t *)params_ptr->data_ptr);
         int8_t *                      fir_cfg_ptr   = params_ptr->data_ptr;

         //  validate received num of cfg
         const uint32_t num_cfg = fir_cfg_state->num_config;
         if (num_cfg < 1 || num_cfg > 63)
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Received incorrect num_config parameter - %lu", num_cfg);
            return CAPI_EBADPARAM;
         }

         fir_cfg_ptr += sizeof(param_id_fir_filter_config_t);

         fir_filter_cfg_t *const fir_coeff_cfg = (fir_filter_cfg_t *)fir_cfg_ptr;

         // validate received payload size
         uint32_t required_size = 0;
         capi_result = capi_fir_validate_fir_coeff_payload_size(me_ptr->miid, fir_coeff_cfg, num_cfg, param_size, &required_size);

         if (CAPI_FAILED(capi_result))
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Filter coeff SetParam 0x%lx, invalid param size %lu ,required_size %lu",
                   param_id,
                   params_ptr->actual_data_len,
                   required_size);
            return capi_result;
         }

         // validate the received channel maps
         capi_result = capi_fir_check_channel_map_coeff_cfg(me_ptr->miid, fir_coeff_cfg, num_cfg);

         if (CAPI_FAILED(capi_result))
         {
            return capi_result;
         }

         //  caching the configuration received

         int8_t *source_ptr = params_ptr->data_ptr;

         // if current cfg pointer is null : rcvd cfg for first time
         // OR lib not created OR crossfade is disabled for all channels then store in 1st config only
         if ((NULL == me_ptr->cache_original_fir_coeff_cfg) || (NULL == me_ptr->fir_channel_lib) ||
             (FALSE == me_ptr->xfade_flag))
         {
            me_ptr->config_type = CAPI_CURR_CFG; // do this check create lib instance
            FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Updating the current filter config with the new configuration");

            capi_result = capi_fir_cache_filter_coeff_payload(me_ptr, source_ptr);
            if (CAPI_EBADPARAM == capi_result)
            {
               return capi_result;
            }

            if (CAPI_ENOMEMORY == capi_result)
            {
               capi_fir_clean_up_memory(me_ptr);
               return capi_result;
            }

            me_ptr->size_req_for_get_coeff = params_ptr->actual_data_len;
         }
         // if current cfg pointer is valid and new config pointer is Null : rcvd 2nd config
         // There is no active cross-fade at this point
         else if (NULL == me_ptr->cache_original_next_fir_coeff_cfg)
         {
            me_ptr->config_type = CAPI_NEXT_CFG;
            FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Caching the configuration in the NEXT filter config ");

            capi_result = capi_fir_cache_filter_coeff_payload(me_ptr, source_ptr);
            if (CAPI_EBADPARAM == capi_result)
            {
               return capi_result;
            }

            if (CAPI_ENOMEMORY == capi_result)
            {
               capi_fir_clean_up_memory(me_ptr); // clearing the entire lib memory
			   capi_fir_update_release_config(me_ptr); // clean up the CURR config
               return capi_result;
            }
			me_ptr->size_req_for_get_next_coeff = params_ptr->actual_data_len;
         }
         else // if the configurations comes during an active configuration, we need to queue the config
         {
            me_ptr->config_type = CAPI_QUEUE_CFG;
            FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Caching the configuration in the QUEUE filter config");

            capi_result = capi_fir_cache_filter_coeff_payload(me_ptr, source_ptr);

            if (CAPI_EBADPARAM == capi_result)
            {
               return capi_result;
            }

            if (CAPI_ENOMEMORY == capi_result)
            {
               capi_fir_clean_up_memory(me_ptr);
               capi_fir_update_release_config(me_ptr); //
			   capi_fir_update_release_config(me_ptr); // clean up CURR & NEXT config
               return capi_result;
            }
            me_ptr->cfg_version = VERSION_V1; // indicates V1 version is set now and can only allow V1 version
                                               // configs for upcoming set params
            // set param will happen from process if all ch's crossfade is completed
            me_ptr->size_req_for_get_queue_coeff = params_ptr->actual_data_len;
         }

         // set param if library is created
         if ((NULL != me_ptr->fir_channel_lib) && (me_ptr->config_type != CAPI_QUEUE_CFG))
         {

            capi_result = capi_fir_set_cached_filter_coeff(me_ptr);
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
         break;
      }

      case PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG:
      {
         if (param_size < sizeof(param_id_fir_filter_crossfade_cfg_t))
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "configuration SetParam 0x%lx, invalid param size %lx ",
                   param_id,
                   params_ptr->actual_data_len);
            capi_result |= CAPI_ENEEDMORE;
            break;
         }
         param_id_fir_filter_crossfade_cfg_t *const fir_crossfade_cfg1 =
            ((param_id_fir_filter_crossfade_cfg_t *)params_ptr->data_ptr);
         int8_t *fir_crossfade_ptr = params_ptr->data_ptr;

         // check no of cfg and cross check with size
         const uint32_t num_cfg = fir_crossfade_cfg1->num_config;
         if (num_cfg < 1 || num_cfg > 63)
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Received incorrect num_config parameter in Set Crossfade config - %lu",
                   num_cfg);
            return CAPI_EBADPARAM;
         }

         fir_crossfade_ptr += sizeof(param_id_fir_filter_crossfade_cfg_t); // pointing to 1st crossfade config
         uint32_t req_payload_size =
            sizeof(param_id_fir_filter_crossfade_cfg_t) + (sizeof(fir_filter_crossfade_cfg_t) * num_cfg);

         if (param_size < req_payload_size)
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Crossfade SetParam 0x%lx, invalid param size %lu ,required_size %lu",
                   param_id,
                   params_ptr->actual_data_len,
                   req_payload_size);
            return CAPI_EBADPARAM;
         }

         fir_filter_crossfade_cfg_t *const fir_crossfade_cfg = (fir_filter_crossfade_cfg_t *)fir_crossfade_ptr;

         // check if the channel masks received is proper or not
         capi_result = capi_fir_check_channel_map_crossfade_cfg(fir_crossfade_cfg, num_cfg);

         if (CAPI_FAILED(capi_result))
         {
            FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Received incorrect channel map for parameter id 0x%lx", param_id);
            return capi_result;
         }

         int8_t *source_ptr = params_ptr->data_ptr;
         // cache the payload
         capi_result = capi_fir_cache_crossfade_payload(me_ptr, source_ptr, param_size);
         if (capi_result)
         {
            return capi_result;
         }
         me_ptr->cfg_version = VERSION_V1; // indicates V1 version is set now and can only allow V1 version
                                            // configs for upcoming set params

         // set param if library is created AND no active crossfade in any of the channels
         if (NULL != me_ptr->fir_channel_lib)
         {
            if (0 == me_ptr->combined_crossfade_status)
            {
               capi_result = capi_fir_set_cached_filter_crossfade_config(me_ptr);
            }
            else
            {
               me_ptr->is_xfade_cfg_pending = TRUE;
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
         me_ptr->cache_fir_crossfade_cfg_size = params_ptr->actual_data_len;
         break;
      }
      case INTF_EXTN_PARAM_ID_PERIOD:
      {
         // Using this extension as a short term solution.
         // Voice would always place a higher vote (assumes always cross-fade is on)
         // and audio would dynamically adjust the vote during the cross-fade
         FIR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "recieved intf_extn_param_id_period, module part of voice graph");
         me_ptr->is_module_in_voice_graph = TRUE;
         break;
      }
      case PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH_V2:
      {
         capi_result = capi_fir_set_fir_filter_max_tap_length_v2(me_ptr, param_id, param_size, params_ptr->data_ptr);
         if (CAPI_FAILED(capi_result))
         {
        	FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Max Fir tap length V2 SetParam 0x%lx failed.", param_id);
            break;
         }
         break;
      }
      case PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG_V2:
      {
         capi_result = capi_fir_set_fir_filter_crossfade_v2(me_ptr, param_id, param_size, params_ptr->data_ptr);
         if (CAPI_FAILED(capi_result))
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Crossfade V2 SetParam 0x%lx failed.", param_id);
            break;
         }
         break;
      }
      case PARAM_ID_FIR_FILTER_CONFIG_V2:
      {
         capi_result = capi_fir_set_fir_filter_config_v2(me_ptr, param_id, param_size, params_ptr->data_ptr);
         if (CAPI_FAILED(capi_result))
         {
            FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Filter coefficient V2 SetParam 0x%lx failed.", param_id);
            break;
         }
         break;
      }

      default:
      {
         FIR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Set unsupported parameter ID %0x", (int)param_id);
         CAPI_SET_ERROR(capi_result, CAPI_EBADPARAM);
      }
   } // switch_case end
   return capi_result;
} // set_param_function end

/*=====================================================================
  Function name: capi_fir_get_param
  DESCRIPTION: Gets the corresponding parameters based on the PARAM ID
  =====================================================================*/
static capi_err_t capi_fir_get_param(capi_t *                _pif,
                                     uint32_t                param_id,
                                     const capi_port_info_t *port_info_ptr,
                                     capi_buf_t *            params_ptr)

{
   if (NULL == _pif || NULL == params_ptr)
   {
      FIR_MSG(MIID_UNKNOWN, DBG_ERROR_PRIO, "Get param received bad pointer, 0x%p, 0x%p",
             _pif,
             params_ptr);
      return CAPI_EBADPARAM;
   }

   capi_err_t  capi_result = CAPI_EOK;
   capi_fir_t *me_ptr      = (capi_fir_t *)(_pif);
   uint32_t miid = me_ptr->miid;
   switch (param_id)
   {
      case PARAM_ID_MODULE_ENABLE:
	     break;
      case PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH:
      case PARAM_ID_FIR_FILTER_CONFIG:
      case PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG:
      {
         if((VERSION_V2 == me_ptr->cfg_version) || (TRUE == me_ptr->higher_channel_map_present))
         {
            FIR_MSG(me_ptr->miid,DBG_ERROR_PRIO, "GetParam 0x%x failed as V2 config is already configured for the module. "
                   "Cannot perform both V1 and V2 operations simultaneously for the module OR higher than 63 channel map present in IMF (0/1): %lu",
				   (int)param_id,
				   me_ptr->higher_channel_map_present);
            return CAPI_EBADPARAM;
         }
         break;
      }
      break;
      case PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH_V2:
      case PARAM_ID_FIR_FILTER_CONFIG_V2:
      case PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG_V2:
      {
         if(VERSION_V1 == me_ptr->cfg_version)
         {
            FIR_MSG(me_ptr->miid,DBG_ERROR_PRIO, "GetParam 0x%x failed as V1 config is already configured for the module. "
                   "Cannot perform both V1 and V2 operations simultaneously for the module",(int)param_id);
            return CAPI_EBADPARAM;
         }
         break;
      }
      default:
      {
         FIR_MSG(me_ptr->miid,DBG_ERROR_PRIO, "GetParam, unsupported param ID 0x%x", (int)param_id);
         return CAPI_EUNSUPPORTED;
      }
   }
   switch (param_id)
   {
      case PARAM_ID_MODULE_ENABLE:
      {
         if (params_ptr->max_data_len >= sizeof(param_id_module_enable_t))
         {
            param_id_module_enable_t *enable_ptr = (param_id_module_enable_t *)(params_ptr->data_ptr);
            enable_ptr->enable                   = me_ptr->is_fir_enabled;
            params_ptr->actual_data_len          = sizeof(param_id_module_enable_t);
         }
         else
         {
            FIR_MSG(miid, DBG_ERROR_PRIO, "Get Enable/Disable, Bad param size %lu", params_ptr->max_data_len);
            capi_result = CAPI_ENEEDMORE;
         }
         break;
      }
      case PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH:
      {
         if (NULL != me_ptr->cache_fir_max_tap)
         {
            params_ptr->actual_data_len = 0;
            if (params_ptr->max_data_len >= me_ptr->cache_fir_max_tap_size)
            {
               params_ptr->actual_data_len = 0;
               param_id_fir_filter_max_tap_cfg_t *pfir_max_tap_length =
                  (param_id_fir_filter_max_tap_cfg_t *)(params_ptr->data_ptr);
               param_id_fir_filter_max_tap_cfg_t *cached_max_tap_param =
                  (param_id_fir_filter_max_tap_cfg_t *)me_ptr->cache_fir_max_tap;
               params_ptr->actual_data_len = memscpy(pfir_max_tap_length,
                                                     params_ptr->max_data_len,
                                                     cached_max_tap_param,
                                                     me_ptr->cache_fir_max_tap_size);
            }
            else
            {
               params_ptr->actual_data_len = me_ptr->cache_fir_max_tap_size;

               FIR_MSG(miid, DBG_ERROR_PRIO, "Get Max tap length, Bad param size %lu, Req size: %lu",
                      params_ptr->max_data_len,
                      me_ptr->cache_fir_max_tap_size);

               capi_result |= CAPI_ENEEDMORE;
            }
         }
         else
         {
            FIR_MSG(miid, DBG_ERROR_PRIO, "Did not receive any set param for parameter 0x%lx", param_id);
            capi_result |= CAPI_EFAILED;
         }
         break;
      }
      case PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH_V2:
      {
    	 capi_result = capi_fir_get_filter_max_tap_length_v2(me_ptr, params_ptr, param_id, miid);
    	 if(CAPI_FAILED(capi_result))
    	 {
    		 return capi_result;
    	 }
         break;
      }
      case PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG_V2:
      {
     	 capi_result = capi_fir_get_filter_crossfade_cfg_v2(me_ptr, params_ptr, param_id, miid);
     	 if(CAPI_FAILED(capi_result))
     	 {
     		 return capi_result;
     	 }
         break;
      }
      case PARAM_ID_FIR_FILTER_CONFIG_V2:
      {
      	 capi_result = capi_fir_get_filter_cfg_v2(me_ptr, params_ptr, param_id, miid);
      	 if(CAPI_FAILED(capi_result))
      	 {
      		 return capi_result;
      	 }
         break;
      }

      case PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG:
      {
         if (NULL != me_ptr->cache_fir_crossfade_cfg)
         {
            if (params_ptr->max_data_len >= me_ptr->cache_fir_crossfade_cfg_size)
            {
               params_ptr->actual_data_len = 0;
               param_id_fir_filter_crossfade_cfg_t *pfir_crossfade_cfg =
                  (param_id_fir_filter_crossfade_cfg_t *)(params_ptr->data_ptr);
               param_id_fir_filter_crossfade_cfg_t *cached_crossfade_param =
                  (param_id_fir_filter_crossfade_cfg_t *)me_ptr->cache_fir_crossfade_cfg;
               params_ptr->actual_data_len = memscpy(pfir_crossfade_cfg,
                                                     params_ptr->max_data_len,
                                                     cached_crossfade_param,
                                                     me_ptr->cache_fir_crossfade_cfg_size);
            }
            else
            {
               params_ptr->actual_data_len = me_ptr->cache_fir_crossfade_cfg_size;

               FIR_MSG(miid, DBG_ERROR_PRIO, "Get Cross-fade param, Bad param size %lu, Req size: %lu",
                      params_ptr->max_data_len,
                      me_ptr->cache_fir_crossfade_cfg_size);

               capi_result |= CAPI_ENEEDMORE;
            }
         }
         else
         {
            FIR_MSG(miid, DBG_ERROR_PRIO, "Did not receive any set param for parameter 0x%lx", param_id);
            capi_result |= CAPI_EFAILED;
            params_ptr->actual_data_len = 0;
         }
         break;
      }
      case PARAM_ID_FIR_FILTER_CONFIG:
      {
         param_id_fir_filter_config_t *cached_coeff_ptr = NULL;
         int8_t *                      fir_cfg_ptr      = NULL;
         uint32_t                      get_coeff_size   = 0;

         // if 2 or more configs are present then return next cfg bcs crossfading going on from cfg1 to cfg2
         if (NULL != me_ptr->cache_next_fir_coeff_cfg)
         {
            FIR_MSG(miid, DBG_LOW_PRIO, "active crossfade going on, return next config");
            get_coeff_size   = me_ptr->size_req_for_get_next_coeff;
            cached_coeff_ptr = (param_id_fir_filter_config_t *)me_ptr->cache_next_fir_coeff_cfg;
            fir_cfg_ptr      = (int8_t *)me_ptr->cache_next_fir_coeff_cfg;
         }
         else if (NULL != me_ptr->cache_fir_coeff_cfg) // only 1 config present
         {
            FIR_MSG(miid, DBG_LOW_PRIO, "no active crossfade, return curr config");
            get_coeff_size   = me_ptr->size_req_for_get_coeff;
            cached_coeff_ptr = (param_id_fir_filter_config_t *)me_ptr->cache_fir_coeff_cfg; // pointing to 1st config
            fir_cfg_ptr      = (int8_t *)me_ptr->cache_fir_coeff_cfg;
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
               CAPI_FIR_ALIGN_8_BYTE(sizeof(param_id_fir_filter_config_t)); // this is done to ensure the coefficients
                                                                            // are always at 8 byte alignment.

               uint32_t size_to_copy = 0;

               uint32_t increment_size_of_ptr;
               uint32_t total_size_copied = 0;
               int8_t * dest_ptr          = params_ptr->data_ptr;
               uint32_t dest_max_size     = params_ptr->max_data_len;

               size_to_copy = sizeof(param_id_fir_filter_config_t);
               total_size_copied += memscpy(dest_ptr, dest_max_size, cached_coeff_ptr, size_to_copy);
               dest_ptr += total_size_copied;
               dest_max_size -= total_size_copied;
               fir_filter_cfg_t *temp_coeff_cfg_ptr = (fir_filter_cfg_t *)fir_cfg_ptr;
               int8_t *          temp_increment_ptr = (int8_t *)temp_coeff_cfg_ptr;
               for (uint32_t count = 0; count < cached_coeff_ptr->num_config; count++)
               {
                  increment_size_of_ptr = 0;
                  if (16 == temp_coeff_cfg_ptr->coef_width)
                  {
                     size_to_copy = capi_fir_coefficient_convert_to_32bit(me_ptr,
                                                                             (int8_t *)temp_coeff_cfg_ptr,
                                                                             temp_coeff_cfg_ptr->num_taps,
                                                                             dest_ptr,
                                                                             dest_max_size);
                     total_size_copied += size_to_copy;
                     increment_size_of_ptr =
                        (temp_coeff_cfg_ptr->num_taps * sizeof(int16_t)) + sizeof(fir_filter_cfg_t);

                     if (temp_coeff_cfg_ptr->num_taps % 4)
                     {
                        increment_size_of_ptr += sizeof(int16_t) * (4 - (temp_coeff_cfg_ptr->num_taps % 4));
                     }
					      increment_size_of_ptr += 4; //this is done because we are storing coefficients in internal cached structure after
					                                  //gap of 4 bytes to make them 8 byte aligned

                     dest_ptr += size_to_copy;
                     dest_max_size -= size_to_copy;
                     temp_increment_ptr += increment_size_of_ptr;
                     temp_coeff_cfg_ptr = (fir_filter_cfg_t *)temp_increment_ptr;
                  }
                  else
                  {
                     size_to_copy = sizeof(fir_filter_cfg_t);
                     total_size_copied += memscpy(dest_ptr, dest_max_size, temp_coeff_cfg_ptr, size_to_copy);
                     dest_ptr += size_to_copy;
                     dest_max_size -= size_to_copy;
                     temp_increment_ptr += size_to_copy;
                     temp_increment_ptr += 4; // this is done because we are storing coefficients in internal cached structure after
                                              // gap of 4 bytes to make them 8 byte aligned

                     // copy the coefficients
                     size_to_copy = sizeof(int32_t) * temp_coeff_cfg_ptr->num_taps;
#ifdef QDSP6_ASM_OPT_FIR_FILTER
                     int32_t *src_temp = (int32_t*) (temp_increment_ptr + size_to_copy); // Because coefficients stored in reverse order
                     int32_t *dest_temp = (int32_t*) dest_ptr;
                     src_temp--;
                     for (uint16_t tap_count = 0; tap_count < temp_coeff_cfg_ptr->num_taps; tap_count++)
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

                     if (temp_coeff_cfg_ptr->num_taps % 4)
                     {
                        increment_size_of_ptr = sizeof(int32_t) * (4 - (temp_coeff_cfg_ptr->num_taps % 4));
                     }
                     temp_increment_ptr += increment_size_of_ptr;
                     temp_coeff_cfg_ptr = (fir_filter_cfg_t *)temp_increment_ptr;
                  }
               }

            params_ptr->actual_data_len = total_size_copied;
         }
         else
         {
            params_ptr->actual_data_len = get_coeff_size;
            FIR_MSG(miid, DBG_ERROR_PRIO, "Get Filter coeff cfg, Bad param size %lu, Req size = %lu",
                   params_ptr->max_data_len,
                   get_coeff_size);
            capi_result |= CAPI_ENEEDMORE;
         }
         break;
      }
   }
   return capi_result;
}
