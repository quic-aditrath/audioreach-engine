/**
 * \file fef_cntr_process_island.c
 * \brief
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#include "fef_cntr_i.h"
#include "irm_cntr_prof_util.h"
// enable static to get accurate savings.
// #define FEF_CNTR_STATIC

#define FEF_CNTR_STATIC static

// #define TIME_PROP_ENABLE

FEF_CNTR_STATIC ar_result_t fef_cntr_preprocess_setup_output(gen_cntr_t *             me_ptr,
                                                             gen_topo_module_t *      module_ptr,
                                                             gen_cntr_ext_out_port_t *ext_out_port_ptr);

FEF_CNTR_STATIC ar_result_t fef_cntr_preprocess_setup_input(gen_cntr_t *            me_ptr,
                                                            gen_topo_module_t *     module_ptr,
                                                            gen_cntr_ext_in_port_t *ext_in_port_ptr);

FEF_CNTR_STATIC ar_result_t fef_cntr_input_dataQ_trigger_peer_cntr(gen_cntr_t *            me_ptr,
                                                                   gen_cntr_ext_in_port_t *ext_in_port_ptr);

FEF_CNTR_STATIC ar_result_t fef_cntr_input_data_set_up_peer_cntr(gen_cntr_t *            me_ptr,
                                                                 gen_cntr_ext_in_port_t *ext_in_port_ptr);


void fef_cntr_init_sdata(gen_topo_t *topo_ptr)
{
   gen_topo_process_context_t *pc = &topo_ptr->proc_context;

   /** ------------- MODULE PROCESS ---------------------
    *  prepare input and output sdata and calls the module capi process.
    */
   /** only module is expected in this container */
   // gen_topo_module_t *module_ptr = (gen_topo_module_t *)topo_ptr->gu.sorted_module_list_ptr->module_ptr;

   for (gu_ext_in_port_list_t *ext_in_port_list_ptr = topo_ptr->gu.ext_in_port_list_ptr; (NULL != ext_in_port_list_ptr);
        LIST_ADVANCE(ext_in_port_list_ptr))
   {
      gu_ext_in_port_t *     ext_in_port_ptr = (gu_ext_in_port_t *)ext_in_port_list_ptr->ext_in_port_ptr;
      gen_topo_input_port_t *in_port_ptr     = (gen_topo_input_port_t *)ext_in_port_ptr->int_in_port_ptr;

      // gen_topo_input_port_t *in_port_ptr =
      //    (gen_topo_input_port_t *)module_ptr->gu.input_port_list_ptr->ip_port_ptr->ext_in_port_ptr->int_in_port_ptr;

      pc->in_port_sdata_pptr[in_port_ptr->gu.cmn.index] = &(in_port_ptr->common.sdata);
   }

   for (gu_ext_out_port_list_t *ext_out_port_list_ptr = topo_ptr->gu.ext_out_port_list_ptr;
        (NULL != ext_out_port_list_ptr);
        LIST_ADVANCE(ext_out_port_list_ptr))
   {
      gu_ext_out_port_t *     ext_out_port_ptr = (gu_ext_out_port_t *)ext_out_port_list_ptr->ext_out_port_ptr;
      gen_topo_output_port_t *out_port_ptr     = (gen_topo_output_port_t *)ext_out_port_ptr->int_out_port_ptr;

      // gen_topo_output_port_t *out_port_ptr =
      // (gen_topo_output_port_t *)module_ptr->gu.output_port_list_ptr->op_port_ptr->ext_out_port_ptr->int_out_port_ptr;
      pc->out_port_sdata_pptr[out_port_ptr->gu.cmn.index] = &(out_port_ptr->common.sdata);
   }
}

ar_result_t fef_cntr_enable_timer(gen_cntr_t *me_ptr)
{
   ar_result_t result = AR_EOK;
   INIT_EXCEPTION_HANDLING

   /* Get the bit mask for stm signal, this signal trigger data processing
   on the container on every DMA interrupt or every time the timer expires */
   uint32_t bit_mask = GEN_CNTR_TIMER_BIT_MASK;

   /* initialize the signal */
   TRY(result,
      cu_init_signal(&me_ptr->cu, bit_mask, fef_cntr_signal_trigger, &me_ptr->st_module.trigger_signal_ptr));

   /* Create a periodic timer */
   posal_timer_callback_info_t cb_info;
   cb_info.cb_func_ptr    = fef_cntr_timer_isr_cb;
   cb_info.cb_context_ptr = (void*)me_ptr;
   if (AR_DID_FAIL(result = posal_timer_create_v2(&me_ptr->periodic_timer,
                                                   POSAL_TIMER_PERIODIC,
                                                   POSAL_TIMER_USER,
                                                   POSAL_TIMER_NOTIFY_OBJ_TYPE_CB_FUNC,
                                                   &cb_info,
                                                   POSAL_HEAP_DEFAULT)))
   {
      AR_MSG(DBG_HIGH_PRIO, "FC: Periodic timer create failed, result = 0x%8x", result);
      THROW(result, AR_EFAILED);
   }

   me_ptr->topo.flags.is_signal_triggered = TRUE;
   me_ptr->topo.flags.is_signal_triggered_active = TRUE;
   me_ptr->topo.flags.cannot_be_pure_signal_triggered = FALSE;

   /** start the timer */
   int64_t periodic_duration = me_ptr->cu.cntr_frame_len.frame_len_us;
   if(!periodic_duration)
   {
      periodic_duration = 1000; /** default to 1ms */
   }

   AR_MSG(DBG_HIGH_PRIO, "FE: Periodic timer duration %luus", periodic_duration);

   posal_timer_periodic_start_with_offset(me_ptr->periodic_timer, periodic_duration, 0 /**start offset */);

   CATCH(result, GEN_CNTR_MSG_PREFIX, me_ptr->topo.gu.log_id)
   {
   }

   return result;
}

