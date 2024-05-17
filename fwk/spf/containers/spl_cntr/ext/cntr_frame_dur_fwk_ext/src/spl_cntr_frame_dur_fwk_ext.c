/**
 * \file spl_cntr_frame_dur_fwk_ext.c
 *
 * \brief
 *  This file contains functions for FWK_EXTN_CONTAINER_FRAME_DURATION in spl container
 *
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause */

/* =======================================================================
Includes
========================================================================== */
#include "spl_cntr_i.h"

/* =======================================================================
Function definitions
========================================================================== */
/**
 * Send a set param to each module that raises the frame duration extension any time the frame duration changed.
 */
ar_result_t spl_cntr_fwk_extn_cntr_frame_duration_changed(spl_cntr_t *me_ptr, uint32_t cntr_frame_duration_us)
{
   ar_result_t result = AR_EOK;
   for (gu_sg_list_t *sg_list_ptr = me_ptr->topo.t_base.gu.sg_list_ptr; (NULL != sg_list_ptr);
        LIST_ADVANCE(sg_list_ptr))
   {
      for (gu_module_list_t *module_list_ptr = sg_list_ptr->sg_ptr->module_list_ptr; (NULL != module_list_ptr);
           LIST_ADVANCE(module_list_ptr))
      {
         spl_topo_module_t *curr_module_ptr = (spl_topo_module_t *)module_list_ptr->module_ptr;

         if (FALSE == curr_module_ptr->t_base.flags.need_cntr_frame_dur_extn)
         {
            continue;
         }

         result = spl_cntr_fwk_extn_set_cntr_frame_duration_per_module(me_ptr, curr_module_ptr, cntr_frame_duration_us);
      }
   }
   return result;
}

/**
 * Send a set param to each module that raises the frame duration extension any time the frame duration changed.
 */

ar_result_t spl_cntr_fwk_extn_set_cntr_frame_duration_per_module(spl_cntr_t *       me_ptr,
                                                                 spl_topo_module_t *module_ptr,
                                                                 uint32_t           frame_len_us)
{
   ar_result_t result = AR_EOK;

   fwk_extn_param_id_container_frame_duration_t payload;
   payload.duration_us = frame_len_us;

   result = gen_topo_capi_set_param(me_ptr->topo.t_base.gu.log_id,
                                      module_ptr->t_base.capi_ptr,
                                      FWK_EXTN_PARAM_ID_CONTAINER_FRAME_DURATION,
                                      (int8_t *)&payload,
                                      sizeof(fwk_extn_param_id_container_frame_duration_t));

   if ((result != AR_EOK) && (result != AR_EUNSUPPORTED))
   {
      SPL_CNTR_MSG(me_ptr->topo.t_base.gu.log_id,
                   DBG_ERROR_PRIO,
                   "Module 0x%lX: setting cntr frame dur %d to fwk extn modules failed",
                   module_ptr->t_base.gu.module_instance_id,
                   frame_len_us);
      return result;
   }
   else
   {
      SPL_CNTR_MSG(me_ptr->topo.t_base.gu.log_id,
                   DBG_HIGH_PRIO,
                   "Module 0x%lX: setting cntr frame dur %d to fwk extn modules success",
                   module_ptr->t_base.gu.module_instance_id,
                   frame_len_us);
   }

   return AR_EOK;
}
