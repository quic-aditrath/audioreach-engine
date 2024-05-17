/* ======================================================================== */
/**
@file capiv2_latency_utils.h

   Header file to implement the Common Audio Post Processor Interface
   for Tx/Rx Tuning latency block
*/

/* =========================================================================
 * Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause
  ========================================================================== */

/*------------------------------------------------------------------------
 * Include files
 * -----------------------------------------------------------------------*/

#ifndef CAPI_LATENCY_UTILS_H
#define CAPI_LATENCY_UTILS_H

#ifndef CAPI_STANDALONE
/* For shared libraries. */
#include "shared_lib_api.h"
#else
#include "capi_util.h"
#endif
#include "posal.h"
#include "capi.h"
#include "capi_cmn.h"
#include "capi_latency.h"
#include "latency_api.h"
#include "latency_buf_utils.h"

static const uint32_t LATENCY_BW         = 1 * 1024 * 1024;
static const uint32_t LATENCY_BW_16      = 2 * 1024 * 1024;
static const uint32_t LATENCY_BW_32      = 4 * 1024 * 1024;
static const uint32_t LATENCY_KPPS       = 1500;
static const uint32_t LATENCY_KPPS_16    = 2500;
static const uint32_t LATENCY_KPPS_32    = 5000;

#define SIZE_OF_ARRAY(a) (sizeof(a) / sizeof((a)[0]))

#if !((defined __hexagon__) || (defined __qdsp6__))
static const uint32_t CAPI_LATENCY_MAX_DELAY_US = 5000000;
#else
static const uint32_t CAPI_LATENCY_MAX_DELAY_US = 100000;
#endif

#define PCM_CHANNEL_NULL 0

#define CAPI_LATENCY_ALIGN_4_BYTE(x) (((x) + 3) & (0xFFFFFFFC))

static inline uint32_t align_to_8_byte(const uint32_t num)
{
   return ((num + 7) & (0xFFFFFFF8));
}

/*------------------------------------------------------------------------
 * Function declarations
 * -----------------------------------------------------------------------*/
#define CAPI_LATENCY_MF_V2_MIN_SIZE (sizeof(capi_standard_data_format_v2_t) + sizeof(capi_set_get_media_format_t))

typedef struct capi_delay_delayline_t
{
   bool_t is16bit;
   union
   {
      delayline_16_t   dl16;
      delayline_32_t   dl32;
   };
}capi_delay_delayline_t;

typedef struct capi_latency_cache_delay_per_config_t
{
   uint32_t channel_mask_lsb;
   uint32_t channel_mask_msb;
   uint32_t delay_in_us;
} capi_latency_cache_delay_per_config_t;

typedef struct capi_latency_cache_delay_t
{
   uint32_t                               num_config;
   capi_latency_cache_delay_per_config_t *cache_delay_per_config;

} capi_latency_cache_delay_t;

typedef struct capi_latency_events_config_t
{
   uint32_t enable;
   uint32_t kpps;
   uint32_t delay_in_us;
   uint32_t data_bw;
} capi_latency_events_config_t;

typedef struct capi_latency_per_chan_t
{
   uint32_t               delay_in_us;
   uint32_t               delay_in_samples;
   capi_delay_delayline_t delay_line;
} capi_latency_per_chan_t;

typedef struct capi_latency_module_config_t
{
   uint32_t                 enable;
   capi_latency_per_chan_t *mchan_config;

   void *mem_ptr;
} capi_latency_module_config_t;

typedef struct capi_latency_t
{
   capi_t                       vtbl;
   capi_event_callback_info_t   cb_info;
   capi_heap_id_t               heap_mem;
   capi_media_fmt_v2_t          media_fmt;
   capi_latency_events_config_t events_config;
   capi_latency_module_config_t lib_config;
   capi_latency_cache_delay_t   cache_delay;
   bool_t                       is_media_fmt_received;
   uint32_t                     cfg_mode;
} capi_latency_t;

capi_err_t capi_latency_process_set_properties(capi_latency_t *me_ptr, capi_proplist_t *proplist_ptr);

capi_err_t capi_latency_process_get_properties(capi_latency_t *me_ptr, capi_proplist_t *proplist_ptr);

void capi_latency_init_config(capi_latency_t *me_ptr);

capi_err_t capi_latency_raise_event(capi_latency_t *me_ptr, bool_t media_fmt_update);

void capi_latency_init_events(capi_latency_t *const me_ptr);

void capi_delay_destroy_buffer(capi_latency_t *me_ptr);

capi_err_t capi_delay_create_buffer(capi_latency_t *me_ptr, uint32_t *old_delay_in_us);

void capi_delay_calc_delay_in_samples(capi_latency_t *me_ptr);

void capi_delay_delayline_read(void *                  dest,
                               void *                  src,
                               capi_delay_delayline_t *delayline_ptr,
                               uint32_t                delay,
                               uint32_t                samples);

void capi_delay_delayline_update(capi_delay_delayline_t *delayline_ptr, void *src, uint32_t samples);

void capi_delay_delayline_copy(capi_delay_delayline_t *delayline_dest_ptr, capi_delay_delayline_t *delayline_src_ptr);

void capi_delay_set_delay(capi_latency_t *me_ptr);

capi_err_t capi_latency_check_channel_map_delay_cfg(delay_param_per_ch_cfg_t *delay_cfg_ptr, uint32_t num_config);

uint32_t capi_latency_get_max_delay(capi_latency_t *me_ptr);

#endif // CAPI_LATENCY_UTILS_H