ar_result_t fef_cntr_disable_timer(gen_cntr_t *me_ptr)
{
   ar_result_t result = AR_EOK;

   // posal_timer_stop(&me_ptr->periodic_timer);
   posal_timer_destroy(&me_ptr->periodic_timer);

   me_ptr->topo.flags.is_signal_triggered = FALSE;
   me_ptr->topo.flags.is_signal_triggered_active = FALSE;

   if (me_ptr->st_module.trigger_signal_ptr)
   {
      cu_deinit_signal(&me_ptr->cu, &me_ptr->st_module.trigger_signal_ptr);
      me_ptr->st_module.raised_interrupt_counter    = 0;
      me_ptr->st_module.processed_interrupt_counter = 0;
   }

   return result;
}


/** call back received from the timer utility for every periodic timer interrupts. This ISR callback is expected to set
 * signal to wakeup the fef container to wakeup and process the data.*/
void fef_cntr_timer_isr_cb(void *handle)
{
   gen_cntr_t *me_ptr = (gen_cntr_t *)handle;
   if (!me_ptr)
   {
      return;
   }

#ifdef VERBOSE_LOGGING
   GEN_CNTR_MSG(me_ptr->topo.gu.log_id, DBG_LOW_PRIO, "Recevied periodic timer interrupt! ");
#endif

   //increment interrupt counters, will be used for signal miss/intr handling miss detection later
   me_ptr->st_module.raised_interrupt_counter++;

   /** Sends signal to wake up the front end container thread.*/
   posal_signal_send(me_ptr->st_module.trigger_signal_ptr);
   return;
}

FEF_CNTR_STATIC ar_result_t fef_cntr_preprocess_setup_input(gen_cntr_t *            me_ptr,
                                                            gen_topo_module_t *     module_ptr,
                                                            gen_cntr_ext_in_port_t *ext_in_port_ptr)
{
   ar_result_t            result      = AR_EOK;
   gen_topo_input_port_t *in_port_ptr = (gen_topo_input_port_t *)ext_in_port_ptr->gu.int_in_port_ptr;
   capi_stream_data_v2_t *sdata_ptr   = &in_port_ptr->common.sdata;
#ifdef VERBOSE_DEBUGGING
   GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                DBG_HIGH_PRIO,
                "Setup FEF cntr for Module 0x%lX input 0x%lx",
                module_ptr->gu.module_instance_id,
                in_port_ptr->gu.cmn.id);
#endif

#ifdef VERBOSE_DEBUGGING
   uint32_t num_polled_buffers = 0;
#endif

   //polling/loop can be avoided if US and DS operate at same threshold. each upstream buffer is expected
   //       to be of the same threshold as current input port's threshold. -> will decide at later point
   // note: polling is required to fill local input buffer if US is lets say 1ms and DS is 5ms.
   while ((!ext_in_port_ptr->cu.input_data_q_msg.payload_ptr) &&
          (posal_queue_poll(ext_in_port_ptr->gu.this_handle.q_ptr)))
   {
      /** todo: pops the input buffer from the queue and populates ports buffer */
      result = fef_cntr_input_dataQ_trigger_peer_cntr(me_ptr, ext_in_port_ptr);
      if (AR_DID_FAIL(result))
      {
         GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                      DBG_ERROR_PRIO,
                      "process failed for ext input port 0x%lx of Module 0x%lX ",
                      in_port_ptr->gu.cmn.id,
                      in_port_ptr->gu.cmn.module_ptr->module_instance_id);
      }

#ifdef VERBOSE_DEBUGGING
      num_polled_buffers++;

      GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                   DBG_LOW_PRIO,
                   "Ext input port 0x%lx of Module 0x%lX. num_polled_buffers %lu",
                   in_port_ptr->gu.cmn.id,
                   in_port_ptr->gu.cmn.module_ptr->module_instance_id,
                   num_polled_buffers);
#endif
   }

   // underrun or EOS case, if actual len < max_len/threshold -> module has to detected internally
   // if(!sdata_ptr->buf_ptr->actual_data_len)
   if (sdata_ptr->buf_ptr->actual_data_len < sdata_ptr->buf_ptr->max_data_len)
   {
      sdata_ptr->flags.erasure = TRUE;
      ext_in_port_ptr->err_msg_underrun_err_count++;
      if (gen_cntr_check_for_err_print(&me_ptr->topo))
      {
         GEN_CNTR_MSG_ISLAND(me_ptr->topo.gu.log_id,
                             DBG_ERROR_PRIO,
                             "Ext input port 0x%lx of Module 0x%lX, actual data len %ld max data len %ld "
                             "(both per ch). No of underrun after last print %u.",
                             in_port_ptr->gu.cmn.id,
                             in_port_ptr->gu.cmn.module_ptr->module_instance_id,
                             in_port_ptr->common.bufs_ptr[0].actual_data_len,
                             in_port_ptr->common.bufs_ptr[0].max_data_len,
                             ext_in_port_ptr->err_msg_underrun_err_count);
         ext_in_port_ptr->err_msg_underrun_err_count = 0;
      }
   }
   else
   {
      sdata_ptr->flags.erasure = FALSE;
   }

   return result;
}

FEF_CNTR_STATIC ar_result_t fef_cntr_send_data_to_downstream_peer_cntr(gen_cntr_t *             me_ptr,
                                                                       gen_cntr_ext_out_port_t *ext_out_port_ptr,
                                                                       spf_msg_header_t *       out_buf_header)
{
   ar_result_t result = AR_EOK;

#ifdef VERBOSE_DEBUGGING
   spf_msg_data_buffer_t *out_buf_ptr = (spf_msg_data_buffer_t *)&out_buf_header->payload_start;
   gen_topo_module_t *    module_ptr  = (gen_topo_module_t *)ext_out_port_ptr->gu.int_out_port_ptr->cmn.module_ptr;
   GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                DBG_LOW_PRIO,
                "gen_cntr_send_pcm_to_downstream_peer_cntr: (0x%lx, 0x%lx) timestamp: %lu (0x%lx%lx). size %lu",
                module_ptr->gu.module_instance_id,
                ext_out_port_ptr->gu.int_out_port_ptr->cmn.id,
                (uint32_t)out_buf_ptr->timestamp,
                (uint32_t)(out_buf_ptr->timestamp >> 32),
                (uint32_t)out_buf_ptr->timestamp,
                (uint32_t)(out_buf_ptr->actual_size));
