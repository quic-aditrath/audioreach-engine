/**
 * \file apm_set_get_cfg_utils.c
 *
 * \brief
 *     This file contains stubbed implementation for
 *     APM Set Get Cfg Utilities
 *
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

/****************************************************************************
 * INCLUDE HEADER FILES                                                     *
 ****************************************************************************/

#include "apm_internal.h"
#include "apm_ext_cmn.h"

/****************************************************************************
 * Function Definitions
 ****************************************************************************/

ar_result_t apm_set_get_cfg_utils_init(apm_t *apm_info_ptr)
{
   apm_info_ptr->ext_utils.set_get_cfg_vtbl_ptr = NULL;

   return AR_EOK;
}
