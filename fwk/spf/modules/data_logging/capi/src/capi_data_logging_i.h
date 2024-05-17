/* =========================================================================
   Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
   SPDX-License-Identifier: BSD-3-Clause
 * =========================================================================*/

/**
 * @file capi_data_logging_i.h
 */

#include "capi.h"
#include "posal.h"
#include "ar_msg.h"
#include "capi_cmn.h"
#include "capi_fwk_extns_island.h"
#include "capi_data_logging.h"
#include "capi_fwk_extns_pcm.h"
#include "data_logging_api.h"
#include "module_cmn_api.h"
#include "posal_memory.h"
#include "posal_timer.h"
#include "capi_cmn.h"
#define LOG_STATUS_QUERY_PERIOD 20 // Only query the log status every 20 process calls when log status is
                                   // disabled.
#define MIID_UNKNOWN 0
#define DATA_LOGGING_MSG_PREFIX "CAPI_DATA_LOGGING:[%lX] "
#define DATA_LOGGING_MSG(ID, xx_ss_mask, xx_fmt, ...) AR_MSG(xx_ss_mask, DATA_LOGGING_MSG_PREFIX xx_fmt, ID, ##__VA_ARGS__)
#define DATA_LOGGING_ALL_CH_LOGGING_MASK (0xFFFFFFFF)
#define LOG_BUFFER_ALIGNMENT 8

// represents the maximum channel supported by system
// this macro is used to derive maximum channel type groups and index groups to support till current maximum:128 channel type
#define DATA_LOGGING_MAX_SUPPORTED_CHANNEL_MAPS 128
// maximum supported channels for all the V1 PIDs
#define MAX_CHANNEL_V1_CFG             32
// number of channels that can be represented in a word mask
#define CHANNELS_PER_MASK              32
#define CHANNELS_PER_MASK_MINUS_ONE    (CHANNELS_PER_MASK - 1)

// optimal approach for mod operation of x with CHANNELS_PER_MASK
#define MOD_WITH_32(var1) (var1 & CHANNELS_PER_MASK_MINUS_ONE)

// optimal approach for divide operation of x with CHANNELS_PER_MASK
#define DIVIDE_WITH_32(var1) (var1 >> 5)

// even though this might depend only on number of channels and not channel maps, due to conversion of
// channel_type configuration to channel index configuration, we would need more number of bits
// In short, would need same number of bits as DATA_LOGGING_MAX_SUPPORTED_CHANNEL_MAPS to store ch index config
#define TOTAL_BITS_NEEDED_FOR_INDEX_CFG   DATA_LOGGING_MAX_SUPPORTED_CHANNEL_MAPS

// would need DATA_LOGGING_MAX_SUPPORTED_CHANNEL_MAPS + 1 bits to store ch type config since
// bit 0 in 1st group is always reserved
#define TOTAL_BITS_NEEDED_FOR_TYPE_CFG   (DATA_LOGGING_MAX_SUPPORTED_CHANNEL_MAPS + 1)

// Adds the value of (CHANNELS_PER_MASK_MINUS_ONE) to the num_ch to account for rounding up to nearest higher integer value.
// The result is division of the sum by CHANNELS_PER_MASK, which determines the channel group.
#define GET_MAX_CHANNEL_GROUPS(num_ch) ((num_ch + (CHANNELS_PER_MASK_MINUS_ONE)) / CHANNELS_PER_MASK)

// maximum valid groups for channel index array
#define MAX_CHANNEL_INDEX_GROUPS GET_MAX_CHANNEL_GROUPS(TOTAL_BITS_NEEDED_FOR_INDEX_CFG)
const static uint32_t max_ch_idx_group = MAX_CHANNEL_INDEX_GROUPS;

// maximum valid groups for channel type array
#define MAX_CHANNEL_MAP_GROUPS GET_MAX_CHANNEL_GROUPS(TOTAL_BITS_NEEDED_FOR_TYPE_CFG)
const static uint32_t max_ch_type_group = MAX_CHANNEL_MAP_GROUPS;

#define CONVERT_TO_32B_MASK(var)   (uint32_t)((uint32_t)1<<var)

//checks if pos bit is set in val
//val and pos should be 32 bit variables.
#define IS_BIT_SET_AT_POS_IN_32B_VAL(val,pos) ((val & (1 << pos)))

