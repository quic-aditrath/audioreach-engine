#ifndef API_FIR_H
#define API_FIR_H
/*==============================================================================
  @file api_fir.h
  @brief This file contains FIR_TUNING_FILTER API
==============================================================================*/

/*=======================================================================
* Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
=========================================================================*/

 /*------------------------------------------------------------------------
 * Include files
 * -----------------------------------------------------------------------*/
#include "module_cmn_api.h"

/**
     @h2xml_title1          {FIR Filter API}
     @h2xml_title_agile_rev {FIR Filter API}
     @h2xml_title_date      {December 7, 2018}
  */

/*==============================================================================
   Constants
==============================================================================*/

/** @ingroup ar_spf_mod_fir_macros
    Maximum number of input ports of the Finite Impulse Response module. */
#define CAPI_FIR_MAX_IN_PORTS  1

/** @ingroup ar_spf_mod_fir_macros
    Maximum number of output ports of the FIR module.. */
#define CAPI_FIR_MAX_OUT_PORTS 1

/** @ingroup ar_spf_mod_fir_macros
    Stack size for the FIR module. */
#define CAPI_FIR_STACK_SIZE    2048

/** @ingroup ar_spf_mod_fir_macros
    Finite Impulse Response Tuning Filter Module which supports Multi Channel filtering.\n

    @subhead4{Supported parameter IDs}
    - PARAM_ID_FIR_ENABLE
    - #PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH
    - #PARAM_ID_FIR_FILTER_CONFIG

    @subhead4{Supported input media format ID}
    - Data Format          : FIXED_POINT @lstsp1
    - fmt_id               : Don't care @lstsp1
    - Sample Rates         : Any (>0) @lstsp1
    - Number of channels   : 1 to 128 (for certain products this module supports only 32 channels) @lstsp1
    - Channel type         : 1 to 128 @lstsp1
    - Bits per sample      : 16, 32 @lstsp1
    - Q format             : 15, 27 @lstsp1
    - Interleaving         : De-interleaved unpacked @lstsp1
    - Signed/unsigned      : Signed @lstsp1 */
#define MODULE_ID_FIR_FILTER                         0x07001022
/**
    @h2xmlm_module       {"MODULE_ID_FIR_FILTER",
                          MODULE_ID_FIR_FILTER}
  @h2xmlm_displayName  {"FIR Filter"}
  @h2xmlm_modSearchKeys{filter, Audio, Voice}
    @h2xmlm_toolPolicy   {Calibration}
    @h2xmlm_description  {Finite Impulse Response Tuning Filter Module which supports Multi Channel filtering.\n
* - This module supports the following parameter IDs:  \n
*      - #PARAM_ID_FIR_ENABLE \n
*      - #PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH \n
*      - #PARAM_ID_FIR_FILTER_CONFIG \n
*
* Supported Input Media Format:\n
*  - Data Format          : FIXED_POINT\n
*  - fmt_id               : Don't care\n
*  - Sample Rates         : Any (>0)\n
*  - Number of channels   : 1 to 128 (for certain products this module supports only 32 channels)\n
*  - Channel type         : 1 to 128\n
*  - Bits per sample      : 16, 32\n
*  - Q format             : 15, 27\n
*  - Interleaving         : de-interleaved unpacked\n
*  - Signed/unsigned      : Signed }

    @h2xmlm_dataMaxInputPorts    {CAPI_FIR_MAX_IN_PORTS}
    @h2xmlm_dataInputPorts       {IN=2}
    @h2xmlm_dataMaxOutputPorts   {CAPI_FIR_MAX_OUT_PORTS}
    @h2xmlm_dataOutputPorts      {OUT=1}
    @h2xmlm_supportedContTypes  {APM_CONTAINER_TYPE_SC, APM_CONTAINER_TYPE_GC}
    @h2xmlm_isOffloadable        {true}
    @h2xmlm_stackSize            {CAPI_FIR_STACK_SIZE}
    @h2xmlm_ToolPolicy              {Calibration}
    @{                   <-- Start of the Module -->
*/

#ifdef PROD_SPECIFIC_MAX_CH
#define CAPI_FIR_MAX_CHANNELS_V1 63
#else
#define CAPI_FIR_MAX_CHANNELS_V1 32
#endif

