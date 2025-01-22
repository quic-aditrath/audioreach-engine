/**
 *   \file capi_audio_dam_buffer_island.c
 *   \brief
 *        This file contains CAPI implementation of Audio Dam buffer module
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#include "capi_audio_dam_buffer_i.h"

static capi_vtbl_t vtbl = { capi_audio_dam_buffer_process,        capi_audio_dam_buffer_end,
                            capi_audio_dam_buffer_set_param,      capi_audio_dam_buffer_get_param,
                            capi_audio_dam_buffer_set_properties, capi_audio_dam_buffer_get_properties };

capi_vtbl_t *capi_audio_dam_buffer_get_vtable()
{
   return &vtbl;
}

static capi_err_t capi_dam_insert_flushing_eos_at_out_port(capi_audio_dam_t   *me_ptr,
                                                           capi_stream_data_t *output,
                                                           bool_t              skip_voting_on_eos);

capi_err_t get_output_arr_index_from_ctrl_port_id(capi_audio_dam_t *me_ptr,
                                                  uint32_t          ctrl_port_id,
                                                  uint32_t         *num_out_ports_ptr,
                                                  uint32_t          out_port_arr_idxs[])
{
   bool_t mapping_found = false;
   for (uint32_t arr_index = 0; arr_index < me_ptr->max_output_ports; arr_index++)
   {
      if (ctrl_port_id == me_ptr->out_port_info_arr[arr_index].ctrl_port_id)
      {
         out_port_arr_idxs[*num_out_ports_ptr] = arr_index;
         (*num_out_ports_ptr)++;

         DAM_MSG_ISLAND(me_ptr->miid,
                        DBG_HIGH_PRIO,
                        "capi_audio_dam: Mapping found Ctrl Port ID = 0x%lx to Out port id = %lu",
                        ctrl_port_id,
                        me_ptr->out_port_info_arr[arr_index].port_id);
         mapping_found = TRUE;
      }
   }

   if (mapping_found)
   {
      return AR_EOK;
   }

   DAM_MSG_ISLAND(me_ptr->miid,
                  DBG_ERROR_PRIO,
                  "capi_audio_dam: Port ID = 0x%lx to index mapping not found.",
                  ctrl_port_id);
   return AR_EBADPARAM;
}

uint32_t capi_dam_get_ctrl_port_arr_idx_from_ctrl_port_id(capi_audio_dam_t *me_ptr, uint32_t ctrl_port_id)
{
   uint32_t available_ctrl_arr_idx = me_ptr->max_output_ports;
   for (uint32_t idx = 0; idx < me_ptr->max_output_ports; idx++)
   {
      if (ctrl_port_id == me_ptr->imcl_port_info_arr[idx].port_id)
      {
         return idx;
      }
      else if (!me_ptr->imcl_port_info_arr[idx].port_id)
      {
         available_ctrl_arr_idx = idx;
      }
   }

   if (available_ctrl_arr_idx != me_ptr->max_output_ports)
   {
      DAM_MSG_ISLAND(me_ptr->miid,
                     DBG_LOW_PRIO,
                     "capi_audio_dam: Mapping Ctrl Port ID =0x%x to index=0x%x",
                     ctrl_port_id,
                     available_ctrl_arr_idx);
      me_ptr->imcl_port_info_arr[available_ctrl_arr_idx].port_id = ctrl_port_id;
      return available_ctrl_arr_idx;
   }

   DAM_MSG_ISLAND(me_ptr->miid,
                  DBG_ERROR_PRIO,
                  "capi_audio_dam: Ctrl Port ID = 0x%lx to index mapping not found.",
                  ctrl_port_id);
   return UMAX_32;
}

/*------------------------------------------------------------------------
  Function name: capi_audio_dam_buffer_set_param
  Sets either a parameter value or a parameter structure containing multiple
  parameters. In the event of a failure, the appropriate error code is
  returned.
 * -----------------------------------------------------------------------*/