#endif

   // Reinterpret node's buffer as a header.
   posal_bufmgr_node_t out_buf_node = ext_out_port_ptr->cu.out_bufmgr_node;

   // Reinterpret the node itself as a spf_msg_t.
   spf_msg_t *msg_ptr   = (spf_msg_t *)&out_buf_node;
   // todo: avoid mssg opcode
   msg_ptr->msg_opcode  = SPF_MSG_DATA_BUFFER;
   msg_ptr->payload_ptr = out_buf_node.buf_ptr;

   result = posal_queue_push_back(ext_out_port_ptr->gu.downstream_handle.spf_handle_ptr->q_ptr,
                                  (posal_queue_element_t *)msg_ptr);
   if (AR_DID_FAIL(result))
   {
      GEN_CNTR_MSG(me_ptr->topo.gu.log_id, DBG_ERROR_PRIO, "Failed to deliver buffer dowstream. Dropping");
      gen_cntr_return_back_out_buf(me_ptr, ext_out_port_ptr);
      goto __bailout;
   }
   else
   {
#ifdef VERBOSE_DEBUGGING
      GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                   DBG_LOW_PRIO,
                   "pushed buffer downstream 0x%p. Current bit mask 0x%x",
                   ext_out_port_ptr->cu.out_bufmgr_node.buf_ptr,
                   me_ptr->cu.curr_chan_mask);
#endif
   }

__bailout:
   ext_out_port_ptr->cu.out_bufmgr_node.buf_ptr = NULL;
   return result;
}


/**
 * loop over output ports to do sanity checks, adjust lengths and populate few variables.
 */

/**  For each external out port, postprocess data, send media fmt and data down. */
FEF_CNTR_STATIC ar_result_t fef_cntr_post_process_peer_ext_output(gen_cntr_t *             me_ptr,
                                                                  gen_topo_output_port_t * out_port_ptr,
                                                                  gen_cntr_ext_out_port_t *ext_out_port_ptr)
{
   ar_result_t            result    = AR_EOK;
   capi_stream_data_v2_t *sdata_ptr = &out_port_ptr->common.sdata;

#ifdef VERBOSE_DEBUGGING
   GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                DBG_LOW_PRIO,
                "postprocess output: before module (%lu of %lu), ext out (%lu of %lu), "
                "bytes_produced_by_pp %lu",
                out_port_ptr->common.bufs_ptr[0].actual_data_len,
                out_port_ptr->common.bufs_ptr[0].max_data_len,
                ext_out_port_ptr->buf.actual_data_len,
                ext_out_port_ptr->buf.max_data_len,
                sdata_ptr->buf_ptr[0].actual_data_len);
#endif

   // topo_port_state_t ds_downgraded_state =
   //    cu_get_external_output_ds_downgraded_port_state(&me_ptr->cu, &ext_out_port_ptr->gu);

   // // todo: check and remove if possible
   // if (TOPO_PORT_STATE_STARTED != ds_downgraded_state)
   // {
   //    GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
   //                 DBG_ERROR_PRIO,
   //                 " Dropping data %lu and/or metadata 0x%p as there's no external buf",
   //                 sdata_ptr->buf_ptr->actual_data_len,
   //                 sdata_ptr->metadata_list_ptr);

   //    gen_cntr_return_back_out_buf(me_ptr, ext_out_port_ptr);
   //    return result;
   // }

   /* Check if there is enough space to the ext out buffer only if,
         1. If the external buffer exist.
         2. And if the ext buffer is not being reused as internal buffer. If its being reused ext buffer
           already has the processed data so we don't have to copy. */
   if (!out_port_ptr->common.bufs_ptr[0].data_ptr)
   {
      gen_cntr_handle_st_overrun_at_post_process(me_ptr, ext_out_port_ptr);
      return AR_EOK;
   }

   spf_msg_t media_fmt_msg = { 0 };

   /** if partial data is present along with the media fmt event*/
   if (out_port_ptr->common.flags.media_fmt_event)
   {
      /**
       * copy MF to ext-out even if bytes_produced_by_topo is zero.
       * in gapless use cases, if EOS comes before initial silence is removed, then we send EOS before any data is sent.
       * before sending EOS, we need to send MF (true for any MD)
       * Don't clear the out_port_ptr->common.flags.media_fmt_event if old data is left.
       */
      gen_cntr_get_output_port_media_format_from_topo(ext_out_port_ptr, TRUE /* updated to unchanged */);

      result = cu_create_media_fmt_msg_for_downstream(&me_ptr->cu,
                                                      &ext_out_port_ptr->gu,
                                                      &media_fmt_msg,
                                                      SPF_MSG_DATA_MEDIA_FORMAT);

      if (result != AR_EOK)
      {
         return result;
      }

#ifdef VERBOSE_DEBUGGING
      GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                   DBG_MED_PRIO,
                   "Sent container output media format from module 0x%x, port 0x%x, flags: media fmt %d frame len %d",
                   ext_out_port_ptr->gu.int_out_port_ptr->cmn.module_ptr->module_instance_id,
                   ext_out_port_ptr->gu.int_out_port_ptr->cmn.id,
                   out_port_ptr->common.flags.media_fmt_event,
                   ext_out_port_ptr->cu.flags.upstream_frame_len_changed);