/** @ingroup ar_spf_mod_fir_macros
    ID of the FIR filter Maximum Tap Length parameter used by
    MODULE_ID_FIR_FILTER. */
#define PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH           0x0800105F

/** @ingroup ar_spf_mod_fir_macros
    ID of the FIR filter coefficient parameter used by
    MODULE_ID_FIR_FILTER. */
#define PARAM_ID_FIR_FILTER_CONFIG                   0x0800111A

/** @ingroup ar_spf_mod_fir_macros
    ID of the FIR filter crossfade parameter used by
    MODULE_ID_FIR_FILTER. */
#define PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG         0x0800151D

/** @ingroup ar_spf_mod_fir_macros
    FIR filter maximum tap length.
*/

/** @h2xmlp_subStruct */

typedef struct fir_filter_max_tap_length_cfg_t
{
   uint32_t channel_mask_lsb;
   /**< Lower 32 bits of the channel mask. Each bit corresponds to channel map
        from 1 (PCM_CHANNEL_L) to 31 (PCM_CHANNEL_LW). \n
        Bit 0 is reserved and must be set to zero. \n
        - A set bit indicates that the filter max_tap_len is applied to corresponding channel-maps.
        Bit position of the channel-map is obtained by left shifting (1 (left shift) Channel_map). */

   /**< @h2xmle_description  {Lower 32 bits of the channel mask. Each bit corresponds to channel map
                             from 1 (PCM_CHANNEL_L) to 31 (PCM_CHANNEL_LW).
                             Bit 0 is reserved and must be set to zero.\n
                             -#A set bit indicates that the filter max_tap_len is applied to corresponding channel-maps.
                             Bit position of the channel-map is obtained by left shifting (1 (left shift) Channel_map)
                             }
    @h2xmle_default        {0xFFFFFFFE} */


   uint32_t channel_mask_msb; 
   /**< Upper 32 bits of the channel mask. Each bit corresponds to channel map
        from 32 (PCM_CHANNEL_RW) to 63 (PCM_CUSTOM_CHANNEL_MAP_16).\n
        - A set bit indicates that the filter max_tap_len is applied to corresponding channel-maps.
        Bit position of the channel-map is obtained by left shifting (1 (left shift) (Channel_map - 32)). */

   /**< @h2xmle_description  {Upper 32 bits of the channel mask. Each bit corresponds to channel map
                             from 32 (PCM_CHANNEL_RW) to 63 (PCM_CUSTOM_CHANNEL_MAP_16).\n
                             -#A set bit indicates that the filter max_tap_len is applied to corresponding channel-maps.
                             Bit position of the channel-map is obtained by left shifting (1 (left shift) (Channel_map - 32))
                             }
    @h2xmle_default        {0xFFFFFFFF}  */


   uint32_t fir_max_tap_length;
   /**< Specifies the maximum tap length of the FIR filter. This value is
        limited by resources (memory, MIPS, etc.). */

   /**< @h2xmle_description  { Specifies the maximum tap length of the FIR filter. This value is
                               limited by resources (memory, MIPS, etc.) }
        @h2xmle_range        {0..4294967295}
        @h2xmle_default      {512}
   */


}fir_filter_max_tap_length_cfg_t
;

/** @ingroup ar_spf_mod_fir_macros
    Structure for the maximum tap length parameter of FIR Filter module.
*/

/** @h2xmlp_parameter   {"PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH", PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH }
    @h2xmlp_description { Structure for the max tap length parameter of Fir filter module.}
     @h2xmlx_expandStructs	{false} 
     @h2xmlp_toolPolicy  {NO_SUPPORT}  */

#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"
/** Immediately following this structure are num_config structures of type
    fir_filter_max_tap_length_cfg_t.
 */
struct param_id_fir_filter_max_tap_cfg_t
{
   uint32_t num_config;
   /**< Specifies the different sets of filter coefficient configurations. */

   /**< @h2xmle_description  {Specifies the different sets of filter coefficient configurations.}
        @h2xmle_range        {1..CAPI_FIR_MAX_CHANNELS_V1}
        @h2xmle_default      {1}  */

   fir_filter_max_tap_length_cfg_t config_data[0];
   /**< Specifies the different sets of filter coefficient configurations. */