capi_err_t capi_audio_dam_buffer_set_param(capi_t                 *capi_ptr,
                                           uint32_t                param_id,
                                           const capi_port_info_t *port_info_ptr,
                                           capi_buf_t             *params_ptr)
{
   capi_err_t        result = CAPI_EOK;
   capi_audio_dam_t *me_ptr = (capi_audio_dam_t *)((capi_ptr));

   if (param_id == INTF_EXTN_PARAM_ID_IMCL_INCOMING_DATA)
   {
      /*TBD: Currently port ID is in the port info. We should change this later.*/
      if (NULL == params_ptr->data_ptr)
      {
         DAM_MSG_ISLAND(me_ptr->miid, DBG_ERROR_PRIO, "capi_dam: Set param id 0x%lx, received null buffer", param_id);
         return CAPI_EBADPARAM;
      }

      // Level 1 check
      if (params_ptr->actual_data_len < MIN_INCOMING_IMCL_PARAM_SIZE_SVA_DAM)
      {
         DAM_MSG_ISLAND(me_ptr->miid,
                        DBG_ERROR_PRIO,
                        "CAPI V2 DAM: Invalid payload size for incoming data %d",
                        params_ptr->actual_data_len);
         return CAPI_ENEEDMORE;
      }

      intf_extn_param_id_imcl_incoming_data_t *payload_ptr =
         (intf_extn_param_id_imcl_incoming_data_t *)params_ptr->data_ptr;

      uint32_t ctrl_port_id = payload_ptr->port_id;

      result = capi_audio_dam_imc_set_param_handler(me_ptr, ctrl_port_id, params_ptr);

      DAM_MSG_ISLAND(me_ptr->miid, DBG_HIGH_PRIO, "capi_audio_dam: IMC set param handler result 0x%x", result);
      return result;
   }

   return capi_audio_dam_buffer_set_param_non_island(capi_ptr, param_id, port_info_ptr, params_ptr);
}

static capi_err_t capi_audio_dam_handle_and_drop_metadata(capi_audio_dam_t   *me_ptr,
                                                          capi_stream_data_t *input,
                                                          uint32_t            ip_port_index)
{
   capi_err_t             capi_result   = CAPI_EOK;
   capi_stream_data_v2_t *in_stream_ptr = (capi_stream_data_v2_t *)input;

   for (module_cmn_md_list_t *node_ptr = in_stream_ptr->metadata_list_ptr; node_ptr;)
   {
      bool_t                IS_DROPPED_TRUE = TRUE;
      module_cmn_md_list_t *next_ptr        = node_ptr->next_ptr;

      // handle pcm frame metadata
      if (MD_ID_ENCODER_FRAME_LENGTH_INFO == node_ptr->obj_ptr->metadata_id)
      {
         capi_audio_dam_handle_pcm_frame_info_metadata(me_ptr,
                                                       input,
                                                       ip_port_index,
                                                       (md_encoder_pcm_frame_length_t *)&node_ptr->obj_ptr
                                                          ->metadata_buf[0]);
      }
      else if (MODULE_CMN_MD_ID_EOS == node_ptr->obj_ptr->metadata_id)
      {
         DAM_MSG_ISLAND(me_ptr->miid, DBG_HIGH_PRIO, "Resetting read position on EOS (drops buffered data)");

         for (uint32_t op_arr_index = 0; op_arr_index < me_ptr->max_output_ports; op_arr_index++)
         {
            // adjust rd ptr inorder to drop all the data
            uint32_t read_offset_in_us = 0;
            audio_dam_stream_read_adjust(me_ptr->out_port_info_arr[op_arr_index].strm_reader_ptr,
                                         read_offset_in_us,
                                         NULL,
                                         FALSE);
         }
      }

      if (me_ptr->metadata_handler.metadata_destroy)
      {
         me_ptr->metadata_handler.metadata_destroy(me_ptr->metadata_handler.context_ptr,
                                                   node_ptr,
                                                   IS_DROPPED_TRUE,
                                                   &in_stream_ptr->metadata_list_ptr);
      }
      else
      {
         return CAPI_EFAILED;
      }
      node_ptr = next_ptr;
   }

   // EOF/EOS will be dropped from the module.
   in_stream_ptr->flags.end_of_frame = FALSE;
   in_stream_ptr->flags.marker_eos   = FALSE;

   return capi_result;
}