#endif

      /** sent media fmt downstream */
      result = cu_send_media_fmt_update_to_downstream(&me_ptr->cu,
                                                      &ext_out_port_ptr->gu,
                                                      &media_fmt_msg,
                                                      ext_out_port_ptr->gu.downstream_handle.spf_handle_ptr->q_ptr);

      if (AR_EOK == result)
      {
         /** reset the media fmt flags*/
         ext_out_port_ptr->flags.out_media_fmt_changed         = FALSE;
         ext_out_port_ptr->cu.flags.upstream_frame_len_changed = FALSE;
      }
   }

   if (sdata_ptr->buf_ptr->actual_data_len || sdata_ptr->metadata_list_ptr)
   {
      /** send buffers downstream*/
      gen_topo_output_port_t *out_port_ptr = (gen_topo_output_port_t *)ext_out_port_ptr->gu.int_out_port_ptr;
      spf_msg_header_t *      header       = (spf_msg_header_t *)(ext_out_port_ptr->cu.out_bufmgr_node.buf_ptr);
      spf_msg_data_buffer_t * out_buf_ptr  = (spf_msg_data_buffer_t *)&header->payload_start;
      capi_stream_data_v2_t * sdata_ptr    = &(out_port_ptr->common.sdata);

      topo_buf_t *buf_ptr = out_port_ptr->common.bufs_ptr;

      // preparing out buf msg header, similar to gen_cntr_populate_peer_cntr_out_buf()
      out_buf_ptr->actual_size = buf_ptr->actual_data_len * sdata_ptr->bufs_num;
      out_buf_ptr->flags       = 0;

      if (sdata_ptr->metadata_list_ptr)
      {
         // When md lib is in nlpi, we dont need to exit here since by this point we should already be in nlpi
         // due to voting against island when propagating md
         gen_topo_check_realloc_md_list_in_peer_heap_id(me_ptr->topo.gu.log_id,
                                                      &(ext_out_port_ptr->gu),
                                                      &(sdata_ptr->metadata_list_ptr));

         bool_t out_buf_has_flushing_eos = FALSE;
         gen_topo_populate_metadata_for_peer_cntr(&(me_ptr->topo),
                                                &(ext_out_port_ptr->gu),
                                                &(sdata_ptr->metadata_list_ptr),
                                                &out_buf_ptr->metadata_list_ptr,
                                                &out_buf_has_flushing_eos);
      }

#ifdef TIME_PROP_ENABLE
      /** Buffer will send down as soon as ready and reused for internal output
         hence directly copy TS from internal port sdata to external buffer */
      if (sdata_ptr->flags.is_timestamp_valid)
      {
         cu_set_bits(&out_buf_ptr->flags,
                     sdata_ptr->flags.is_timestamp_valid,
                     DATA_BUFFER_FLAG_TIMESTAMP_VALID_MASK,
                     DATA_BUFFER_FLAG_TIMESTAMP_VALID_SHIFT);

         out_buf_ptr->timestamp = sdata_ptr->timestamp;
      }

#ifdef VERBOSE_DEBUGGING
      GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                  DBG_LOW_PRIO,
                  "Outgoing timestamp: %lu (0x%lx%lx), flag=0x%lx, size=%lu TS_flag = %d",
                  (uint32_t)out_buf_ptr->timestamp,
                  (uint32_t)(out_buf_ptr->timestamp >> 32),
                  (uint32_t)out_buf_ptr->timestamp,
                  out_buf_ptr->flags,
                  out_buf_ptr->actual_size,
                  sdata_ptr->flags.is_timestamp_valid);
#endif
#endif

      // check and send prebuffer
      gen_cntr_check_and_send_prebuffers(me_ptr, ext_out_port_ptr, out_buf_ptr);
      result = fef_cntr_send_data_to_downstream_peer_cntr(me_ptr, ext_out_port_ptr, header);

      // it's enough to reset first buf, as only this is checked against.
      buf_ptr->data_ptr     = NULL;
      buf_ptr->max_data_len = 0;
      gen_topo_set_all_bufs_len_to_zero(&out_port_ptr->common);
   }
   else
   {
      /** return the buffer back, if data or metadata is not present*/
      gen_cntr_return_back_out_buf(me_ptr, ext_out_port_ptr);
   }

   return result;
}

FEF_CNTR_STATIC void fef_cntr_process_attached_module_to_input(gen_topo_t *           topo_ptr,
                                                               gen_topo_module_t *    module_ptr,
                                                               gen_topo_input_port_t *in_port_ptr)
{
#ifdef VERBOSE_DEBUGGING
   TOPO_MSG(topo_ptr->gu.log_id, DBG_MED_PRIO, "Attached Module at the input to fef cntr");
#endif
}

FEF_CNTR_STATIC void fef_cntr_process_attached_module_to_output(gen_topo_t *            topo_ptr,
                                                                gen_topo_module_t *     module_ptr,
                                                                gen_topo_output_port_t *out_port_ptr)
{
   capi_err_t             attached_proc_result    = CAPI_EOK;
   gen_topo_module_t *    out_attached_module_ptr = (gen_topo_module_t *)out_port_ptr->gu.attached_module_ptr;
   capi_stream_data_v2_t *sdata_ptr               = &out_port_ptr->common.sdata;

   if (out_attached_module_ptr->flags.disabled || !sdata_ptr->buf_ptr[0].data_ptr)
   {
#ifdef VERBOSE_DEBUGGING
      TOPO_MSG(topo_ptr->gu.log_id,
               DBG_MED_PRIO,
               "Skipping process on attached elementary module miid 0x%lx for input port idx %ld id 0x%lx - "
               "disabled %ld, is_mf_valid %ld, no buffer %ld, no data provided %ld, md list ptr NULL %ld",
               out_attached_module_ptr->gu.module_instance_id,
               out_port_ptr->gu.cmn.index,
               module_ptr->gu.module_instance_id,
               out_attached_module_ptr->flags.disabled,
               out_port_ptr->common.flags.is_mf_valid,
               (NULL == sdata_ptr->buf_ptr),
               (NULL == sdata_ptr->buf_ptr) ? 0 : (0 == sdata_ptr->buf_ptr[0].actual_data_len),
               (NULL == sdata_ptr->metadata_list_ptr));
#endif
      return;
   }

#ifdef VERBOSE_DEBUGGING
   TOPO_MSG(topo_ptr->gu.log_id,
            DBG_MED_PRIO,
            "Before process attached elementary module miid 0x%lx for output port idx %ld id 0x%lx",
            out_attached_module_ptr->gu.module_instance_id,
            out_port_ptr->gu.cmn.index,
            module_ptr->gu.module_instance_id);
#endif

   /** todo: enable optimized profiling, pull changes from thin topo */
   // PROF_BEFORE_PROCESS(out_attached_module_ptr->prof_info_ptr)
   uint32_t out_port_idx = out_port_ptr->gu.cmn.index;
   attached_proc_result =
      out_attached_module_ptr->capi_ptr->vtbl_ptr
         ->process(out_attached_module_ptr->capi_ptr,
                   (capi_stream_data_t **)&(topo_ptr->proc_context.out_port_sdata_pptr[out_port_idx]),
                   (capi_stream_data_t **)&(topo_ptr->proc_context.out_port_sdata_pptr[out_port_idx]));
   // PROF_AFTER_PROCESS(out_attached_module_ptr->prof_info_ptr, topo_ptr->gu.prof_mutex)

#ifdef VERBOSE_DEBUGGING
   TOPO_MSG(topo_ptr->gu.log_id,
            DBG_MED_PRIO,
            "After process attached elementary module miid 0x%lx for output port idx %ld id 0x%lx",
            out_attached_module_ptr->gu.module_instance_id,
            out_port_ptr->gu.cmn.index,
            module_ptr->gu.module_instance_id);
#endif

#ifdef VERBOSE_DEBUGGING
   // Don't ignore need more for attached modules.
   if (CAPI_FAILED(attached_proc_result))
   {
      TOPO_MSG(topo_ptr->gu.log_id,
               DBG_ERROR_PRIO,
               "Attached elementary module miid 0x%lx for output port idx %ld id 0x%lx returned error 0x%lx "
               "during process",
               out_attached_module_ptr->gu.module_instance_id,
               out_port_ptr->gu.cmn.index,
               module_ptr->gu.module_instance_id,
               attached_proc_result);
   }
#endif
}