   /**< @h2xmle_description {Specifies the different sets of filter coefficient configurations}
        @h2xmle_variableArraySize {num_config}
        @h2xmle_default      {0}   */

}
#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;

typedef struct param_id_fir_filter_max_tap_cfg_t param_id_fir_filter_max_tap_cfg_t;

/** @ingroup ar_spf_mod_fir_macros
    Structure for the filter config parameter of FIR filter module.
*/

/** @h2xmlp_subStruct */

#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"
struct fir_filter_cfg_t
{
   uint32_t channel_mask_lsb;
   /**< Lower 32 bits of the channel mask. Each bit corresponds to channel map
        from 1 (PCM_CHANNEL_L) to 31 (PCM_CHANNEL_LW).
        Bit 0 is reserved and must be set to zero.\n
        - A set bit indicates that the filter corresponding to the set channel-maps are configured.
        Bit position of the channel-map is obtained by left shifting (1 (left shift) Channel_map). */

   /**< @h2xmle_description  {Lower 32 bits of the channel mask. Each bit corresponds to channel map
                             from 1 (PCM_CHANNEL_L) to 31 (PCM_CHANNEL_LW).
                             Bit 0 is reserved and must be set to zero.\n
                             -#A set bit indicates that the filter corresponding to the set channel-maps are configured.
                             Bit position of the channel-map is obtained by left shifting (1 (left shift) Channel_map)
                             }
        @h2xmle_default          {0xFFFFFFFE} */

   uint32_t channel_mask_msb;
   /**< Upper 32 bits of the channel mask. Each bit corresponds to channel map
        from 32 (PCM_CHANNEL_RW) to 63 (PCM_CUSTOM_CHANNEL_MAP_16).\n
        - A set bit indicates that the filter corresponding to the set channel-maps are configured.
        Bit position of the channel-map is obtained by left shifting (1 (left shift) (Channel_map - 32)). */

   /**< @h2xmle_description  {Upper 32 bits of the channel mask. Each bit corresponds to channel map
                             from 32 (PCM_CHANNEL_RW) to 63 (PCM_CUSTOM_CHANNEL_MAP_16).\n
                             -#A set bit indicates that the filter corresponding to the set channel-maps are configured.
                             Bit position of the channel-map is obtained by left shifting (1 (left shift) (Channel_map - 32))
                             }
        @h2xmle_default         {0xFFFFFFFF}  */

    uint32_t coef_width;
    /**< Indicates the bit width of the filter coefficients. */

    /**< @h2xmle_description  {Indicates the bit width of the filter coefficients.}
         @h2xmle_rangeList    {"16"=16;
                               "32"=32}
         @h2xmle_default      {16}   */

    uint16_t coef_q_factor;
    /**< Indicates the Q factor of the coefficients. */

    /**< @h2xmle_description  {Indicates the Q factor of the coefficients.}
         @h2xmle_range        {1..31}
         @h2xmle_default      {13}   */

    uint16_t num_taps;
    /**< Indicates the filter tap length. (num_taps should be lesser than fir_max_tap_length). */

    /**< @h2xmle_description  {Indicates the filter tap length. (num_taps should be lesser than fir_max_tap_length)}
         @h2xmle_range        {1..65535}
         @h2xmle_default      {16} */

    uint32_t filter_delay_in_samples;
    /**< Indicates the delay of the filter in samples.
         The client uses this field to configure the delay corresponding to
         the current filter design. IF this value is configured as anything
         apart from the default (0xFFFFFFFF), it overrides the num_taps value
         to report the filter delay. */

    /**< @h2xmle_description  {Indicates the delay of the filter in samples.
                               The client uses this field to configure the delay corresponding to
                               the current filter design. IF this value is configured as anything
                               apart from the default (0xFFFFFFFF), it overrides the num_taps value
                               to report the filter delay.}
        @h2xmle_rangeList    {"UNUSED"=0xFFFFFFFF}
        @h2xmle_range        {1..0xFFFFFFFF}
        @h2xmle_default      {0xFFFFFFFF}     */

    int32_t filter_coefficients[0];
    /**< Array of filter coefficients. The array size depends on the number of
         taps. If coef_width = 16, store the filter coefficients in the
         lower 16 bits of the 32-bit field, and sign extend it to
         32 bits.. */