/*------------------------------------------------------------------------
  Function name: capi_audio_dam_buffer_process
  Processes an input buffer and generates an output buffer.
 * -----------------------------------------------------------------------*/
capi_err_t capi_audio_dam_buffer_process(capi_t *capi_ptr, capi_stream_data_t *input[], capi_stream_data_t *output[])
{
   capi_err_t result = CAPI_EOK;

   if (NULL == capi_ptr)
   {
      AR_MSG(DBG_ERROR_PRIO, "capi_audio_dam: received bad property pointer");
      return CAPI_EFAILED;
   }

   if ((NULL == input) && (NULL == output))
   {
      return result;
   }

   capi_audio_dam_t *me_ptr = (capi_audio_dam_t *)capi_ptr;

   if (input)
   {
      for (uint32_t arr_idx = 0; arr_idx < me_ptr->max_input_ports; arr_idx++)
      {
         // Check the port index from arr index
         uint32_t port_index = me_ptr->in_port_info_arr[arr_idx].port_index;

         // input port buffers sanity check
         // if buffer ptrs are null return
         if (NULL == input[port_index] || (NULL == input[port_index]->buf_ptr))
         {
            continue;
         }

         result |= capi_audio_dam_handle_and_drop_metadata(me_ptr, input[port_index], port_index);

         // input port buffers sanity check
         if ((NULL == me_ptr->in_port_info_arr[arr_idx].strm_writer_ptr) ||
             (NULL == input[port_index]->buf_ptr[0].data_ptr))
         {
            continue;
         }

         // Usually num configured inp chs is same as input media format, even if they are not same module handles
         // graciously in the following way,
         // 1. If num inp chs is less than configured chs, module buffers available inp num chs and ignore other chs.
         // 2. if num inp chs is greater than configured, module should buffer configured chs and drop others.
         uint32_t actual_input_chs =
            MIN(me_ptr->in_port_info_arr[arr_idx].strm_writer_ptr->num_channels, input[port_index]->bufs_num);

         // Write data into the dam buffers using dam buffers.
         result |= audio_dam_stream_write(me_ptr->in_port_info_arr[arr_idx].strm_writer_ptr,
                                          actual_input_chs,
                                          input[port_index]->buf_ptr,
                                          input[port_index]->flags.is_timestamp_valid,
                                          input[port_index]->timestamp);

#ifdef AUDIO_DAM_TS_DEBUG
         AR_MSG(DBG_HIGH_PRIO,
                "dam_write: buf ts: %lu is_valid: %lu actual_data_len:%lu ",
                input[port_index]->timestamp,
                input[port_index]->flags.is_timestamp_valid,
                input[port_index]->buf_ptr[0].actual_data_len);
#endif
      }
   }

   if (output)
   {
      for (uint32_t arr_idx = 0; arr_idx < me_ptr->max_output_ports; arr_idx++)
      {
         // Generate output only if,
         //      1. Steam reader is created [means port is Started and circular buffers are initialized]
         //      2. Gate is opened by the detection engine, after detection.
         if (!me_ptr->out_port_info_arr[arr_idx].is_gate_opened || !is_dam_output_port_initialized(me_ptr, arr_idx) ||
             !me_ptr->out_port_info_arr[arr_idx].is_started)
         {
            continue;
         }

         // Get port index from the arr index.
         uint32_t port_index = me_ptr->out_port_info_arr[arr_idx].port_index;

         // output buffers sanity check.
         if ((NULL == output[port_index]) || (NULL == output[port_index]->buf_ptr) ||
             (NULL == output[port_index]->buf_ptr[0].data_ptr))
         {
#ifdef DEBUG_AUDIO_DAM_BUFFER_MODULE
            DAM_MSG_ISLAND(me_ptr->miid,
                           DBG_MED_PRIO,
                           "warning: output buffers not available port_id=0x%x ",
                           me_ptr->out_port_info_arr[arr_idx].port_id);
#endif
            continue;
         }

         // IF the buffer has stale data.
         if ((output[port_index]->buf_ptr->actual_data_len == output[port_index]->buf_ptr->max_data_len) ||
             (0 == output[port_index]->buf_ptr->max_data_len))
         {
            continue;
         }

         if (me_ptr->out_port_info_arr[arr_idx].is_pending_gate_close)
         {
            // insert eos marker
            bool_t skip_voting_on_eos = me_ptr->out_port_info_arr[arr_idx].is_peer_aad ? TRUE : FALSE;
            if (CAPI_EOK == (result = capi_dam_insert_flushing_eos_at_out_port(me_ptr, output[port_index], skip_voting_on_eos)))
            {
               capi_check_and_close_the_gate(me_ptr, arr_idx, FALSE);
               me_ptr->out_port_info_arr[arr_idx].is_pending_gate_close = FALSE;
            }
            continue; // done with this arr index, don't have to update ts
         }

         // Usually num configured output chs is same as output media format, even if they are not same module handles
         // graciously in the following way,
         // 1. If num output chs is less than configured chs, module produces output num chs and ignore other chs.
         // 2. if num output chs is greater than configured, module produces configured chs and drop others.
         uint32_t num_output_chs =
            MIN(me_ptr->out_port_info_arr[arr_idx].actual_output_num_chs, output[port_index]->bufs_num);

         uint32_t output_frame_len_us = 0;
         bool_t   is_timestamp_valid  = FALSE;

         result = audio_dam_stream_read(me_ptr->out_port_info_arr[arr_idx].strm_reader_ptr,
                                        num_output_chs,
                                        output[port_index]->buf_ptr,
                                        &is_timestamp_valid,
                                        &output[port_index]->timestamp,
                                        &output_frame_len_us);

         if (AR_ENEEDMORE == result)
         {
            // continue reading data from other output ports
            continue;
         }
         else if (AR_EOK == result) // If read was successful update the current timestamp.
         {
            output[port_index]->flags.is_timestamp_valid = is_timestamp_valid;

            if (me_ptr->out_port_info_arr[arr_idx].ftrt_unread_data_len_in_us)
            {
               // decrement ftrt unread length based on ouput actual data length
               me_ptr->out_port_info_arr[arr_idx].ftrt_unread_data_len_in_us =
                  (me_ptr->out_port_info_arr[arr_idx].ftrt_unread_data_len_in_us < output_frame_len_us)
                     ? 0
                     : (me_ptr->out_port_info_arr[arr_idx].ftrt_unread_data_len_in_us - output_frame_len_us);

               if (!me_ptr->out_port_info_arr[arr_idx].ftrt_unread_data_len_in_us)
               {
                  if (me_ptr->out_port_info_arr[arr_idx].is_drain_history)
                  {
                     // mark it as pending and handle at next process call
                     me_ptr->out_port_info_arr[arr_idx].is_pending_gate_close = TRUE;
                     DAM_MSG_ISLAND(me_ptr->miid,
                                    DBG_HIGH_PRIO,
                                    "DAM: Port index %lu gate close. history drain is complete",
                                    arr_idx);
                  }
                  DAM_MSG_ISLAND(me_ptr->miid,
                                 DBG_HIGH_PRIO,
                                 "DAM_DRIVER: Stream reader ftrt data is drained, moving to RT mode out_port 0x%x ",
                                 me_ptr->out_port_info_arr[arr_idx].port_id);
               }
            }
            if (me_ptr->out_port_info_arr[arr_idx].strm_reader_ptr->is_batch_streaming)
            {
               capi_audio_dam_buffer_update_kpps_vote(me_ptr);
            }
#ifdef DEBUG_AUDIO_DAM_BUFFER_MODULE
            uint32_t temp_unread_bytes = 0;
            audio_dam_get_stream_reader_unread_bytes(me_ptr->out_port_info_arr[arr_idx].strm_reader_ptr,
                                                     &temp_unread_bytes);

            DAM_MSG_ISLAND(me_ptr->miid,
                           DBG_LOW_PRIO,
                           "dam_read: Port_id: %lu buf_ts: %lu, is_valid: %ld, actual_data_len: %lu, "
                           "un_read_bytes: %lu",
                           me_ptr->out_port_info_arr[arr_idx].port_id,
                           output[port_index]->timestamp,
                           output[port_index]->flags.is_timestamp_valid,
                           output[port_index]->buf_ptr[0].actual_data_len,
                           temp_unread_bytes);
#endif
         }
         else
         {
            return CAPI_EFAILED;
         }
      }
   }
   return CAPI_EOK;
}