/**
 * after input and output buffers are set-up, topo process is called once on signal triggers.
 * 1. If sufficient input is not present, ext in will underrun at this point.
 * 2. If ext output is ready, it ready_to_deliver_output flag will be set and buffer will be released by the caller.
 * 3. If output is ready, delivers the buffers to downstream containers.
 */
FEF_CNTR_STATIC ar_result_t fef_cntr_data_process_one_frame(gen_cntr_t *me_ptr)
{
   ar_result_t                 result        = AR_EOK;
   capi_err_t                  proc_result   = CAPI_EOK;
   gen_topo_t *                topo_ptr      = &me_ptr->topo;
   gen_topo_process_context_t *pc            = &topo_ptr->proc_context;
   gen_topo_process_info_t *   proc_info_ptr = &topo_ptr->proc_context.process_info;
   INIT_EXCEPTION_HANDLING

   /** ------------- MODULE PROCESS ---------------------
    *  prepare input and output sdata and calls the module capi process.
    */
   /** only module is expected in this container */
   gen_topo_module_t *module_ptr = (gen_topo_module_t *)me_ptr->topo.gu.sorted_module_list_ptr->module_ptr;

   /** -------- PRE-PROCESS EXTERNAL OUTPUTS --------------- */
   /* Iterate through modules inputs and copy data from prev output if required. */
   for (gu_ext_in_port_list_t *ext_in_port_list_ptr = topo_ptr->gu.ext_in_port_list_ptr; (NULL != ext_in_port_list_ptr);
        LIST_ADVANCE(ext_in_port_list_ptr))
   {
      gen_cntr_ext_in_port_t *ext_in_port_ptr = (gen_cntr_ext_in_port_t *)ext_in_port_list_ptr->ext_in_port_ptr;
      gen_topo_input_port_t * in_port_ptr     = (gen_topo_input_port_t *)ext_in_port_ptr->gu.int_in_port_ptr;

      /** pulls ext input buffer and populates capi input sdata.
       *    1. Underrun -> setting erasure flag
       *    2. what if ext input buffer is not present ?
       */
      TRY(result, fef_cntr_preprocess_setup_input(me_ptr, module_ptr, ext_in_port_ptr));

      if (in_port_ptr->attached_module_ptr)
      {
         fef_cntr_process_attached_module_to_input(topo_ptr, module_ptr, in_port_ptr);
      }


#ifdef VERBOSE_DEBUGGING
      PRINT_PORT_INFO_AT_PROCESS(module_ptr->gu.module_instance_id,
                                 in_port_ptr->gu.cmn.id,
                                 in_port_ptr->common,
                                 proc_result,
                                 "input",
                                 "before");
#endif
   }

   /** -------- PRE-PROCESS EXTERNAL OUTPUTS --------------- */
   for (gu_ext_out_port_list_t *ext_out_port_list_ptr = topo_ptr->gu.ext_out_port_list_ptr;
        (NULL != ext_out_port_list_ptr);
        LIST_ADVANCE(ext_out_port_list_ptr))
   {
      gen_cntr_ext_out_port_t *ext_out_port_ptr = (gen_cntr_ext_out_port_t *)ext_out_port_list_ptr->ext_out_port_ptr;

      /** setup ext output buffer and setup the internal capi output sdata with the ext buffer pointers.
       *    1. If buffer is not present
       *       option 1: pass NULL buf ptr - no need to alloc topo, module can internally overrun.
       *       option 2: alloc a topo buffer and assign it, post process drop the buffer.
       */
      result = fef_cntr_preprocess_setup_output(me_ptr, module_ptr, ext_out_port_ptr);

      /** todo: handle post process, doesnt seem to be really required. check further. */
#ifdef VERBOSE_DEBUGGING
      gen_topo_output_port_t *out_port_ptr = (gen_topo_output_port_t *)ext_out_port_ptr->gu.int_out_port_ptr;
      PRINT_PORT_INFO_AT_PROCESS(module_ptr->gu.module_instance_id,
                                 out_port_ptr->gu.cmn.id,
                                 out_port_ptr->common,
                                 proc_result,
                                 "output",
                                 "before");
#endif
   }

   /** todo: enable optimized profiling, pull changes from thin topo */
   PROF_BEFORE_PROCESS(module_ptr->prof_info_ptr)

   // todo: remove this flag
   proc_info_ptr->is_in_mod_proc_context = TRUE;

   proc_result                           = module_ptr->capi_ptr->vtbl_ptr->process(module_ptr->capi_ptr,
                                                         (capi_stream_data_t **)pc->in_port_sdata_pptr,
                                                         (capi_stream_data_t **)pc->out_port_sdata_pptr);
   proc_info_ptr->is_in_mod_proc_context = FALSE;

   PROF_AFTER_PROCESS(module_ptr->prof_info_ptr, topo_ptr->gu.prof_mutex)

#ifdef VERBOSE_DEBUGGING
   GEN_CNTR_MSG(topo_ptr->gu.log_id,
                DBG_HIGH_PRIO,
                "capi process done for Module 0x%lX result 0x%lx",
                module_ptr->gu.module_instance_id,
                proc_result);
#endif

   // here MF must propagate from next module, starting from current module overwrites any data that module
   // might have outputed in this call.
   gen_topo_capi_event_flag_t *event_flag_ptr = (gen_topo_capi_event_flag_t *)&((topo_ptr)->capi_event_flag_);

   if (event_flag_ptr->media_fmt_event || event_flag_ptr->port_thresh)
   {
      bool_t                  mf_th_ps_event = FALSE;
      gen_topo_process_info_t temp           = { 0 };

      gen_cntr_handle_process_events_and_flags(me_ptr, &temp, &mf_th_ps_event, NULL);
   }

   /**------------------- POST PROCESS EXTERNAL OUTPUT PORTS --------------------------- */
   for (gu_ext_out_port_list_t *ext_out_port_list_ptr = topo_ptr->gu.ext_out_port_list_ptr;
        (NULL != ext_out_port_list_ptr);
        LIST_ADVANCE(ext_out_port_list_ptr))
   {
      gen_cntr_ext_out_port_t *ext_out_port_ptr = (gen_cntr_ext_out_port_t *)ext_out_port_list_ptr->ext_out_port_ptr;
      gen_topo_output_port_t * out_port_ptr     = (gen_topo_output_port_t *)ext_out_port_ptr->gu.int_out_port_ptr;

      /** todo: handle post process, doesnt seem to be really required. check further. */
#ifdef VERBOSE_DEBUGGING
      PRINT_PORT_INFO_AT_PROCESS(module_ptr->gu.module_instance_id,
                                 out_port_ptr->gu.cmn.id,
                                 out_port_ptr->common,
                                 proc_result,
                                 "output",
                                 "after");
#endif

      /** todo: log data if there is an attached data logging module.
       *
       * 1. can implment a simplified verision of data logging for fef container ?
       */

      if (out_port_ptr->gu.attached_module_ptr)
      {
         fef_cntr_process_attached_module_to_output(topo_ptr, module_ptr, out_port_ptr);
      }

      /** prepare ext output buffer and send it to downstream. and the GEN_TOPO_BUF_ORIGIN_EXT_BUF will be cleared
       *
       * move ptr from internal port to ext port, move Metadata as well. Usually output buffer must be released every
       * interrupt, because module will produce eith data or metadata on the started outuput ports.
       *
       * if module doesnt generate may be just retain the output buffer for the next process and dont send it
       * downstream and return the buffers.
       */
      result |= fef_cntr_post_process_peer_ext_output(me_ptr,
                                                      out_port_ptr,
                                                      (gen_cntr_ext_out_port_t *)out_port_ptr->gu.ext_out_port_ptr);
   }

   /**------------------- POST PROCESS EXTERNAL INPUT PORTS --------------------------- */
   for (gu_ext_in_port_list_t *ext_in_port_list_ptr = topo_ptr->gu.ext_in_port_list_ptr; (NULL != ext_in_port_list_ptr);
        LIST_ADVANCE(ext_in_port_list_ptr))
   {
      gen_cntr_ext_in_port_t *ext_in_port_ptr = (gen_cntr_ext_in_port_t *)ext_in_port_list_ptr->ext_in_port_ptr;
      gen_topo_input_port_t * in_port_ptr     = (gen_topo_input_port_t *)ext_in_port_ptr->gu.int_in_port_ptr;

#ifdef VERBOSE_DEBUGGING
      PRINT_PORT_INFO_AT_PROCESS(module_ptr->gu.module_instance_id,
                                 in_port_ptr->gu.cmn.id,
                                 in_port_ptr->common,
                                 proc_result,
                                 "input",
                                 "after");
#endif

      /** todo: If external buffer is held for process return buffer to peer and clear the buffer pointer*/
      /** todo: free actually returns buffer to Upstream container.*/
      // gen_cntr_free_input_data_cmd(me_ptr, ext_in_port_ptr, AR_EOK, FALSE);

      if (!ext_in_port_ptr->cu.input_data_q_msg.payload_ptr)
      {
         continue;
      }
      spf_msg_ack_msg(&ext_in_port_ptr->cu.input_data_q_msg, AR_EOK);
      ext_in_port_ptr->cu.input_data_q_msg.payload_ptr = NULL;

      in_port_ptr->common.bufs_ptr[0].data_ptr     = NULL;
      in_port_ptr->common.bufs_ptr[0].max_data_len = 0;
      gen_topo_set_all_bufs_len_to_zero(&in_port_ptr->common);
   }
   CATCH(result, GEN_CNTR_MSG_PREFIX, me_ptr->topo.gu.log_id)
   {
   }
   return result;
}