    /**< @h2xmle_description  {Array of filter coefficients. The array size depends on the number of
                               taps. If coef_width = 16, store the filter coefficients in the
                               lower 16 bits of the 32-bit field, and sign extend it to
                               32 bits.}
         @h2xmle_variableArraySize {"num_taps"}
         @h2xmlx_expandStructs {false}
         @h2xmle_defaultList {8192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }*/

}
#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;

/* Typedef Structure for the filter config parameter of Fir filter module. */
typedef struct fir_filter_cfg_t fir_filter_cfg_t;

/** @ingroup ar_spf_mod_fir_macros
    Payload of the PARAM_ID_FIR_FILTER_CONFIG parameter used by the Fir filter module
*/

/** @h2xmlp_parameter   {"PARAM_ID_FIR_FILTER_CONFIG", PARAM_ID_FIR_FILTER_CONFIG }
    @h2xmlp_description {Payload of the PARAM_ID_FIR_FILTER_CONFIG parameter used by the Fir filter module}
    @h2xmlp_toolPolicy  {NO_SUPPORT} 
    @h2xmlx_expandStructs	{false}  */

#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"

/** Immediately following this structure are num_config structures of type
    fir_filter_cfg_t.
 */
struct param_id_fir_filter_config_t
{
   uint32_t num_config;
   /**< Specifies the different sets of filter coefficient configurations. */

   /**< @h2xmle_description  {Specifies the different sets of filter coefficient configurations.}
        @h2xmle_range        {1..CAPI_FIR_MAX_CHANNELS_V1}
        @h2xmle_default      {1}   */

   fir_filter_cfg_t config_data[0];
   /**< Specifies the different sets of filter coefficient configurations. */

   /**< @h2xmle_description {Specifies the different sets of filter coefficient configurations.}
        @h2xmle_variableArraySize {num_config}
        @h2xmle_default      {0} */

}
#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;

/* Typedef Structure for the filter coefficients parameter of Fir filter module. */
typedef struct param_id_fir_filter_config_t param_id_fir_filter_config_t;

/** @ingroup ar_spf_mod_fir_macros
    Typedef Structure for the filter coefficients parameter of FIR Filter module
*/

/** @h2xmlp_subStruct */

#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"
struct fir_filter_crossfade_cfg_t
{
   uint32_t channel_mask_lsb;
   /**< Lower 32 bits of the channel mask. Each bit corresponds to channel map
        from 1 (PCM_CHANNEL_L) to 31 (PCM_CHANNEL_LW).
        Bit 0 is reserved and must be set to zero.\n
        - A set bit indicates that the filter corresponding to the set channel-maps are configured.
        Bit position of the channel-map is obtained by left shifting (1 (left shift) Channel_map). */

   /**< @h2xmle_description  {Lower 32 bits of the channel mask. Each bit corresponds to channel map
                             from 1 (PCM_CHANNEL_L) to 31 (PCM_CHANNEL_LW).
                             Bit 0 is reserved and must be set to zero.\n
                             -#A set bit indicates that the filter corresponding to the set channel-maps are configured.
                             Bit position of the channel-map is obtained by left shifting (1 (left shift) Channel_map)
                             }
    @h2xmle_default          {0xFFFFFFFE} */

   uint32_t channel_mask_msb;
   /**< Upper 32 bits of the channel mask. Each bit corresponds to channel map
        from 32 (PCM_CHANNEL_RW) to 63 (PCM_CUSTOM_CHANNEL_MAP_16).\n
        - A set bit indicates that the filter corresponding to the set channel-maps are configured.
        Bit position of the channel-map is obtained by left shifting (1 (left shift) (Channel_map - 32)). */

   /**< @h2xmle_description  {Upper 32 bits of the channel mask. Each bit corresponds to channel map
                             from 32 (PCM_CHANNEL_RW) to 63 (PCM_CUSTOM_CHANNEL_MAP_16).\n
                             -#A set bit indicates that the filter corresponding to the set channel-maps are configured.
                             Bit position of the channel-map is obtained by left shifting (1 (left shift) (Channel_map - 32))
                             }
    @h2xmle_default         {0xFFFFFFFF}  */

   uint32_t fir_cross_fading_mode;
   /**< Specifies if the crossfading is enable or disable.
        - 0 -- Disable \n
        - 1 -- Enable */