#define SET_MASK_32B 0xFFFFFFFF

//#define DATA_LOGGING_DBG

// determines which version of selective channel PID is set
typedef enum capi_data_logging_selective_channel_cfg_state_t
{
   SELECTVE_CH_LOGGING_CFG_NOT_PRESENT = 0,
   SELECTVE_CH_LOGGING_CFG_V1          = 1,
   SELECTVE_CH_LOGGING_CFG_V2          = 2
} capi_data_logging_selective_channel_cfg_state_t;

/* CAPI structure  */

typedef struct capi_data_logging_channel_cfg_v2_t
{
   data_logging_select_channels_v2_t *cache_channel_logging_cfg_ptr;
   /*to cache selective channel logging configuration*/

   uint32_t cache_channel_logging_cfg_size;
   /*size to cache selective channel logging configuration*/

   uint32_t channel_mask_index_to_log_arr[MAX_CHANNEL_INDEX_GROUPS];
   /*new selective channel logging configuration*/

   uint16_t ch_index_mask_list_size_in_bytes;
   /* Array size of channel index mask payload represented in bytes. */
   /* Each element in the index group mask array must be traversed in order to reach the channel type group mask */
   /* since direct access to channel type group mask in not possible. */

   uint16_t ch_type_mask_list_size_in_bytes;
   /* Array size of channel type mask payload represented in bytes.*/
   /* Channel type mask array size is dependent on number of set bits in channel type group mask*/

} capi_data_logging_channel_cfg_v2_t;

typedef struct capi_data_logging_non_island_t
{
   uint32_t mid;
   /* Module ID of the Source module */

   uint32_t iid;
   /* Instance ID of the Source module */

   uint32_t log_code;
   /* QXDM ID for logging */

   uint32_t log_tap_point_id;
   /* Tap point ID for logging */

   uint32_t log_id;
   /* Unique log ID provided by framework */

   uint32_t log_id_reserved_mask;
   /* bits reserved for logging module to use from the LSB*/

   int8_t *log_buf_ptr;
   /* Buffer for logging */

   uint64_t log_buf_size;
   /* size of the log buffer */

   uint32_t per_ch_buf_part_size;
   /*Size of the log buffer partitions per channel (PCM Deinterleaved)*/

   uint32_t log_buf_fill_size;
   /* Current filled size of the log buffer */

   uint32_t per_channel_log_buf_offset;
   /* per channel buffer offset for deinterleaved data*/

   uint32_t seq_number;
   /* Current log packet sequence number */

   uint32_t counter;
   /* Counter for log code status query */

   uint32_t logging_mode;
   /* Indicates whether to log immediately or wait until log buffer is completely
    * filled */

   fwk_extn_pcm_param_id_media_fmt_extn_t extn_params;
   /* parameters for PCM fwk extension */

   uint32_t bitstream_format;
   /* Bit stream format for raw compressed media */

   capi_heap_id_t nlpi_heap_id;
   /*non-island heap ID*/

   data_logging_select_channels_t selective_ch_logging_cfg;
   /*selective channel logging configuration*/

   uint32_t enabled_channel_mask_array[MAX_CHANNEL_INDEX_GROUPS];
   /*channel index mask for which logging is enabled.*/

   capi_data_logging_channel_cfg_v2_t channel_logging_cfg;
   /*structure variable for selective channels V2 PID*/

   capi_data_logging_selective_channel_cfg_state_t selective_ch_logging_cfg_state;

   uint32_t number_of_channels_to_log;
   /*number of channels for which logging is enabled.*/

   uint16_t log_channel_type[CAPI_MAX_CHANNELS_V2];
   /*channel type array for the channels(num_channels) enabled for logging*/

   uint32_t is_media_fmt_extn_received : 1;
   /* flag to indicate reception of media fmt fwk extn */

   uint32_t is_enabled : 1;
   /*module enable flag via set-param*/

   uint32_t log_code_status : 1;
   /* Whether log code is connected or not. */

   uint32_t is_data_scaling_enabled : 1;
   /* For 1586 logging, we want to convert the Q27 data to Q31 before the logging.
    * This is a temporary change until we have a solution in HW EP modules to log the data*/

   uint32_t is_sink_mode : 1;
   /* If output port is closed state then data-logging module can act as sink module.*/

} capi_data_logging_non_island_t;