/* thn container's callback for the interrupt signal.*/
ar_result_t fef_cntr_signal_trigger(cu_base_t *cu_ptr, uint32_t channel_bit_index)
{
   ar_result_t result = AR_EOK;
   gen_cntr_t *me_ptr = (gen_cntr_t *)cu_ptr;

#ifdef VERBOSE_DEBUGGING
   GEN_CNTR_MSG(me_ptr->topo.gu.log_id, DBG_LOW_PRIO, "fef_cntr_signal_trigger: Received signal trigger");
#endif

   // Increment in process context
   me_ptr->st_module.processed_interrupt_counter++;
   me_ptr->st_module.steady_state_interrupt_counter++;

   /* Check for signal miss, if raised_interrupt_counter is greater than process counter,
    * one or more interrupts have not been serviced.
    * This check will be skipped for timer modules eg:spr, rat since raised_interrupt_counter is always 0
    * This checks kind of signal miss because of the container thread being busy and one or more interrupts have not
    * been serviced. */
   if (me_ptr->st_module.raised_interrupt_counter > me_ptr->st_module.processed_interrupt_counter)
   {
      // todo: check if anything needs to be done on signal miss ?
#ifdef VERBOSE_DEBUGGING
      GEN_CNTR_MSG(me_ptr->topo.gu.log_id, DBG_ERROR_PRIO, "fef_cntr_signal_trigger: Signal Miss");
#endif
   }

   /*clear the trigger signal */
   posal_signal_clear_inline(me_ptr->st_module.trigger_signal_ptr);

   /** Process one frame per signal trigger. */
   result = fef_cntr_data_process_one_frame(me_ptr);

   /** Poll control channel and check for incoming ctrl msgs.
    * If any present, do set param and return the msgs. */
   result = cu_poll_and_process_ctrl_msgs(&me_ptr->cu);

   /**
    * TODO: implement post process lite weight signal miss detection. if data process takes more than interrupt duration
    *
    * For signal miss detection,
    *    1. increment "me_ptr->st_module.signal_miss_counter" if there is a signal miss
    *    2. increment me_ptr->st_module.processed_interrupt_counter++
    *    3. me_ptr->topo.flags.need_to_ignore_signal_miss = FALSE;
    *    4. me_ptr->prev_err_print_time_ms = me_ptr->topo.proc_context.err_print_time_in_this_process_ms;
    *
    *    1. option 1: may be put this under debug no need to enable signal miss detection always.
    *    2. option 2: just print error and do nothing
    *
    */

   return result;
}