    /**< @h2xmle_description  {Specifies if the crossfading is enable or disable. 0-Disable, 1-Enable}
    @h2xmle_rangeList    {"Disable"=0;
                          "Enable"=1}
    @h2xmle_default           {0} */

   uint32_t transition_period_ms;
   /**< Specifies Transition period in milli seconds in Q0, eg. 20 ==> 20 msec.. */

    /**< @h2xmle_description  {Specifies Transition period in milli seconds in Q0, eg. 20 ==> 20 msec.}
    @h2xmle_range             {0..50}
    @h2xmle_default           {20} */

}
#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;

typedef fir_filter_crossfade_cfg_t fir_filter_crossfade_cfg_t;

/** @ingroup ar_spf_mod_fir_macros
    Payload of the PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG parameter used by the Fir filter module
*/

/** @h2xmlp_parameter   {"PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG", PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG }
    @h2xmlp_description {Payload of the PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG parameter used by the Fir filter module}
    @h2xmlp_toolPolicy  {NO_SUPPORT} 
    @h2xmlx_expandStructs	{false}  */

#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"

/** Immediately following this structure are num_config structures of type
    fir_filter_crossfade_cfg_t.
 */

struct param_id_fir_filter_crossfade_cfg_t
{
   uint32_t num_config;
   /**< Specifies the different sets of filter crossfade configurations. */

   /**< @h2xmle_description  {Specifies the different sets of filter crossfade configurations.}
       @h2xmle_range        {1..CAPI_FIR_MAX_CHANNELS_V1}
       @h2xmle_default      {1}  */

   fir_filter_crossfade_cfg_t crossfade_config_data[0];
   /**< Specifies the different sets of filter crossfade configurations. */

   /**< @h2xmle_description {Specifies the different sets of filter crossfade configurations}
       @h2xmle_variableArraySize {num_config}
       @h2xmle_default      {0}   */

}

#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;

/** Typedef Structure for the filter coefficients parameter of FIR filter module. */

typedef struct param_id_fir_filter_crossfade_cfg_t param_id_fir_filter_crossfade_cfg_t;

/** @h2xmlp_subStruct */
#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"
struct fir_filter_max_tap_length_cfg_v2_t
{
   uint32_t channel_type_group_mask;
   /**< @h2xmle_description  {Indicates the mask for channel_type_mask_list array.
                             Each bit in channel_type_group_mask corresponds to a channel group.
                             Read as
                             Bit 0 corresponds to channel group 1, which includes channel map for channels 1-31.
                             Bit 1 corresponds to channel group 2, which includes channel map for channels 32-63.
                             Bit 2 corresponds to channel group 3, which includes channel map for channels 64-95.
                             Bit 3 corresponds to channel group 4, which includes channel map for channels 96-127.
                             Bit 4 corresponds to channel group 5, which includes channel map for channels 128-159.

                             A set bit (1) in channel_type_group_mask indicates that the channels in that channel group are configured.
                             }
    @h2xmle_range            {0..31}
    @h2xmle_default          {0x00000003} */


   uint32_t channel_type_mask_list[0];
   /**< @h2xmle_description  {An array used to configure the channels for different channel groups. The array size depends on the number of
                             bits set in channel_type_group_mask.\n
                             For group 1, each bit of channel_type_mask_list corresponds to channel map from 1 (PCM_CHANNEL_L) to 31 (PCM_CHANNEL_LW).\n
                             Bit 0 of group 1 channel_type_mask_list is reserved and must always be set to zero.\n
                             For any other group, each bit of channel_type_mask_list corresponds to channel map from [32(group_no -1) to 32(group_no)-1].\n
                             Bit position of the channel-map for channel_type_mask_list of defined group is obtained by left shifting (1 (left shift) Channel_map%32
                             }
    @h2xmle_variableArraySizeFunction {GET_SET_BITS_COUNT, channel_type_group_mask}
    @h2xmle_copySrcList      {channel_mask_lsb, channel_mask_msb}
    @h2xmle_defaultList      {0xfffffffe, 0xffffffff}  */

   uint32_t fir_max_tap_length;
   /**< @h2xmle_description  { Specifies the maximum tap length of the FIR filter. This value is
                               limited by resources (memory, MIPS, etc.) }
     @h2xmle_range           {0..4294967295}
     @h2xmle_copySrc         {fir_max_tap_length}
     @h2xmle_default         {512}
   */

}
#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;
typedef struct fir_filter_max_tap_length_cfg_v2_t fir_filter_max_tap_length_cfg_v2_t;

