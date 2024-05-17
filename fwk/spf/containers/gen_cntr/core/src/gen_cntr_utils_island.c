/**
 * \file gen_cntr_utils_island.c
 * \brief
 *     This file contains utility functions for GEN_CNTR
 *
 *
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#include "gen_cntr_i.h"

ar_result_t gen_cntr_handle_frame_done(gen_topo_t *topo_ptr, uint8_t path_index)
{
   gen_cntr_t *me_ptr = (gen_cntr_t *)GET_BASE_PTR(gen_cntr_t, topo, topo_ptr);
   ar_result_t result = AR_EOK;

   bool_t need_to_handle_frame_done = FALSE;

   for (gu_ext_in_port_list_t *ext_in_port_list_ptr = me_ptr->cu.gu_ptr->ext_in_port_list_ptr; ext_in_port_list_ptr;
        LIST_ADVANCE(ext_in_port_list_ptr))
   {
      gen_cntr_ext_in_port_t *ext_in_port_ptr = (gen_cntr_ext_in_port_t *)ext_in_port_list_ptr->ext_in_port_ptr;

      // if external port is in the same path where frame is delivered and threshold is enabled
      if (ext_in_port_ptr->cu.preserve_prebuffer &&
          (path_index == ext_in_port_ptr->gu.int_in_port_ptr->cmn.module_ptr->path_index))
      {
         // requeue prebuffers
         gen_topo_exit_island_temporarily(topo_ptr);
         cu_ext_in_requeue_prebuffers(&me_ptr->cu, &ext_in_port_ptr->gu);
         ext_in_port_ptr->cu.preserve_prebuffer = FALSE;
      }

      if (ext_in_port_ptr->cu.preserve_prebuffer)
      {
         // if at least one external input port is using prebuffer Q then continue to handle frame_done
         need_to_handle_frame_done = TRUE;
      }
   }

   if (AR_SUCCEEDED(cu_raise_frame_done_event(&me_ptr->cu, me_ptr->topo.gu.log_id)))
   {
      // frame done event is registered then continue to handle frame_done subsequent frames
      need_to_handle_frame_done = TRUE;
   }

   me_ptr->topo.flags.need_to_handle_frame_done = need_to_handle_frame_done;
   return result;
}

ar_result_t gen_cntr_handle_ext_in_data_flow_begin(gen_cntr_t *me_ptr, gen_cntr_ext_in_port_t *ext_in_port_ptr)
{
   ar_result_t            result      = AR_EOK;
   gen_topo_input_port_t *in_port_ptr = (gen_topo_input_port_t *)ext_in_port_ptr->gu.int_in_port_ptr;
   bool_t                 prev_dfs    = in_port_ptr->common.data_flow_state;
   gen_topo_handle_data_flow_begin(&me_ptr->topo, &in_port_ptr->common, &in_port_ptr->gu.cmn);

   if ((prev_dfs != in_port_ptr->common.data_flow_state) &&
       (TOPO_DATA_FLOW_STATE_FLOWING == in_port_ptr->common.data_flow_state))
   {
      if (FALSE == me_ptr->topo.flags.defer_voting_on_dfs_change)
      {
         CU_SET_ONE_FWK_EVENT_FLAG(&me_ptr->cu, dfs_change);
      }

      GEN_CNTR_MSG_ISLAND(me_ptr->topo.gu.log_id,
                          DBG_HIGH_PRIO,
                          "Input of miid 0x%lx defer_voting_on_dfs_change:%lu",
                          in_port_ptr->gu.cmn.module_ptr->module_instance_id,
                          me_ptr->topo.flags.defer_voting_on_dfs_change);

      gen_cntr_handle_fwk_events_in_data_path(me_ptr);
   }

   return result;
}

ar_result_t gen_cntr_event_data_to_dsp_client_v2_topo_cb(gen_topo_module_t *module_ptr,
                                                         capi_event_info_t *event_info_ptr)
{
   gen_cntr_t *me_ptr = (gen_cntr_t *)GET_BASE_PTR(gen_cntr_t, topo, module_ptr->topo_ptr);
   return cu_handle_event_data_to_dsp_client_v2_topo_cb(&me_ptr->cu, &module_ptr->gu, event_info_ptr);
}

ar_result_t gen_cntr_raise_data_to_dsp_service_event(gen_topo_module_t *module_ptr, capi_event_info_t *event_info_ptr)
{
   gen_cntr_t *me_ptr = (gen_cntr_t *)GET_BASE_PTR(gen_cntr_t, topo, module_ptr->topo_ptr);
   ar_result_t result = AR_EOK;

   // try to handle the event in island, if not handle in non-island
   bool_t handled_event_within_island = FALSE;
   result =
      gen_cntr_raise_data_to_dsp_service_event_within_island(module_ptr, event_info_ptr, &handled_event_within_island);

   // Exit island and handle if it cannot be handled within island
   if (FALSE == handled_event_within_island)
   {
      // Exit island if in island before processing any event
      gen_cntr_vote_against_island((void *)&me_ptr->cu);

      result = gen_cntr_raise_data_to_dsp_service_event_non_island(module_ptr, event_info_ptr);
   }
#ifdef VERBOSE_DEBUGGING
   else
   {
      // handled the event within island with result
      GEN_CNTR_MSG_ISLAND(me_ptr->topo.gu.log_id,
                          DBG_HIGH_PRIO,
                          "Handled the event callback within island result: 0x%lx",
                          result);
   }
#endif

   return result;
}

ar_result_t gen_cntr_raise_data_to_dsp_service_event_within_island(gen_topo_module_t *module_ptr,
                                                                   capi_event_info_t *event_info_ptr,
                                                                   bool_t *           handled_event_within_island)
{
   gen_cntr_t *                      me_ptr        = (gen_cntr_t *)GET_BASE_PTR(gen_cntr_t, topo, module_ptr->topo_ptr);
   ar_result_t                       result        = AR_EOK;
   capi_buf_t *                      payload       = &event_info_ptr->payload;
   capi_event_data_to_dsp_service_t *dsp_event_ptr = (capi_event_data_to_dsp_service_t *)(payload->data_ptr);

   *handled_event_within_island = TRUE;

   switch (dsp_event_ptr->param_id)
   {
      case INTF_EXTN_EVENT_ID_IMCL_RECURRING_BUF_INFO:
      case INTF_EXTN_EVENT_ID_IMCL_OUTGOING_DATA:
      {
         // Exit island if ctrl lib is compiled in in nlpi
         cu_exit_lpi_temporarily_if_ctrl_port_lib_in_nlpi(&me_ptr->cu);

         result = cu_handle_imcl_event(&me_ptr->cu, &module_ptr->gu, event_info_ptr);
         break;
      }
      default:
      {
         *handled_event_within_island = FALSE;
         result                       = AR_EUNSUPPORTED;
      }
   }

   return result;
}

ar_result_t gen_cntr_raise_data_from_dsp_service_event(gen_topo_module_t *module_ptr, capi_event_info_t *event_info_ptr)
{
   gen_cntr_t *                            me_ptr  = (gen_cntr_t *)GET_BASE_PTR(gen_cntr_t, topo, module_ptr->topo_ptr);
   ar_result_t                             result  = AR_EOK;
   capi_buf_t *                            payload = &event_info_ptr->payload;
   capi_event_get_data_from_dsp_service_t *dsp_event_ptr =
      (capi_event_get_data_from_dsp_service_t *)(payload->data_ptr);

   switch (dsp_event_ptr->param_id)
   {
      default:
      {
         return cu_handle_event_from_dsp_service_topo_cb(&me_ptr->cu, &module_ptr->gu, event_info_ptr);
      }
   }

   return result;
}

// Exit island till thread goes to sleep
void gen_ctr_exit_island_temporarily(void *cu_ptr)
{
   gen_cntr_t *me_ptr = (gen_cntr_t *)cu_ptr;

   gen_topo_exit_island_temporarily(&me_ptr->topo);
}

ar_result_t gen_cntr_vote_against_island_topo(gen_topo_t *topo_ptr)
{
   gen_cntr_t *me_ptr = (gen_cntr_t *)GET_BASE_PTR(gen_cntr_t, topo, topo_ptr);

   return gen_cntr_vote_against_island((void *)&me_ptr->cu);
}

// After exiting island, stays in nlpi for 2 frames
ar_result_t gen_cntr_vote_against_island(void *cu_ptr)
{
   ar_result_t result = AR_EOK;
   gen_cntr_t *me_ptr = (gen_cntr_t *)cu_ptr;

   gen_topo_exit_island_temporarily(&me_ptr->topo);

   posal_pm_island_vote_t island_vote;
   island_vote.island_vote_type = PM_ISLAND_VOTE_EXIT;
   gen_cntr_update_island_vote(me_ptr, island_vote);

   return result;
}

ar_result_t gen_cntr_ext_out_port_basic_reset(gen_cntr_t *me_ptr, gen_cntr_ext_out_port_t *ext_out_port_ptr)
{
   ar_result_t result = AR_EOK;

   gen_topo_output_port_t *out_port_ptr = (gen_topo_output_port_t *)ext_out_port_ptr->gu.int_out_port_ptr;

   topo_basic_reset_output_port(&me_ptr->topo, out_port_ptr, TRUE);

   gen_cntr_ext_out_port_int_reset(me_ptr, ext_out_port_ptr);

   return result;
}

ar_result_t gen_cntr_ext_out_port_int_reset(gen_cntr_t *me_ptr, gen_cntr_ext_out_port_t *ext_out_port_ptr)
{
   ar_result_t result = AR_EOK;

   gen_topo_output_port_t *out_port_ptr = (gen_topo_output_port_t *)ext_out_port_ptr->gu.int_out_port_ptr;

   ext_out_port_ptr->cu.icb_info.is_prebuffer_sent = FALSE;

   if (ext_out_port_ptr->cu.out_bufmgr_node.buf_ptr)
   {
      gen_cntr_return_back_out_buf(me_ptr, ext_out_port_ptr);
   }

   ext_out_port_ptr->buf.data_ptr        = NULL;
   ext_out_port_ptr->buf.actual_data_len = 0;
   ext_out_port_ptr->buf.max_data_len    = 0;

   memset(&ext_out_port_ptr->next_out_buf_ts, 0, sizeof(ext_out_port_ptr->next_out_buf_ts));

   GEN_CNTR_MSG_ISLAND(me_ptr->topo.gu.log_id,
                       DBG_LOW_PRIO,
                       "External output reset Module 0x%lX, Port 0x%lx",
                       out_port_ptr->gu.cmn.module_ptr->module_instance_id,
                       out_port_ptr->gu.cmn.id);

   return result;
}

void gen_cntr_clear_ext_out_bufs(gen_cntr_ext_out_port_t *ext_port_ptr, bool_t clear_max)
{
   ext_port_ptr->buf.data_ptr        = NULL;
   ext_port_ptr->buf.actual_data_len = 0;
   if (clear_max)
   {
      ext_port_ptr->buf.max_data_len = 0;
   }

   if (ext_port_ptr->bufs_num)
   {
      for (uint32_t b = 0; b < ext_port_ptr->bufs_num; b++)
      {
         ext_port_ptr->bufs_ptr[b].data_ptr        = NULL;
         ext_port_ptr->bufs_ptr[b].actual_data_len = 0;
         if (clear_max)
         {
            ext_port_ptr->bufs_ptr[b].max_data_len = 0;
         }
      }
   }
}

bool_t gen_cntr_is_data_present_in_ext_in_bufs(gen_cntr_ext_in_port_t *ext_in_port_ptr)
{
   bool_t is_data_present = FALSE;
   if (!gen_cntr_is_ext_in_v2(ext_in_port_ptr))
   {
      is_data_present = (ext_in_port_ptr->buf.actual_data_len != 0);
   }
   else
   {
      is_data_present = TRUE;
      for (uint32_t b = 0; b < ext_in_port_ptr->bufs_num; b++)
      {
         if (0 == ext_in_port_ptr->bufs_ptr[b].actual_data_len)
         {
            is_data_present = FALSE;
            break;
         }
      }
   }

   return is_data_present;
}

bool_t gen_cntr_is_data_present_in_ext_out_bufs(gen_cntr_ext_out_port_t *ext_out_port_ptr)
{
   bool_t is_data_present = FALSE;
   if (!gen_cntr_is_ext_out_v2(ext_out_port_ptr))
   {
      is_data_present = (ext_out_port_ptr->buf.actual_data_len != 0);
   }
   else
   {
      is_data_present = TRUE;
      for (uint32_t b = 0; b < ext_out_port_ptr->bufs_num; b++)
      {
         if (0 == ext_out_port_ptr->bufs_ptr[b].actual_data_len)
         {
            is_data_present = FALSE;
            break;
         }
      }
   }

   return is_data_present;
}

void gen_cntr_get_ext_out_total_actual_max_data_len(gen_cntr_ext_out_port_t *ext_out_port_ptr,
                                                    uint32_t *               actual_data_len_ptr,
                                                    uint32_t *               max_data_len_ptr)
{
   if ((!actual_data_len_ptr) && (!max_data_len_ptr))
   {
      return;
   }

   uint32_t actual_data_len = 0, max_data_len = 0;

   if (gen_cntr_is_ext_out_v2(ext_out_port_ptr))
   {
      for (uint32_t b = 0; b < ext_out_port_ptr->bufs_num; b++)
      {
         actual_data_len += ext_out_port_ptr->bufs_ptr[b].actual_data_len;
         max_data_len += ext_out_port_ptr->bufs_ptr[b].max_data_len;
      }
   }
   else
   {
      actual_data_len = ext_out_port_ptr->buf.actual_data_len;
      max_data_len    = ext_out_port_ptr->buf.max_data_len;
   }

   if (actual_data_len_ptr)
   {
      *actual_data_len_ptr = actual_data_len;
   }

   if (max_data_len_ptr)
   {
      *max_data_len_ptr = max_data_len;
   }
}

uint32_t gen_cntr_get_ext_in_total_actual_data_len(gen_cntr_ext_in_port_t *ext_in_port_ptr)
{
   uint32_t actual_data_len = 0;

   if (gen_cntr_is_ext_in_v2(ext_in_port_ptr))
   {
      for (uint32_t b = 0; b < ext_in_port_ptr->bufs_num; b++)
      {
         actual_data_len += ext_in_port_ptr->bufs_ptr[b].actual_data_len;
      }
   }
   else
   {
      actual_data_len = ext_in_port_ptr->buf.actual_data_len;
   }

   return actual_data_len;
}

/* This is in line with assuming first channel for dein raw compr in gen_topo_get_bytes_for_md_prop_from_len_per_buf and
 * gen_topo_get_actual_len_for_md_prop*/
uint32_t gen_cntr_get_bytes_in_ext_in_for_md(gen_cntr_ext_in_port_t *ext_in_port_ptr)
{
   uint32_t actual_data_len = 0;

   if (gen_cntr_is_ext_in_v2(ext_in_port_ptr))
   {
      actual_data_len += ext_in_port_ptr->bufs_ptr[0].actual_data_len;
   }
   else
   {
      actual_data_len = ext_in_port_ptr->buf.actual_data_len;
   }

   return actual_data_len;
}

/* This is in line with assuming first channel for dein raw compr in gen_topo_get_bytes_for_md_prop_from_len_per_buf and
 * gen_topo_get_actual_len_for_md_prop*/
uint32_t gen_cntr_get_bytes_in_ext_out_for_md(gen_cntr_ext_out_port_t *ext_out_port_ptr)
{
   uint32_t actual_data_len = 0;

   if (gen_cntr_is_ext_out_v2(ext_out_port_ptr))
   {
      actual_data_len += ext_out_port_ptr->bufs_ptr[0].actual_data_len;
   }
   else
   {
      actual_data_len = ext_out_port_ptr->buf.actual_data_len;
   }

   return actual_data_len;
}