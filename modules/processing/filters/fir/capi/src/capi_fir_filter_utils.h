/* =================================================================================== */
/**
  @file capi_fir_filter_utils.h
  Header file for utilities to implement the Common Audio Processor Interface v2 for
  Fir Filter library
 */

/* ====================================================================================
 * Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
   ========================================================================== */

#ifndef CAPI_FIR_FILTER_UTILS_H
#define CAPI_FIR_FILTER_UTILS_H

/*------------------------------------------------------------------------
 * Include files
 * -----------------------------------------------------------------------*/
#include "../../lib/inc/FIR_ASM_macro.h"
#include "capi_fir_filter.h"
#include "fir_api.h"

#include "posal.h"
#include "api_fir.h"
#include "capi_cmn.h"

/*------------------------------------------------------------------------
 * Macros, Defines, Type declarations
 * -----------------------------------------------------------------------*/
#define CAPI_FIR_DEFAULT_PORT 0
#define CAPI_FIR_DEBUG_MSG 1

#define ALIGN_8_BYTE 8
#define CAPI_FIR_ALIGN_8_BYTE(x) (((x) + 7) & (0xFFFFFFF8))
#define CAPI_FIR_ALIGN_4_BYTE(x) (((x) + 3) & (0xFFFFFFFC))
#define CAPI_MAX_PROCESS_FRAME_SIZE 240 // library has similar macro. We should ideally use the same one

#define FILTER_CFG_BASE_PAYLOAD_SIZE sizeof(param_id_fir_filter_config_v2_t)
#define FILTER_CFG_PER_CFG_BASE_PAYLOAD_SIZE sizeof(fir_filter_cfg_v2_t)
#define MAX_TAP_BASE_PAYLOAD_SIZE sizeof(param_id_fir_filter_max_tap_cfg_v2_t)
#define MAX_TAP_PER_CFG_BASE_PAYLOAD_SIZE sizeof(fir_filter_max_tap_length_cfg_v2_t)
#define CROSSFADE_BASE_PAYLOAD_SIZE sizeof(param_id_fir_filter_crossfade_cfg_v2_t)
#define CROSSFADE_PER_CFG_BASE_PAYLOAD_SIZE sizeof(fir_filter_crossfade_cfg_v2_t)