#define PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH_V2  0x8001A8D
/** @h2xmlp_parameter   {"PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH_V2", PARAM_ID_FIR_FILTER_MAX_TAP_LENGTH_V2 }
    @h2xmlp_copySrc     {0x0800105F}
    @h2xmlp_description { Structure for the max tap length parameter of Fir filter module.}
     @h2xmlp_toolPolicy  {RTC, Calibration}
     @h2xmlx_expandStructs {false}*/

#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"
/** Immediately following this structure are num_config structures of type
    fir_filter_max_tap_length_cfg_v2_t.
 */
struct param_id_fir_filter_max_tap_cfg_v2_t
{
   uint32_t num_config;
   /**< @h2xmle_description  {Specifies the different sets of filter coefficient configurations.}
        @h2xmle_copySrc      {num_config}
        @h2xmle_range        {1..MODULE_CMN_MAX_CHANNEL}
        @h2xmle_default      {1}  */
#ifdef __H2XML__
   fir_filter_max_tap_length_cfg_v2_t config_data[0];
   /**< @h2xmle_description {Specifies the different sets of filter coefficient configurations}
        @h2xmle_variableArraySize {num_config}
        @h2xmle_default      {0}   */
#endif
}
#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;

typedef struct param_id_fir_filter_max_tap_cfg_v2_t param_id_fir_filter_max_tap_cfg_v2_t;

/** @h2xmlp_subStruct */
#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"
struct fir_filter_cfg_v2_t
{
   uint32_t channel_type_group_mask;
   /**< @h2xmle_description  {Indicates the mask for channel_type_mask_list array.
                             Each bit in channel_type_group_mask corresponds to a channel group.
                             Read as
                             Bit 0 corresponds to channel group 1, which includes channel map for channels 1-31.
                             Bit 1 corresponds to channel group 2, which includes channel map for channels 32-63.
                             Bit 2 corresponds to channel group 3, which includes channel map for channels 64-95.
                             Bit 3 corresponds to channel group 4, which includes channel map for channels 96-127.
                             Bit 4 corresponds to channel group 5, which includes channel map for channels 128-159.

                             A set bit (1) in channel_type_group_mask indicates that the channels in that channel group are configured.
                             }
    @h2xmle_range            {0..31}
    @h2xmle_default          {0x00000003} */


   uint32_t channel_type_mask_list[0];
   /**< @h2xmle_description  {An array used to configure the channels for different channel groups. The array size depends on the number of
                             bits set in channel_type_group_mask.\n
                             For group 1, each bit of channel_type_mask_list corresponds to channel map from 1 (PCM_CHANNEL_L) to 31 (PCM_CHANNEL_LW).\n
                             Bit 0 of group 1 channel_type_mask_list is reserved and must always be set to zero.\n
                             For any other group, each bit of channel_type_mask_list corresponds to channel map from [32(group_no -1) to 32(group_no)-1].\n
                             Bit position of the channel-map for channel_type_mask_list of defined group is obtained by left shifting (1 (left shift) Channel_map%32
                             }
    @h2xmle_variableArraySizeFunction {GET_SET_BITS_COUNT, channel_type_group_mask}
    @h2xmle_copySrcList      {channel_mask_lsb, channel_mask_msb}
    @h2xmle_defaultList      {0xfffffffe, 0xffffffff}  */

   uint32_t coef_width;
    /**< @h2xmle_description  {Indicates the bit width of the filter coefficients.}
         @h2xmle_copySrc      {coef_width}
         @h2xmle_rangeList    {"16"=16;
                               "32"=32}
         @h2xmle_default      {16}   */

    uint16_t coef_q_factor;
    /**< @h2xmle_description  {Indicates the Q factor of the coefficients.}
         @h2xmle_copySrc      {coef_q_factor}
         @h2xmle_range        {1..31}
         @h2xmle_default      {13}   */


    uint16_t num_taps;
    /**< @h2xmle_description  {Indicates the filter tap length. (num_taps should be lesser than fir_max_tap_length)}
         @h2xmle_copySrc      {num_taps}
         @h2xmle_range        {1..65535}
         @h2xmle_default      {16} */