capi_err_t capi_audio_dam_change_trigger_policy(capi_audio_dam_t *me_ptr)
{
   capi_err_t result = CAPI_EOK;

   // evaluate if tp needs to enabled/disabled.
   bool_t need_to_enable_tp = TRUE;

   // Note: We cannot use optimization to enable and disable TGP dynamically for Dam
   // when there are two Dams in same container this optimization will cause issues.
   //
   // eg: In Voice Activation usecase its causing KW read latency issue. If Dam switches to default TGP
   //     when gate is closed. Container pops and holds ext out buffer. When gate opens, dam
   //     updates TGP in cmd context to "optional" output and waits for the next interrupt to start
   //     draining KW data, causing almost ~40ms of latency.
   //
   //      Ideally if we are able to return the
   //     unprocessed buffer to the dataQ we should be able to trigger container before interrupt
   //     is fired, but with 2 Dams in Voice Activation+Acoustic Activity Detection, returning the buffer to dataQ is causing infinite loop.
   //     check gen_cntr_peer_output_island file for more details.
#if 0
   bool_t need_to_enable_tp = FALSE;
   if (me_ptr->cannot_toggle_to_default_tgp)
   {
      // tgp must be always enabled in this case,
      need_to_enable_tp = TRUE;
   }
   else // can toggle TGP
   {
      // TP needs to be enabled if atleast one output's gate is opened.
      for (uint32_t op_idx = 0; op_idx < me_ptr->max_output_ports; op_idx++)
      {
         if (me_ptr->out_port_info_arr[op_idx].is_gate_opened)
         {
            need_to_enable_tp = TRUE;
         }
      }
   }
#endif

   DAM_MSG_ISLAND(me_ptr->miid,
                  DBG_HIGH_PRIO,
                  "capi_audio_dam: Changing trigger policy toggle_allowed: %lu need_to_enable:%lu, "
                  "currently_enabled:%lu",
                  me_ptr->cannot_toggle_to_default_tgp,
                  need_to_enable_tp,
                  me_ptr->is_tp_enabled);

   // return if already disabled
   if (!me_ptr->is_tp_enabled && !need_to_enable_tp)
   {
      return result;
   }

   if (need_to_enable_tp)
   {
      // raise signal trigger policy change, this change can be incremental i.e making a secondry
      // output port optional/blocked at gate open
      me_ptr->policy_chg_cb.change_signal_trigger_policy_cb_fn(me_ptr->policy_chg_cb.context_ptr,
                                                               me_ptr->signal_tp.non_trigger_group_ptr,
                                                               FWK_EXTN_PORT_TRIGGER_POLICY_OPTIONAL,
                                                               1,
                                                               me_ptr->signal_tp.trigger_groups_ptr);

      // raise data trigger policy change, this change can be incremental i.e making a secondry
      // output port optional/blocked at gate open
      me_ptr->policy_chg_cb.change_data_trigger_policy_cb_fn(me_ptr->policy_chg_cb.context_ptr,
                                                             me_ptr->data_tp.non_trigger_group_ptr,
                                                             FWK_EXTN_PORT_TRIGGER_POLICY_OPTIONAL,
                                                             1,
                                                             me_ptr->data_tp.trigger_groups_ptr);
   }
   else // Disable TP
   {
      // Disable trigger policy
      me_ptr->policy_chg_cb.change_data_trigger_policy_cb_fn(me_ptr->policy_chg_cb.context_ptr,
                                                             NULL,
                                                             FWK_EXTN_PORT_TRIGGER_POLICY_OPTIONAL,
                                                             0,
                                                             NULL);

      // Disable trigger policy
      me_ptr->policy_chg_cb.change_signal_trigger_policy_cb_fn(me_ptr->policy_chg_cb.context_ptr,
                                                               NULL,
                                                               FWK_EXTN_PORT_TRIGGER_POLICY_OPTIONAL,
                                                               0,
                                                               NULL);
   }

   me_ptr->is_tp_enabled = need_to_enable_tp;

   return result;
}

