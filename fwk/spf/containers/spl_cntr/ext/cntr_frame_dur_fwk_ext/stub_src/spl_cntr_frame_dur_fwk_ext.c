/**
 * \file spl_cntr_frame_dur_fwk_ext.c
 *
 * \brief
 *     Implementation of stub implementation for FWK_EXTN_CONTAINER_FRAME_DURATION
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#include "spl_cntr_frame_dur_fwk_ext.h"

ar_result_t spl_cntr_fwk_extn_cntr_frame_duration_changed(spl_cntr_t *me_ptr, uint32_t cntr_frame_duration_us)
{
   return AR_EOK;
}

/*Send a set param to each module that raises the frame duration extension any time the frame duration changed*/
ar_result_t spl_cntr_set_cntr_frame_dur_per_module(spl_cntr_t *       me_ptr,
                                                   spl_topo_module_t *module_ptr,
                                                   uint32_t           frame_len_us)
{
   return AR_EOK;
}