    uint32_t filter_delay_in_samples;
    /**< @h2xmle_description  {Indicates the delay of the filter in samples.
                               The client uses this field to configure the delay corresponding to
                               the current filter design. IF this value is configured as anything
                               apart from the default (0xFFFFFFFF), it overrides the num_taps value
                               to report the filter delay.}
        @h2xmle_copySrc      {filter_delay_in_samples}
        @h2xmle_rangeList    {"UNUSED"=0xFFFFFFFF}
        @h2xmle_range        {1..0xFFFFFFFF}
        @h2xmle_default      {0xFFFFFFFF}     */


    int32_t filter_coefficients[0];
    /**< @h2xmle_description  {Array of filter coefficients. The array size depends on the number of
                               taps. If coef_width = 16, store the filter coefficients in the
                               lower 16 bits of the 32-bit field, and sign extend it to
                               32 bits.}
         @h2xmle_copySrc      {filter_coefficients}
         @h2xmle_variableArraySize {"num_taps"}
         @h2xmlx_expandStructs {false}
         @h2xmle_defaultList {8192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }*/

}

#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;
typedef struct fir_filter_cfg_v2_t fir_filter_cfg_v2_t;


#define PARAM_ID_FIR_FILTER_CONFIG_V2 0x8001A8E
/** @h2xmlp_parameter   {"PARAM_ID_FIR_FILTER_CONFIG_V2", PARAM_ID_FIR_FILTER_CONFIG_V2 }
    @h2xmlp_copySrc     {0x0800111A}
    @h2xmlp_description {Payload of the PARAM_ID_FIR_FILTER_CONFIG_V2 parameter used by the Fir filter module}
    @h2xmlp_toolPolicy  {RTC, Calibration}
    @h2xmlx_expandStructs   {false} */

#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"
/** Immediately following this structure are num_config structures of type
    fir_filter_cfg_v2_t.
 */
struct param_id_fir_filter_config_v2_t
{
   uint32_t num_config;
   /**< @h2xmle_description  {Specifies the different sets of filter coefficient configurations.}
        @h2xmle_copySrc      {num_config}
        @h2xmle_range        {1..MODULE_CMN_MAX_CHANNEL}
        @h2xmle_default      {1}   */
#ifdef __H2XML__
   fir_filter_cfg_v2_t config_data[0];
   /**< @h2xmle_description {Specifies the different sets of filter coefficient configurations.}
        @h2xmle_variableArraySize {num_config}
        @h2xmle_default      {0} */
#endif 
}
#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;

/* Typedef Structure for the filter coefficients parameter of Fir filter module. */
typedef struct param_id_fir_filter_config_v2_t param_id_fir_filter_config_v2_t;

/** @h2xmlp_subStruct */
#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"
struct fir_filter_crossfade_cfg_v2_t
{
   uint32_t channel_type_group_mask;
   /**< @h2xmle_description  {Indicates the mask for channel_type_mask_list array.
                             Each bit in channel_type_group_mask corresponds to a channel group.
                             Read as
                             Bit 0 corresponds to channel group 1, which includes channel map for channels 1-31.
                             Bit 1 corresponds to channel group 2, which includes channel map for channels 32-63.
                             Bit 2 corresponds to channel group 3, which includes channel map for channels 64-95.
                             Bit 3 corresponds to channel group 4, which includes channel map for channels 96-127.
                             Bit 4 corresponds to channel group 5, which includes channel map for channels 128-159.
                             A set bit (1) in channel_type_group_mask indicates that the channels in that channel group are configured.
                             }
    @h2xmle_range            {0..31}
    @h2xmle_default          {0x00000003} */


   uint32_t channel_type_mask_list[0];
   /**< @h2xmle_description  {An array used to configure the channels for different channel groups. The array size depends on the number of
                             bits set in channel_type_group_mask.\n
                             For group 1, each bit of channel_type_mask_list corresponds to channel map from 1 (PCM_CHANNEL_L) to 31 (PCM_CHANNEL_LW).\n
                             Bit 0 of group 1 channel_type_mask_list is reserved and must always be set to zero.\n
                             For any other group, each bit of channel_type_mask_list corresponds to channel map from [32(group_no -1) to 32(group_no)-1].\n
                             Bit position of the channel-map for channel_type_mask_list of defined group is obtained by left shifting (1 (left shift) Channel_map%32
                             }
    @h2xmle_variableArraySizeFunction {GET_SET_BITS_COUNT, channel_type_group_mask}
    @h2xmle_copySrcList      {channel_mask_lsb, channel_mask_msb}
    @h2xmle_defaultList      {0xfffffffe, 0xffffffff}  */

