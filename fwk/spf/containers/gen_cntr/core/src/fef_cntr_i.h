#ifndef FEF_CNTR_H
#define FEF_CNTR_H

/**
 * \file fef_cntr.h
 *
 * \brief
 *     Compression-Decompression Container
 *
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#include "gen_cntr_cmn_utils.h"
#include "spf_utils.h"
#include "container_utils.h"

#include "gen_cntr_i.h"

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

/**
 Disable the Frontend fwk timer.

 @param [in]  me_ptr  gen cntr ptr

 @dependencies
 None.
 */
ar_result_t fef_cntr_disable_timer(gen_cntr_t *me_ptr);


/**
 Enable the Frontend fwk timer.

 @param [in]  me_ptr  gen cntr ptr

 @dependencies
 None.
 */
ar_result_t fef_cntr_enable_timer(gen_cntr_t *me_ptr);


/** ----------FRONT End container utilities -------- **/
void fef_cntr_timer_isr_cb(void* handle);
ar_result_t fef_cntr_signal_trigger(cu_base_t *cu_ptr, uint32_t channel_bit_index);

#ifdef __cplusplus
}
#endif //__cplusplus
#endif //FEF_CNTR_H
