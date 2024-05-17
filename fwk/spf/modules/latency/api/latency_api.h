#ifndef LATENCY_API_H
#define LATENCY_API_H
/*==============================================================================
  @file latency_api.h
  @brief This file contains Latency API
==============================================================================*/

/*=======================================================================
* Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
* SPDX-License-Identifier: BSD-3-Clause
=========================================================================*/
/** @h2xml_title1           {Audio Delay/Latency}
    @h2xml_title_agile_rev  {Audio Delay/Latency}
    @h2xml_title_date       {May 12, 2020} */

#include  "module_cmn_api.h"

/*------------------------------------------------------------------------------
   Module
------------------------------------------------------------------------------*/
/*==============================================================================
   Constants
==============================================================================*/
#define CAPI_LATENCY_MAX_IN_PORTS  1

#define CAPI_LATENCY_MAX_OUT_PORTS 1

#define LATENCY_STACK_SIZE         4096
/* Global unique Module ID definition
   Module library is independent of this number, it defined here for static
   loading purpose only */
#define MODULE_ID_LATENCY               0x0700101C

/** 
    @h2xmlm_module       {"MODULE_ID_LATENCY", 
                          MODULE_ID_LATENCY}
    @h2xmlm_displayName  {"Latency or Delay"}
    @h2xmlm_modSearchKeys{delay, Audio, Voice}
    @h2xmlm_toolPolicy   {Calibration}
    @h2xmlm_description  {ID of the Delay/Latency module on the LPCM data path.\n
    This module introduces the specified amount of delay in the audio path.\n
    If the delay is increased, silence is inserted. If the delay is decreased,
    data is dropped.\n

    There are no smooth transitions.The resolution of the delay applied is
    limited by the period of a single sample. It is recommended that device path be
    muted when the delay is changed (to avoid glitches).\n

    This module supports 
  - #PARAM_ID_LATENCY_CFG\n 
  - #PARAM_ID_MODULE_ENABLE \n

*   Supported Input Media Format: \n
*  - Data Format          : PCM DATA Format \n
*  - fmt_id               : Don't care \n
*  - Sample Rates         : Any \n
*  - Number of channels   : 1 to 63 (for certain products this module supports only 32 channels)\n
*  - Channel type         : 1 to 63 (CAPI_MAX, support upto 128) \n
*  - Bits per sample      : 16, 32 \n
*  - Q format             : 15, 27, 31\n
*  - Interleaving         : de-interleaved unpacked \n
*  - Signed/unsigned      : Signed }

    @h2xmlm_dataMaxInputPorts    {CAPI_LATENCY_MAX_IN_PORTS}
    @h2xmlm_dataInputPorts       {IN=2}
    @h2xmlm_dataMaxOutputPorts   {CAPI_LATENCY_MAX_OUT_PORTS}
    @h2xmlm_dataOutputPorts      {OUT=1}
    @h2xmlm_supportedContTypes   {APM_CONTAINER_TYPE_GC, APM_CONTAINER_TYPE_SC}
    @h2xmlm_isOffloadable        {true}
    @h2xmlm_stackSize            {LATENCY_STACK_SIZE}
    @h2xmlm_ToolPolicy           {Calibration}

    @{                   <-- Start of the Module -->
*/
/* Supported Latency Configuration Modes by the module */

#define LATENCY_MODE_GLOBAL 0

#define LATENCY_MODE_PER_CH 1

/* Structure for delay parameter in LPCM data paths. */
typedef struct delay_param_per_ch_cfg_t delay_param_per_ch_cfg_t;

/** @h2xmlp_subStruct */
struct delay_param_per_ch_cfg_t
{
   uint32_t channel_mask_lsb;
   /**< @h2xmle_description  { Lower 32 bits of the mask that indicates the corresponding channel
                               whose delay is to be set.
                               - Set the bits corresponding to 1 to 31 channels of standard channel
                                 mapping (channels are mapped per standard channel mapping)
                               - Position of the bit to set 1 (left shift) (channel_map)                   } */

   uint32_t channel_mask_msb;
   /**< @h2xmle_description  { Upper 32 bits of the mask that indicates the corresponding channel
                               whose delay is to be set.
                               - Set the bits corresponding to 32 to 63 channels of standard channel
                                 mapping (channels are mapped per standard channel mapping)
                               - Position of the bit to set  1 (left shift) (channel_map - 32)              } */


   uint32_t delay_us;
    /**< @h2xmle_description  {Delay in microseconds.\n
   -# The amount of delay must be greater than 0.\n  If the value is zero, this module is disabled.\n
   -# The actual resolution of the delay is limited by the period of a single audio sample.\n}
   @h2xmle_range   {0..100000}
   @h2xmle_default {1000} */
  };

/* ID of the Delay parameter used by MODULE_ID_LATENCY. */
#define PARAM_ID_LATENCY_CFG                     0x08001212

/* Structure for delay parameter in LPCM data paths. */
typedef struct param_id_latency_cfg_t param_id_latency_cfg_t;
/** @h2xmlp_parameter   {"PARAM_ID_LATENCY_CFG", PARAM_ID_LATENCY_CFG}
    @h2xmlp_description {Delay in microseconds - supports global delay (all channels), or per channel delay.}
    @h2xmlp_toolPolicy  {Calibration}  */

#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"
/* Payload of the PARAM_ID_LATENCY_CFG parameter used by
 MODULE_ID_LATENCY.
 */
struct param_id_latency_cfg_t
{
   uint32_t cfg_mode;
   /**< @h2xmle_description  {Latency mode.\n
   -# Denotes the mode - Global (same delay for all channels), or per channel (can be tuned).}
   @h2xmle_rangeList         {"LATENCY_MODE_GLOBAL" = LATENCY_MODE_GLOBAL,
                              "LATENCY_MODE_PER_CH" = LATENCY_MODE_PER_CH}
   @h2xmle_default {LATENCY_MODE_GLOBAL} */

   uint32_t global_delay_us;
   /**< @h2xmle_description  {Global Delay in microseconds.\n
   -#Valid only if cfg_mode is LATENCY_MODE_GLOBAL \n 
   -#The amount of delay must be greater than 0.\n  If the value is zero, this module is disabled.\n
   -#The actual resolution of the delay is limited by the period of a single audio sample.\n}
   @h2xmle_range   {0..100000}
   @h2xmle_default {0} */

   uint32_t num_config;
   /**< @h2xmle_description  {Valid only for LATENCY_MODE_PER_CH_mode \n
        -#Specifies the different delay configurations per channel.}
        @h2xmle_range        {0..63}
        @h2xmle_default      {0}                        */

   delay_param_per_ch_cfg_t mchan_delay[0];
   /**< @h2xmle_description {Specifies the different delay configurations.}}
        @h2xmle_variableArraySize {num_config}
        @h2xmle_default      {0} */

}
#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;

/**
  @h2xml_Select         {param_id_module_enable_t}
  @h2xmlm_InsertParameter

  @h2xml_Select         {delay_param_per_ch_cfg_t}
  @h2xmlm_InsertParameter

*/


/** @}                   <-- End of the Module -->*/
#endif


