   uint32_t fir_cross_fading_mode;
    /**< @h2xmle_description  {Specifies if the crossfading is enable or disable. 0-Disable, 1-Enable}
    @h2xmle_rangeList    {"Disable"=0;
                          "Enable"=1}
    @h2xmle_default           {0} */

   uint32_t transition_period_ms;
    /**< @h2xmle_description  {Specifies Transition period in milli seconds in Q0, eg. 20 ==> 20 msec.}
    @h2xmle_range             {0..50}
    @h2xmle_default           {20} */

}
#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;
/* Structure type def for above payload. */
typedef fir_filter_crossfade_cfg_v2_t fir_filter_crossfade_cfg_v2_t;

#define PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG_V2 0x8001A8F
/** @h2xmlp_parameter   {"PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG_V2", PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG_V2 }
    @h2xmlp_copySrc     {0x0800151D}
    @h2xmlp_description {Payload of the PARAM_ID_FIR_FILTER_CROSSFADE_CONFIG_V2 parameter used by the Fir filter module}
    @h2xmlp_toolPolicy  {RTC, Calibration} 
    @h2xmlx_expandStructs    {false}   */

#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"
/** Immediately following this structure are num_config structures of type
    fir_filter_crossfade_cfg_t.
 */

struct param_id_fir_filter_crossfade_cfg_v2_t
{
   uint32_t num_config;
    /**< @h2xmle_description  {Specifies the different sets of filter crossfade configurations.}
        @h2xmle_range        {1..MODULE_CMN_MAX_CHANNEL}
        @h2xmle_default      {1}  */
#ifdef __H2XML__
   fir_filter_crossfade_cfg_v2_t crossfade_config_data[0];
    /**< @h2xmle_description {Specifies the different sets of filter crossfade configurations}
        @h2xmle_variableArraySize {num_config}
        @h2xmle_default      {0}   */
#endif 
}

#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;

/* Typedef Structure for the filter coefficients parameter of Fir filter module. */
typedef struct param_id_fir_filter_crossfade_cfg_v2_t param_id_fir_filter_crossfade_cfg_v2_t;

/**
	@h2xml_Select					{param_id_module_enable_t}
   @h2xmlm_InsertParameter
*/
/**
	@h2xml_Select					{param_id_fir_filter_max_tap_cfg_t}
   @h2xmlm_InsertParameter
*/

/**
	@h2xml_Select					{fir_filter_max_tap_length_cfg_t}
   @h2xmlm_InsertParameter
*/

/**
	@h2xml_Select					{param_id_fir_filter_config_t}
   @h2xmlm_InsertParameter
*/

/**
	@h2xml_Select					{fir_filter_cfg_t}
   @h2xmlm_InsertParameter
*/

/**
	@h2xml_Select					{param_id_fir_filter_crossfade_cfg_t}
   @h2xmlm_InsertParameter
*/

/**
	@h2xml_Select					{fir_filter_crossfade_cfg_t}
   @h2xmlm_InsertParameter
*/
/**
    @h2xml_Select                    {param_id_fir_filter_max_tap_cfg_v2_t}
   @h2xmlm_InsertParameter
*/

/**
    @h2xml_Select                    {fir_filter_max_tap_length_cfg_v2_t}
   @h2xmlm_InsertParameter
*/

/**
    @h2xml_Select                    {param_id_fir_filter_config_v2_t}
   @h2xmlm_InsertParameter
*/

/**
    @h2xml_Select                    {fir_filter_cfg_v2_t}
   @h2xmlm_InsertParameter
*/

/**
    @h2xml_Select                    {param_id_fir_filter_crossfade_cfg_v2_t}
   @h2xmlm_InsertParameter
*/

/**
    @h2xml_Select                    {fir_filter_crossfade_cfg_v2_t}
   @h2xmlm_InsertParameter
*/

/** @}                   <-- End of the Module -->*/

#endif //API_FIR_H
