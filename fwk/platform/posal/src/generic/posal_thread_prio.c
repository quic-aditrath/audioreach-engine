/**
 * \file posal_thread_prio.c
 * \brief
 *   This file contains functional implementations that are exposed to the framework to be invoked in order to retrieve the thread priority.
 *
 * \copyright
 *    Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *    SPDX-License-Identifier: BSD-3-Clause
 */

/* =======================================================================
INCLUDE FILES FOR MODULE
========================================================================== */
#include "posal_thread_prio.h"
#include "thread_prio_devcfg.h"
#include "ar_msg.h"
/* =======================================================================
**                          Function Definitions
** ======================================================================= */
/*
 * Calculates the thread priority of a thread given a
 * duration in microseconds OR static thread ID.
 *
 * @param[in]  priority query
 *
 * @return
 * Thread priority value
 */
ar_result_t posal_thread_calc_prio(prio_query_t *prio_query_ptr, posal_thread_prio_t *thread_prio_ptr)
{
   if (NULL == prio_query_ptr)
   {
      AR_MSG(DBG_ERROR_PRIO, "Received null ptr for the thread prio query");
      return AR_EBADPARAM;
   }
   if (SPF_THREAD_DYN_ID == prio_query_ptr->static_req_id)
   {
      *thread_prio_ptr = spf_thread_calc_dyn_prio(prio_query_ptr->frame_duration_us, prio_query_ptr->is_interrupt_trig);
   }
   else
   {
      *thread_prio_ptr = spf_thread_calc_static_prio(prio_query_ptr->static_req_id, prio_query_ptr->is_interrupt_trig);
   }
   return AR_EOK;
}

/**
 * Gets a default low priority for the passed in prio_id. Currently only implemented
 * for SPF_THREAD_STAT_CNTR_ID.
 */
posal_thread_prio_t posal_thread_get_floor_prio(spf_thread_prio_id_t prio_id)
{
   return spf_thread_calc_floor_prio(prio_id);
}