capi_err_t capi_check_and_close_the_gate(capi_audio_dam_t *me_ptr, uint32_t op_arr_index, bool_t is_destroy)
{
   // Close the gate
   if (TRUE == me_ptr->out_port_info_arr[op_arr_index].is_gate_opened)
   {
      // Reset the output port state.
      me_ptr->out_port_info_arr[op_arr_index].is_gate_opened = FALSE;
   }

   me_ptr->out_port_info_arr[op_arr_index].is_drain_history           = FALSE;
   me_ptr->out_port_info_arr[op_arr_index].is_pending_gate_close      = FALSE;
   me_ptr->out_port_info_arr[op_arr_index].ftrt_unread_data_len_in_us = 0;

   /** reset batching mode to False, if it was enabled at gate open */
   audio_dam_stream_reader_enable_batching_mode(me_ptr->out_port_info_arr[op_arr_index].strm_reader_ptr,
                                                FALSE /**enable batching mode*/,
                                                0);

   // When gate is closed or port is closed, move the out port to nontriggerable group
   // for both signal and data triggers.`
   if (me_ptr->out_port_info_arr[op_arr_index].is_started)
   {
      me_ptr->data_tp.non_trigger_group_ptr
         ->out_port_grp_policy_ptr[me_ptr->out_port_info_arr[op_arr_index].port_index] =
         FWK_EXTN_PORT_NON_TRIGGER_BLOCKED;

      me_ptr->signal_tp.non_trigger_group_ptr
         ->out_port_grp_policy_ptr[me_ptr->out_port_info_arr[op_arr_index].port_index] =
         FWK_EXTN_PORT_NON_TRIGGER_BLOCKED;

      me_ptr->data_tp.trigger_groups_ptr[0]
         .out_port_grp_affinity_ptr[me_ptr->out_port_info_arr[op_arr_index].port_index] =
         FWK_EXTN_PORT_TRIGGER_AFFINITY_NONE;

      me_ptr->signal_tp.trigger_groups_ptr[0]
         .out_port_grp_affinity_ptr[me_ptr->out_port_info_arr[op_arr_index].port_index] =
         FWK_EXTN_PORT_TRIGGER_AFFINITY_NONE;

      capi_audio_dam_change_trigger_policy(me_ptr);
   }

   // During gate open, the channels might have been reordered based on best channel info from detection engines.
   // When control port is closing the gate but Dam module is not destroyed. we just need to revert the channel order
   // as per the output port cfg calib.
   //
   // For Acoustic Activity Detection usecase, we do not support best channel feature, so we dont have to revert the channel order.
   if (FALSE == me_ptr->out_port_info_arr[op_arr_index].is_peer_aad)
   {
      capi_audio_dam_reorder_chs_at_gate_close(me_ptr, op_arr_index, is_destroy);

      // In AAD usecase, do not update KPPS in process context.
      // And its not required to bump up KPPS since there is no IPC exchange in AAD opens the gate.
      capi_audio_dam_buffer_update_kpps_vote(me_ptr);
   }

   DAM_MSG_ISLAND(me_ptr->miid,
                  DBG_HIGH_PRIO,
                  "DAM: Gate at Port id %lu closed",
                  me_ptr->out_port_info_arr[op_arr_index].port_id);
   return CAPI_EOK;
}