/* debug message */
#define MIID_UNKNOWN 0
#define FIR_MSG_PREFIX "CAPI FIR:[%lX] "
#define FIR_MSG(ID, xx_ss_mask, xx_fmt, ...)\
         AR_MSG(xx_ss_mask, FIR_MSG_PREFIX xx_fmt, ID, ##__VA_ARGS__)

static const fir_lib_mem_requirements_t capi_fir_null_mem_req = { 0, 0 };

typedef enum capi_fir_config_type_t
{
   CAPI_CURR_CFG = 0,
   CAPI_NEXT_CFG,
   CAPI_QUEUE_CFG
} capi_fir_config_type_t;

typedef enum capi_fir_config_version_t
{
   DEFAULT    = 0,
   VERSION_V1 = 1,
   VERSION_V2 = 2
} capi_fir_config_version_t;

typedef struct capi_fir_event_config
{
   bool_t   enable;
   uint32_t kpps;
   uint32_t delay_in_us;
   uint32_t code_bw;
   uint32_t data_bw;
} capi_fir_event_config_t;

typedef struct capi_fir_filter_cfg_static_params
{
   uint32_t coef_width;
   uint16_t coef_q_factor;
   uint16_t num_taps;
   uint32_t filter_delay_in_samples;
}capi_fir_filter_cfg_static_params;

typedef struct capi_fir_channel_lib
{
   fir_static_struct_t            fir_static_variables;
   fir_config_struct_t            fir_config_variables;
   fir_cross_fading_struct_t      fir_crossfade_variables;
   fir_transition_status_struct_t fir_transition_status_variables;
   fir_lib_mem_requirements_t     lib_mem_req;
   fir_lib_t                      fir_lib_instance;
} capi_fir_channel_lib_t;

typedef struct capi_fir_v2_configs
{
   capi_fir_config_type_t config_type; // describes the config type of filter config out  of curr, next or pending
   // updated only after media format received
   bool_t   xfade_flag;                // true if crossfade enabled in any of the channels
   int32_t  is_xfade_cfg_pending;      // specifies (0/1/-1) --> (crossfade config is not pending/pending/failed to apply)
   uint32_t combined_crossfade_status; // tells if crossfade completed for all the channels

   // cache params
   param_id_fir_filter_max_tap_cfg_v2_t *cache_fir_max_tap;
   param_id_fir_filter_config_v2_t *cache_fir_coeff_cfg;         // current filter config FIR is operating on
   param_id_fir_filter_config_v2_t *cache_next_fir_coeff_cfg;    // next filter config where we need to crossfade to
   param_id_fir_filter_config_v2_t *cache_queue_fir_coeff_cfg;   // config that comes during crossfade
   param_id_fir_filter_crossfade_cfg_v2_t *cache_fir_crossfade_cfg;

   // Original cache_fir_coeff_cfg pointer
   int8_t *cache_original_fir_coeff_cfg;
   int8_t *cache_original_next_fir_coeff_cfg;
   int8_t *cache_original_queue_fir_coeff_cfg;

   // cache sizes
   uint32_t cache_fir_max_tap_size;
   uint32_t cache_fir_crossfade_cfg_size;

   // the size needed to cache the param is not same as the incoming payload size
   uint32_t cache_fir_coeff_cfg_size;
   uint32_t cache_next_fir_coeff_cfg_size;
   uint32_t cache_queue_fir_coeff_cfg_size;

   // makes it easy to return size for needmore get
   // it is same as the incoming param size
   uint32_t size_req_for_get_coeff;
   uint32_t size_req_for_get_next_coeff;
   uint32_t size_req_for_get_queue_coeff;
} capi_fir_v2_configs;


typedef struct capi_fir_t
{
   capi_t                     vtbl;
   capi_event_callback_info_t cb_info;
   capi_heap_id_t             heap_info;
   capi_fir_event_config_t    event_config;

   // media_info
   capi_media_fmt_v2_t input_media_fmt[CAPI_FIR_MAX_IN_PORTS];

   // lib_info
   capi_fir_channel_lib_t *fir_channel_lib;
   bool_t                  is_fir_enabled;

   capi_fir_config_type_t config_type; // describes the config type of filter config out  of curr, next or pending
   // updated only after media format received
   bool_t   xfade_flag;                // true if crossfade enabled in any of the channels
   int32_t  is_xfade_cfg_pending;      // specifies (0/1/-1) --> (crossfade config is not pending/pending/failed to apply)
   uint32_t combined_crossfade_status; // tells if crossfade completed for all the channels
   // cache params
   param_id_fir_filter_max_tap_cfg_t *cache_fir_max_tap;

   param_id_fir_filter_config_t *cache_fir_coeff_cfg;         // current filter config FIR is operating on
   param_id_fir_filter_config_t *cache_next_fir_coeff_cfg;     // next filter config where we need to crossfade to
   param_id_fir_filter_config_t *cache_queue_fir_coeff_cfg; // config that comes during crossfade

   param_id_fir_filter_crossfade_cfg_t *cache_fir_crossfade_cfg;
   // Original cache_fir_coeff_cfg pointer
   int8_t *cache_original_fir_coeff_cfg;
   int8_t *cache_original_next_fir_coeff_cfg;
   int8_t *cache_original_queue_fir_coeff_cfg;

   // cache sizes
   uint32_t cache_fir_max_tap_size;
   uint32_t cache_fir_crossfade_cfg_size;

   // the size needed to cache the param is not same as the incoming payload size
   uint32_t cache_fir_coeff_cfg_size;
   uint32_t cache_next_fir_coeff_cfg_size;
   uint32_t cache_queue_fir_coeff_cfg_size;

   // makes it easy to return size for needmore get
   // it is same as the incoming param size
   uint32_t size_req_for_get_coeff;
   uint32_t size_req_for_get_next_coeff;
   uint32_t size_req_for_get_queue_coeff;

   capi_fir_v2_configs capi_fir_v2_cfg;

   uint32_t is_module_in_voice_graph; // helps to classify where the module is placed.
   uint32_t frame_size_in_samples;    // specify the frame-size for the library to create buffers.

   uint32_t miid;
   // flag to detect if higher than 63 channel map received or not
   bool_t   higher_channel_map_present;

   capi_fir_config_version_t cfg_version;
} capi_fir_t;

/* ------------------------------------------------------------------------
 * Function declarations
 * -----------------------------------------------------------------------*/
static inline bool_t capi_fir_filter_lib_is_enabled(capi_fir_t *const me_ptr)
{
   bool_t flag = FALSE;
   if(VERSION_V1 == me_ptr->cfg_version)
   {
      flag = ((NULL != me_ptr->cache_fir_max_tap) && (NULL != me_ptr->cache_fir_coeff_cfg))
              ? TRUE : FALSE;
   }
   else
   {
      flag = ((NULL != me_ptr->capi_fir_v2_cfg.cache_fir_max_tap) && (NULL != me_ptr->capi_fir_v2_cfg.cache_fir_coeff_cfg))
              ? TRUE : FALSE;
   }
   return ((FALSE != me_ptr->is_fir_enabled) && (NULL != me_ptr->fir_channel_lib) && flag)
             ? TRUE
             : FALSE;
}
static inline uint64_t capi_fir_calculate_channel_mask(uint32_t channel_mask_lsb, uint32_t channel_mask_msb)
{
   return (((uint64_t)(channel_mask_lsb)) | (((uint64_t)(channel_mask_msb)) << 32));
}

void capi_fir_init_events(capi_fir_t *const me_ptr);

capi_err_t capi_fir_raise_process_event(capi_fir_t *me_ptr);

void capi_fir_free_memory(capi_fir_t *me_ptr);

capi_err_t capi_fir_process_set_properties(capi_fir_t *me_ptr, capi_proplist_t *proplist_ptr);

capi_err_t capi_fir_process_get_properties(capi_fir_t *me_ptr, capi_proplist_t *proplist_ptr);

capi_err_t capi_fir_check_create_lib_instance(capi_fir_t *me_ptr, bool_t is_num_channels_changed);

capi_err_t capi_fir_allocate_per_channel_lib_struct(capi_fir_t *me_ptr, uint32_t num_chan);

capi_err_t capi_fir_raise_events(capi_fir_t *me_ptr);

capi_err_t capi_fir_check_channel_map_max_tap_cfg(fir_filter_max_tap_length_cfg_t *cfg_ptr, uint32_t num_config);

capi_err_t capi_fir_check_channel_map_coeff_cfg(uint32_t miid, fir_filter_cfg_t *cfg_ptr, uint32_t num_config);

capi_err_t capi_fir_set_cached_filter_coeff(capi_fir_t *me_ptr);

capi_err_t capi_fir_reset(capi_fir_t *me_ptr);

capi_err_t capi_fir_lib_set_enable_param(capi_fir_t *me_ptr);

void capi_fir_clean_up_memory(capi_fir_t *me_ptr);

void capi_fir_clean_up_cached_coeff_memory(capi_fir_t *me_ptr);

uint32_t capi_fir_coefficient_convert_to_32bit(capi_fir_t *me_ptr,
                                               int8_t *    coeff_ptr,
                                               uint16_t    num_of_taps,
                                               int8_t *    dest_ptr,
                                               uint32_t    max_dest_size);

capi_err_t capi_fir_cache_max_tap_payload(capi_fir_t *   me_ptr,
                                          int8_t *const  payload_ptr,
                                          const uint32_t size_of_payload);

bool_t capi_fir_has_max_tap_payload_changed(capi_fir_t *me_ptr, int8_t *const payload_ptr);

capi_err_t capi_fir_cache_filter_coeff_payload(capi_fir_t *me_ptr, int8_t *const payload_ptr);

capi_err_t capi_fir_validate_fir_coeff_payload_size(uint32_t 		  miid,
												    fir_filter_cfg_t *filter_coeff_cfg_ptr,
                                                    uint32_t          num_cfg,
                                                    uint32_t          param_size,
                                                    uint32_t *        received_payload_size);

uint32_t capi_fir_calculate_delay_of_filter(capi_fir_t *me_ptr);

capi_err_t capi_fir_copy_fir_coeff_cfg(capi_fir_t *me_ptr, int8_t *payload_ptr);

void capi_fir_clean_up_lib_memory(capi_fir_t *me_ptr);

capi_err_t capi_fir_check_channel_map_crossfade_cfg(fir_filter_crossfade_cfg_t *cfg_ptr, uint32_t num_config);

capi_err_t capi_fir_cache_crossfade_payload(capi_fir_t *   me_ptr,
                                            int8_t *const  payload_ptr,
                                            const uint32_t size_of_payload);

capi_err_t capi_fir_set_cached_filter_crossfade_config(capi_fir_t *me_ptr);

void capi_fir_update_config(capi_fir_t *me_ptr);

void capi_fir_release_config_pointers(capi_fir_t *me_ptr);

void capi_fir_update_release_config(capi_fir_t *me_ptr);

capi_err_t capi_fir_check_combined_crossfade(capi_fir_t *me_ptr, uint32_t chan_num);

capi_err_t capi_fir_set_fir_filter_max_tap_length_v2(capi_fir_t *me_ptr, uint32_t param_id, uint32_t param_size, int8_t *data_ptr);

capi_err_t capi_fir_validate_multichannel_v2_payload(capi_fir_t *me_ptr, int8_t *data_ptr,
		                                             uint32_t param_id, uint32_t param_size, uint32_t *req_payload_size,
													 uint32_t base_payload_size, uint32_t per_cfg_base_payload_size);

capi_err_t capi_fir_validate_per_channel_v2_payload(capi_fir_t *me_ptr,
                                                uint32_t        num_cfg,
                                                int8_t          *payload_cfg_ptr,
                                                uint32_t        param_size,
                                                uint32_t        *required_size_ptr,
                                                uint32_t        param_id,
												uint32_t base_payload_size,
												uint32_t per_cfg_base_payload_size);

bool_t capi_fir_has_max_tap_v2_payload_changed(capi_fir_t *me_ptr, int8_t *const payload_ptr,
		                                       uint32_t req_payload_size);

capi_err_t capi_fir_cache_max_tap_v2_payload(capi_fir_t *me_ptr, int8_t *const payload_ptr, const uint32_t size_of_payload);

capi_err_t capi_fir_check_create_lib_instance_v2(capi_fir_t *me_ptr, bool_t is_num_channels_changed);

void capi_fir_set_static_params_v2(capi_fir_t *me_ptr, uint32_t per_cfg_base_payload_size);

capi_err_t capi_fir_get_filter_max_tap_length_v2(capi_fir_t *me_ptr, capi_buf_t* params_ptr, uint32_t param_id, uint32_t miid);

capi_err_t capi_fir_set_fir_filter_crossfade_v2(capi_fir_t *me_ptr, uint32_t param_id, uint32_t param_size, int8_t *data_ptr);

capi_err_t capi_fir_cache_crossfade_v2_payload(capi_fir_t *   me_ptr,
                                            int8_t *const  payload_ptr,
                                            const uint32_t size_of_payload);

capi_err_t capi_fir_set_cached_filter_crossfade_config_v2(capi_fir_t *me_ptr);

capi_err_t capi_fir_check_combined_crossfade_v2(capi_fir_t *me_ptr, uint32_t chan_num);

void capi_fir_update_release_config_v2(capi_fir_t *me_ptr);

void capi_fir_update_config_v2(capi_fir_t *me_ptr);

void capi_fir_release_config_pointers_v2(capi_fir_t *me_ptr);

void capi_fir_clean_up_cached_coeff_memory_v2(capi_fir_t *me_ptr);

capi_err_t capi_fir_get_filter_crossfade_cfg_v2(capi_fir_t *me_ptr, capi_buf_t* params_ptr, uint32_t param_id, uint32_t miid);

capi_err_t capi_fir_set_fir_filter_config_v2(capi_fir_t *me_ptr, uint32_t param_id, uint32_t param_size, int8_t *data_ptr);

capi_err_t capi_fir_cache_filter_coeff_payload_v2(capi_fir_t *me_ptr, int8_t *const payload_ptr);

uint32_t capi_fir_calculate_cache_size_for_coeff_v2(uint32_t miid, fir_filter_cfg_v2_t *filter_coeff_cfg_ptr, uint32_t num_cfg);

capi_err_t capi_fir_copy_fir_coeff_cfg_v2(capi_fir_t *me_ptr, int8_t *payload_ptr);

capi_err_t capi_fir_set_cached_filter_coeff_v2(capi_fir_t *me_ptr);

uint32_t capi_fir_coeff_config_cache_increment_size_v2(uint32_t ch_mask_list_size, uint32_t coef_width, uint16_t num_taps);

void capi_fir_process_set_pending_crossfade_config(capi_fir_t *me_ptr, uint32_t is_crossfade_flag_updated);

void capi_fir_set_pending_xfade_cfg(capi_fir_t *me_ptr, uint32_t is_crossfade_flag_updated);

void capi_fir_set_pending_xfade_v2_cfg(capi_fir_t *me_ptr, uint32_t is_crossfade_flag_updated);

capi_err_t capi_fir_get_filter_cfg_v2(capi_fir_t *me_ptr, capi_buf_t* params_ptr, uint32_t param_id, uint32_t miid);

uint32_t capi_fir_coefficient_convert_to_32bit_v2(capi_fir_t *me_ptr,
                                               int8_t     *filter_cfg,
                                               uint16_t    num_of_taps,
                                               int8_t     *destination_ptr,
                                               uint32_t    max_dest_size);

bool_t capi_fir_check_multi_ch_channel_mask_v2_param(uint32_t miid,
		                                    uint32_t    num_config,
                                            uint32_t    param_id,
                                            int8_t *    param_ptr,
                                            uint32_t    base_payload_size,
											uint32_t per_cfg_base_payload_size);
#endif // CAPI_FIR_FILTER_UTILS_H
