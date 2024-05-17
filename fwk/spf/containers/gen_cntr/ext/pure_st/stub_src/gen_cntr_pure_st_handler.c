/**
 * \file gen_cntr_err_handler.c
 * \brief
 *     This file contains functions that do error handeling.
 *
 *
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#include "gen_cntr_utils.h"

ar_result_t gen_cntr_pure_st_process_frames(gen_cntr_t *me_ptr)
{
   return AR_EUNSUPPORTED;
}

/* Checks if pure signal triggered data process frames can be used for signal triggered containers.
   If there is any module with active trigger policy then it uses generic topology, else it uses
   pure signal triggered topology. */
ar_result_t gen_cntr_check_and_assign_st_data_process_fn(gen_cntr_t *me_ptr)
{
   // if the container is already downgraded to generic topology then no need to check further.
   if (!gen_cntr_is_pure_signal_triggered(me_ptr))
   {
      return AR_EOK;
   }

   me_ptr->topo.flags.cannot_be_pure_signal_triggered = TRUE;

   GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                DBG_LOW_PRIO,
                "This is NOT a pure signal trigger container=%lu ",
                gen_cntr_is_pure_signal_triggered(me_ptr),
                me_ptr->topo.flags.is_signal_triggered,
                num_data_tpm,
                num_signal_tpm);
   return AR_EOK;
}