// function to update the kpps votes.
void capi_audio_dam_buffer_update_kpps_vote(capi_audio_dam_t *me_ptr)
{
   uint32_t agg_kpps_vote = CAPI_AUDIO_DAM_LOW_KPPS;
   for (int i = 0; i < me_ptr->max_output_ports; i++)
   {
      uint32_t kpps_vote = CAPI_AUDIO_DAM_LOW_KPPS;

      /** When gate is opened, bump up the clock by voting for higher KPPS
          This is because, FTRT data of about 1.4s needs to be drained in less than 10 ms.
         If we assume MPPS of 2 for draining, we would need to actually vote (1400*2/10) = 280 MPPS.
         Just voting 500 MPPS to be safe. Vote is removed once FTRT data is fully drained or at algorithmic_reset.*/
      if (me_ptr->out_port_info_arr[i].is_open && me_ptr->out_port_info_arr[i].is_started &&
          me_ptr->out_port_info_arr[i].is_gate_opened)
      {
         if (me_ptr->out_port_info_arr[i].strm_reader_ptr->is_batch_streaming)
         {
            // sometimes 40ms batching will be done at the DMA layer itself
            // therefore DAM will not have FTRT data.
            const uint32_t LOW_COMP_BATCH_PERIOD = 40000;
            if (me_ptr->out_port_info_arr[i].strm_reader_ptr->data_batching_us <=
                   LOW_COMP_BATCH_PERIOD ||                                             // sort of real time streaming
                0 == me_ptr->out_port_info_arr[i].strm_reader_ptr->pending_batch_bytes) // going into buffering mode
            {
               kpps_vote = CAPI_AUDIO_DAM_LOW_KPPS;
            }
            else
            { // in ftrt drain mode
               kpps_vote = CAPI_AUDIO_DAM_HIGH_KPPS;
            }
         }
         else if (me_ptr->out_port_info_arr[i].ftrt_unread_data_len_in_us)
         { // in ftrt drain mode
            kpps_vote = CAPI_AUDIO_DAM_HIGH_KPPS;
         }
      }

      agg_kpps_vote = MAX(agg_kpps_vote, kpps_vote);
   }

   if (me_ptr->kpps_vote != agg_kpps_vote)
   {
      DAM_MSG_ISLAND(me_ptr->miid, DBG_LOW_PRIO, "capi_audio_dam: updating kpps %lu", agg_kpps_vote);
      capi_cmn_update_kpps_event(&me_ptr->event_cb_info, agg_kpps_vote);
      me_ptr->kpps_vote = agg_kpps_vote;
   }
}