typedef struct capi_data_logging_t
{
   const capi_vtbl_t *vtbl;
   /* pointer to virtual table */

   capi_event_callback_info_t event_cb_info;
   /* Event Call back info received from Framework*/

   capi_heap_id_t heap_mem;
   /* Heap id received from framework*/

   capi_media_fmt_v2_t media_format;
   /* Media format set from upstream module */

   capi_data_logging_non_island_t *nlpi_me_ptr;
   /*capi structure which are not accessed in lpi*/

   bool_t forced_logging;
   /* Whether to Log in island mode by exiting the island if necessary */

   bool_t is_process_state;
   /*overall process state, true: enabled, false: disabled */

} capi_data_logging_t;

uint32_t count_set_bits(uint32_t num);

uint32_t calculate_size_for_ch_mask_array(uint32_t num);

bool_t capi_data_logging_media_fmt_is_valid(capi_media_fmt_v2_t *media_fmt_ptr);

void incr_log_id(capi_data_logging_t *me_ptr);

uint32_t get_number_of_channels_to_log(capi_data_logging_t *me_ptr);

capi_err_t check_alloc_log_buf(capi_data_logging_t *me_ptr);

void data_logging(capi_data_logging_t *me_ptr, int8_t *log_buf_ptr, bool_t is_full_frame);

capi_err_t capi_data_logging_raise_kpps_bw_event(capi_data_logging_t *me_ptr);

capi_err_t capi_data_logging_raise_process_state_event(capi_data_logging_t *me_ptr);

uint32_t capi_data_logging_get_channel_index_mask_to_log(capi_data_logging_t *           me_ptr,
                                                         data_logging_select_channels_t *ch_mask_cfg_ptr);

capi_err_t capi_data_logging_process(capi_t *_pif, capi_stream_data_t *input[], capi_stream_data_t *output[]);

capi_err_t capi_data_logging_end(capi_t *_pif);

capi_err_t capi_data_logging_set_param(capi_t *                _pif,
                                       uint32_t                param_id,
                                       const capi_port_info_t *port_info_ptr,
                                       capi_buf_t *            params_ptr);

capi_err_t capi_data_logging_get_param(capi_t *                _pif,
                                       uint32_t                param_id,
                                       const capi_port_info_t *port_info_ptr,
                                       capi_buf_t *            params_ptr);

capi_err_t capi_data_logging_set_properties(capi_t *_pif, capi_proplist_t *props_ptr);

capi_err_t capi_data_logging_get_properties(capi_t *_pif, capi_proplist_t *props_ptr);

capi_err_t capi_data_logging_process_nlpi(capi_data_logging_t *me_ptr,
                                          capi_stream_data_t * input[],
                                          capi_stream_data_t * output[]);

capi_err_t capi_data_logging_check_and_calculate_mask_payload_size(capi_data_logging_t *me_ptr,
                                                                   uint32_t *           array_size_ptr,
                                                                   uint32_t             max_valid_channel_group,
                                                                   uint32_t             channel_group_mask);

capi_err_t capi_data_logging_validate_ch_mask_payload_size(capi_data_logging_t *              me_ptr,
                                                           data_logging_select_channels_v2_t *chan_mask_cfg_ptr,
                                                           uint32_t                           param_size,
                                                           uint32_t *                         required_size_ptr);

capi_err_t capi_data_logging_get_channel_index_mask_to_log_v2(capi_data_logging_t *              me_ptr,
                                                              data_logging_select_channels_v2_t *ch_mask_cfg_ptr);

capi_err_t capi_data_logging_cache_selective_channel_payload(capi_data_logging_t *me_ptr,
                                                             int8_t *const        payload_ptr,
                                                             uint32_t             payload_actual_data_len,
                                                             uint32_t             required_cache_size);

capi_err_t capi_data_logging_update_enabled_channels_to_log(
   capi_data_logging_t *                           me_ptr,
   capi_data_logging_selective_channel_cfg_state_t selective_ch_logging_cfg_state);

capi_err_t capi_data_logging_set_selective_channels_v2_pid(capi_data_logging_t *me_ptr, capi_buf_t *params_ptr);

capi_err_t capi_data_logging_get_selective_channels_v2_pid(capi_data_logging_t *me_ptr,
                                                           uint32_t             param_id,
                                                           capi_buf_t *         params_ptr);