///////////////////////   FEF cntr external output utilities ///////////////////////////////

/**
 * sets up output buffers and calls process on topo.
 *
 * any of the output port can trigger this.
 */
FEF_CNTR_STATIC ar_result_t fef_cntr_preprocess_setup_output(gen_cntr_t *             me_ptr,
                                                             gen_topo_module_t *      module_ptr,
                                                             gen_cntr_ext_out_port_t *ext_out_port_ptr)
{
   ar_result_t             result       = AR_EOK;
   gen_topo_output_port_t *out_port_ptr = (gen_topo_output_port_t *)ext_out_port_ptr->gu.int_out_port_ptr;
   capi_stream_data_v2_t * sdata_ptr    = &out_port_ptr->common.sdata;

   /*this could have been populatd if MF is unknown, module wont generate data in earlier process call*/
   if (ext_out_port_ptr->cu.out_bufmgr_node.buf_ptr)
   {
      return AR_EOK;
   }

   topo_port_state_t ds_downgraded_state =
      cu_get_external_output_ds_downgraded_port_state(&me_ptr->cu, &ext_out_port_ptr->gu);

   // todo: check and remove if possible
   if (TOPO_PORT_STATE_STARTED != ds_downgraded_state)
   {
      GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                   DBG_ERROR_PRIO,
                   " Dropping data %lu and/or metadata 0x%p as there's no external buf",
                   sdata_ptr->buf_ptr->actual_data_len,
                   sdata_ptr->metadata_list_ptr);

      return result;
   }

   /* pops a buffer, polls & recreate buffers if required */
   result = gen_cntr_process_pop_out_buf(me_ptr, ext_out_port_ptr);
   if (AR_FAILED(result))
   {
      /** if ext output buf is not available print overrun and return */
      if (NULL == ext_out_port_ptr->cu.out_bufmgr_node.buf_ptr)
      {
         gen_cntr_st_check_print_overrun(me_ptr, ext_out_port_ptr);
      }

      // error can be due to buffer reallocation failure, return buffer to make sure
      // process is not called with incorrect sized buffers.
      gen_cntr_return_back_out_buf(me_ptr, ext_out_port_ptr);
      return result;
   }

   spf_msg_header_t *     header      = (spf_msg_header_t *)(ext_out_port_ptr->cu.out_bufmgr_node.buf_ptr);
   spf_msg_data_buffer_t *out_buf_ptr = (spf_msg_data_buffer_t *)&header->payload_start;

   uint32_t max_len_per_buf = out_port_ptr->common.max_buf_len_per_buf;

   /** assign for the internal output port which is nothing but the end of an NBLC and propagate the same buffer
    * backwards.*/
   int8_t* data_ptr = (int8_t *)(&(out_buf_ptr->data_buf));
   for (uint32_t b = 0; b < sdata_ptr->bufs_num; b++)
   {
      out_port_ptr->common.bufs_ptr[b].data_ptr        = data_ptr;
      out_port_ptr->common.bufs_ptr[b].actual_data_len = 0;
      out_port_ptr->common.bufs_ptr[b].max_data_len    = max_len_per_buf;
      data_ptr += max_len_per_buf;
   }

   // handle timestamp
   sdata_ptr->flags.is_timestamp_valid = FALSE;

#ifdef VERBOSE_DEBUGGING
   GEN_CNTR_MSG(&me_ptr->topo.gu.log_id,
                DBG_HIGH_PRIO,
                "Module 0x%lX external output 0x%lx int buf: (%p %lu %lu) ",
                module_ptr->gu.module_instance_id,
                out_port_ptr->gu.cmn.id,
                out_port_ptr->common.bufs_ptr[0].data_ptr,
                out_port_ptr->common.bufs_ptr[0].actual_data_len,
                out_port_ptr->common.bufs_ptr[0].max_data_len);
#endif

   return result;
}



/** setup input data bufs (pre-process), handles TS and Data*/
FEF_CNTR_STATIC ar_result_t fef_cntr_input_data_set_up_peer_cntr(gen_cntr_t *            me_ptr,
                                                                 gen_cntr_ext_in_port_t *ext_in_port_ptr)
{
   ar_result_t            result        = AR_EOK;
   spf_msg_header_t *     header        = (spf_msg_header_t *)(ext_in_port_ptr->cu.input_data_q_msg.payload_ptr);
   spf_msg_data_buffer_t *input_buf_ptr = (spf_msg_data_buffer_t *)&header->payload_start;

   gen_topo_input_port_t *in_port_ptr = (gen_topo_input_port_t *)ext_in_port_ptr->gu.int_in_port_ptr;
   capi_stream_data_v2_t *sdata_ptr   = &in_port_ptr->common.sdata;

#ifdef VERBOSE_DEBUGGING
   GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                DBG_HIGH_PRIO,
                "Setup ext input port 0x%lx of Module 0x%lX ",
                in_port_ptr->gu.cmn.id,
                in_port_ptr->gu.cmn.module_ptr->module_instance_id);
   uint32_t bytes_in_int_inp_md_prop = gen_topo_get_actual_len_for_md_prop(&in_port_ptr->common);
