/**
 * \file gen_topo_fwk_extn_utils.c
 * \brief
 *     This file implements the framework extension utility functions
 *
 *
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#include "gen_topo.h"
#include "gen_topo_capi.h"
#include "apm_sub_graph_api.h"

/*--------------------------------------------------------------*/
/* Macro definitions                                            */
/* -------------------------------------------------------------*/

/* -----------------------------------------------------------------------
 ** Constant / Define Declarations
 ** ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 ** Function prototypes
 ** ----------------------------------------------------------------------- */

ar_result_t gen_topo_fmwk_extn_handle_at_init(gen_topo_t *topo_ptr, gen_topo_module_t *module_ptr)
{
   ar_result_t result = AR_EOK;
   ar_result_t ignore_result = AR_EUNSUPPORTED;

   if (module_ptr->flags.need_thresh_cfg_extn)
   {
      uint32_t new_thresh_us = 1000 * TOPO_PERF_MODE_TO_FRAME_DURATION_MS(module_ptr->gu.sg_ptr->perf_mode);
      fwk_extn_param_id_threshold_cfg_t fm_dur = {.duration_us = new_thresh_us };
      result |= gen_topo_capi_set_param(topo_ptr->gu.log_id,
                                        module_ptr->capi_ptr,
                                        FWK_EXTN_PARAM_ID_THRESHOLD_CFG,
                                        (int8_t *)&fm_dur,
                                        sizeof(fm_dur));
   }

   if (module_ptr->flags.need_trigger_policy_extn)
   {
      fwk_extn_param_id_trigger_policy_cb_fn_t handler;
      gen_topo_populate_trigger_policy_extn_vtable(module_ptr, &handler);
      result |= gen_topo_capi_set_param(topo_ptr->gu.log_id,
                                        module_ptr->capi_ptr,
                                        FWK_EXTN_PARAM_ID_TRIGGER_POLICY_CB_FN,
                                        (int8_t *)&handler,
                                        sizeof(handler));
   }

   // other framework extensions are handled in create_module callback to fwk
   // supressing unsupported error because SYNC modules trigger policy is supported under fwk-extenstion code.
   return (result & (~ignore_result));
}

ar_result_t gen_topo_fmwk_extn_handle_at_deinit(gen_topo_t *topo_ptr, gen_topo_module_t *module_ptr)
{
   ar_result_t result = AR_EOK;

   return result;
}