static capi_err_t capi_dam_insert_flushing_eos_at_out_port(capi_audio_dam_t   *me_ptr,
                                                           capi_stream_data_t *output,
                                                           bool_t              skip_voting_on_eos)
{
   capi_err_t             capi_result    = CAPI_EOK;
   capi_stream_data_v2_t *out_stream_ptr = (capi_stream_data_v2_t *)output;

   if (CAPI_STREAM_V2 != out_stream_ptr->flags.stream_data_version)
   {
      DAM_MSG_ISLAND(me_ptr->miid, DBG_ERROR_PRIO, "capi_audio_dam: stream version must be 1");
      return CAPI_EFAILED;
   }
   module_cmn_md_list_t **md_list_pptr = &out_stream_ptr->metadata_list_ptr;
   module_cmn_md_t       *new_md_ptr   = NULL;
   module_cmn_md_eos_t   *eos_md_ptr   = NULL;
   capi_heap_id_t         heap;
   heap.heap_id = (uint32_t)me_ptr->heap_id;

   capi_result = me_ptr->metadata_handler.metadata_create(me_ptr->metadata_handler.context_ptr,
                                                          md_list_pptr,
                                                          sizeof(module_cmn_md_eos_t),
                                                          heap,
                                                          FALSE /*is_out_band*/,
                                                          &new_md_ptr);

   new_md_ptr->metadata_id                     = MODULE_CMN_MD_ID_EOS;
   new_md_ptr->offset                          = 0;
   eos_md_ptr                                  = (module_cmn_md_eos_t *)&new_md_ptr->metadata_buf;
   eos_md_ptr->flags.is_flushing_eos           = TRUE;
   eos_md_ptr->flags.is_internal_eos           = TRUE;
   eos_md_ptr->flags.skip_voting_on_dfs_change = skip_voting_on_eos;
   eos_md_ptr->cntr_ref_ptr                    = NULL;
   new_md_ptr->tracking_ptr                    = NULL;

   DAM_MSG_ISLAND(me_ptr->miid,
                  DBG_HIGH_PRIO,
                  "DAM: Created and inserted flushing eos at output defer_voting:%lu",
                  skip_voting_on_eos);

   return capi_result;
}