#endif

   /** Hold the external buffer for nblc modules processing. expected actual length is already checked hence assuming
    * per buf actual length to be 'max_buf_len_per_buf'*/
   uint32_t    actual_len_per_buf = input_buf_ptr->actual_size / sdata_ptr->bufs_num;
   topo_buf_t *buf_ptr            = NULL;
   int8_t *    data_buf_ptr       = (int8_t *)input_buf_ptr->data_buf;
   for (uint32_t b = 0; b < sdata_ptr->bufs_num; b++)
   {
      buf_ptr                  = &in_port_ptr->common.bufs_ptr[b];
      buf_ptr->data_ptr        = data_buf_ptr;
      buf_ptr->actual_data_len = actual_len_per_buf;
      buf_ptr->max_data_len    = actual_len_per_buf;
      data_buf_ptr += actual_len_per_buf;
   }

#ifdef TIME_PROP_ENABLE
   /** copy external input timestamp and update ts_to_sync to the next expected timestmap if buffer has valid, ts to
    * sync will be updated based on the ext buffer TS so no need to update here. */
   bool_t new_ts_valid =
      cu_get_bits(input_buf_ptr->flags, DATA_BUFFER_FLAG_TIMESTAMP_VALID_MASK, DATA_BUFFER_FLAG_TIMESTAMP_VALID_SHIFT);

   /** updates timestamp validity flag to true/false  */
   sdata_ptr->flags.is_timestamp_valid = new_ts_valid;
   if (new_ts_valid && input_buf_ptr->actual_size)
   {
      sdata_ptr->timestamp = input_buf_ptr->timestamp;
   }
#endif // TIME_PROP_ENABLE

   // handle meta-data
   sdata_ptr->metadata_list_ptr = input_buf_ptr->metadata_list_ptr;
   input_buf_ptr->metadata_list_ptr = NULL;

#ifdef VERBOSE_DEBUGGING
   {
      GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                   DBG_LOW_PRIO,
                   "preprocess input: prev len %lu module (%lu of %lu) per buf, ext in (%lu of %lu), flags %08lX, "
                   "curr_trigger%u, inport flags0x%lX",
                   bytes_in_int_inp_md_prop,
                   in_port_ptr->common.bufs_ptr[0].actual_data_len,
                   in_port_ptr->common.bufs_ptr[0].max_data_len,
                   ext_in_port_ptr->buf.actual_data_len,
                   ext_in_port_ptr->buf.max_data_len,
                   sdata_ptr->flags.word,
                   me_ptr->topo.proc_context.curr_trigger,
                   in_port_ptr->flags.word);
   }
#endif
   return result;
}



FEF_CNTR_STATIC ar_result_t fef_cntr_input_dataQ_trigger_peer_cntr(gen_cntr_t *            me_ptr,
                                                                   gen_cntr_ext_in_port_t *ext_in_port_ptr)

{
   ar_result_t result = AR_EOK;
   INIT_EXCEPTION_HANDLING

   /* pops the front of the queue*/
   result = posal_queue_pop_front(ext_in_port_ptr->gu.this_handle.q_ptr,
                                  (posal_queue_element_t *)&(ext_in_port_ptr->cu.input_data_q_msg));
   if (AR_EOK != result)
   {
      ext_in_port_ptr->cu.input_data_q_msg.payload_ptr = NULL;

#ifdef VERBOSE_DEBUGGING
      GEN_CNTR_MSG(me_ptr->topo.gu.log_id, DBG_ERROR_PRIO, "Failed to pop the ext input buffer.");
#endif

      return result;
   }

#ifdef BUF_CNT_DEBUG
   if (ext_in_port_ptr->vtbl_ptr->is_data_buf(me_ptr, ext_in_port_ptr))
   {
      ext_in_port_ptr->cu.buf_recv_cnt++;
   }
#endif

#ifdef VERBOSE_DEBUGGING
   GEN_CNTR_MSG(me_ptr->topo.gu.log_id,
                DBG_LOW_PRIO,
                "Popped an input msg buffer 0x%lx with opcode 0x%x from (miid,port-id) (0x%lX, 0x%lx) queue "
                "act_len[%d] max_len[%d]",
                ext_in_port_ptr->cu.input_data_q_msg.payload_ptr,
                ext_in_port_ptr->cu.input_data_q_msg.msg_opcode,
                ext_in_port_ptr->gu.int_in_port_ptr->cmn.module_ptr->module_instance_id,
                ext_in_port_ptr->gu.int_in_port_ptr->cmn.id,
                ext_in_port_ptr->buf.actual_data_len,
                ext_in_port_ptr->buf.max_data_len);
#endif

   // process messages
   switch (ext_in_port_ptr->cu.input_data_q_msg.msg_opcode)
   {
      case SPF_MSG_DATA_BUFFER:
      {
         TRY(result, fef_cntr_input_data_set_up_peer_cntr(me_ptr, ext_in_port_ptr));
         break;
      }
      case SPF_MSG_DATA_MEDIA_FORMAT:
      {
         GEN_CNTR_MSG_ISLAND(me_ptr->topo.gu.log_id,
                             DBG_MED_PRIO,
                             "FEF_CNTR input media format update cmd from peer service to module, port index  (0x%lX, "
                             "%lu) ",
                             ext_in_port_ptr->gu.int_in_port_ptr->cmn.module_ptr->module_instance_id,
                             ext_in_port_ptr->gu.int_in_port_ptr->cmn.index);

         // Exit island temporarily for handling media format.If there is any event as part of this command
         // we will vote against island in that context
         gen_topo_exit_island_temporarily(&me_ptr->topo);
         gen_cntr_check_process_input_media_fmt_data_cmd(me_ptr, ext_in_port_ptr);
         break;
      }
      default:
      {
         GEN_CNTR_MSG_ISLAND(me_ptr->topo.gu.log_id,
                             DBG_ERROR_PRIO,
                             "FEF_CNTR received unsupported message 0x%lx",
                             ext_in_port_ptr->cu.input_data_q_msg.msg_opcode);
         result = AR_EUNSUPPORTED;
         result |= gen_cntr_free_input_data_cmd(me_ptr, ext_in_port_ptr, AR_EUNSUPPORTED, FALSE);
         break;
      }
   }

   CATCH(result, GEN_CNTR_MSG_PREFIX, me_ptr->topo.gu.log_id)
   {
   }
   return (result);
}
