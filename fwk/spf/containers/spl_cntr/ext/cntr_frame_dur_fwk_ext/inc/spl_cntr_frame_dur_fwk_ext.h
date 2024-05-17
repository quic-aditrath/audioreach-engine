#ifndef SPL_CNTR_CONTAINER_FRAME_DUR_FWK_H
#define SPL_CNTR_CONTAINER_FRAME_DUR_FWK_H
/**
 * // clang-format off
 * \file spl_cntr_frame_dur_fwk_ext.h
 * \brief
 *  This file contains utility functions for FWK_EXTN_CONTAINER_FRAME_DURATION
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */
// clang-format on

#include "spl_cntr_i.h"
#include "spl_topo.h"

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

typedef struct spl_cntr_t spl_cntr_t;

ar_result_t spl_cntr_fwk_extn_cntr_frame_duration_changed(spl_cntr_t *me_ptr, uint32_t cntr_frame_duration_us);

ar_result_t spl_cntr_fwk_extn_set_cntr_frame_duration_per_module(spl_cntr_t *       me_ptr,
                                                                 spl_topo_module_t *module_ptr,
                                                                 uint32_t           frame_len_us);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif /* SPL_CNTR_CONTAINER_FRAME_DUR_FWK_H */
