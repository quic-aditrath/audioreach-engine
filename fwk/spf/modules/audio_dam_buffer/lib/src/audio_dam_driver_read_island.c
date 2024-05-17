/**
 *   \file audio_dam_driver_read_island.c
 *   \brief
 *        This file contains implementation of Audio Dam buffer driver
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#include "audio_dam_driver.h"
#include "audio_dam_driver_i.h"
#include "circular_buffer_i.h"

static ar_result_t audio_dam_stream_read_util_(audio_dam_stream_reader_t *reader_handle,   // in
                                               uint32_t                   num_chs_to_read, // in
                                               uint32_t    bytes_req_to_read, // in, considered valid only if non zero
                                               capi_buf_t *output_buf_arr,    // in/out
                                               bool_t     *output_buf_ts_is_valid, // out
                                               int64_t    *output_buf_ts,          // out
                                               uint32_t   *output_buf_len_in_us)     // out
{
   ar_result_t         result     = AR_EOK;
   circbuf_result_t    buf_result = CIRCBUF_SUCCESS;
   audio_dam_driver_t *drv_ptr    = reader_handle->driver_ptr;
   // Iterate through all the channel write clients and write the input data
   for (uint32_t iter = 0; iter < num_chs_to_read; iter++)
   {
      int8_t  *frame_ptr          = NULL;
      uint32_t max_read_frame_len = 0;
      uint32_t actual_data_len    = 0;
      if (reader_handle->driver_ptr->is_raw_compressed)
      {
         audio_dam_raw_comp_frame_header_t *scratch_frame_ptr =
            (audio_dam_raw_comp_frame_header_t *)reader_handle->driver_ptr->ch_frame_scratch_buf_ptr;
         scratch_frame_ptr->magic_word      = 0;
         scratch_frame_ptr->actual_data_len = 0;

         frame_ptr          = (int8_t *)scratch_frame_ptr;
         max_read_frame_len = AUDIO_DAM_GET_TOTAL_FRAME_LEN(reader_handle->driver_ptr->frame_max_data_len_in_bytes);
      }
      else
      {
         frame_ptr          = output_buf_arr[iter].data_ptr + output_buf_arr[iter].actual_data_len;
         max_read_frame_len = (bytes_req_to_read)
                                 ? bytes_req_to_read
                                 : output_buf_arr[iter].max_data_len - output_buf_arr[iter].actual_data_len;
      }

      buf_result =
         circ_buf_read(reader_handle->rd_client_ptr_arr[iter], frame_ptr, max_read_frame_len, &actual_data_len);
      if (CIRCBUF_UNDERRUN == buf_result)
      {
         return AR_ENEEDMORE;
      }

      /*  Copies from scratch to capi buffer (for Raw only) */
      if (reader_handle->driver_ptr->is_raw_compressed)
      {
         // write data to local ch buffer
         audio_dam_raw_comp_frame_header_t *scratch_frame_ptr = (audio_dam_raw_comp_frame_header_t *)frame_ptr;

#ifdef DEBUG_AUDIO_DAM_DRIVER
         DAM_MSG_ISLAND(drv_ptr->iid,
                        DBG_HIGH_PRIO,
                        "read: magic_word:%lu, frame_len:%lu actual_len:%lu",
                        scratch_frame_ptr->magic_word,
                        max_read_frame_len,
                        scratch_frame_ptr->actual_data_len);
#endif

         if (max_read_frame_len != actual_data_len)
         {
            DAM_MSG_ISLAND(drv_ptr->iid,
                           DBG_ERROR_PRIO,
                           "read: Error reading compressed frame! magic_word:%lu, actual_len:%lu expected_len:%lu",
                           scratch_frame_ptr->magic_word,
                           actual_data_len,
                           max_read_frame_len);
            return AR_EFAILED;
         }

         if (AUDIO_DAM_RAW_COMPRESSED_FRAME_MAGIC_WORD != scratch_frame_ptr->magic_word)
         {
            DAM_MSG_ISLAND(drv_ptr->iid,
                           DBG_ERROR_PRIO,
                           "read: Error reading compressed frame! magic_word:%lu, actual_len:%lu",
                           scratch_frame_ptr->magic_word,
                           scratch_frame_ptr->actual_data_len);
            return AR_EFAILED;
         }

         if (scratch_frame_ptr->actual_data_len >
             (output_buf_arr[iter].max_data_len - output_buf_arr[iter].actual_data_len))
         {
            DAM_MSG_ISLAND(drv_ptr->iid,
                           DBG_ERROR_PRIO,
                           "read: Error copying compressed frame from scratch to capi output!  actual_len:%lu, "
                           "free_space:%lu",
                           scratch_frame_ptr->actual_data_len,
                           (output_buf_arr[iter].max_data_len - output_buf_arr[iter].actual_data_len));
            return AR_EFAILED;
         }

         memscpy(output_buf_arr[iter].data_ptr + output_buf_arr[iter].actual_data_len,
                 output_buf_arr[iter].max_data_len - output_buf_arr[iter].actual_data_len,
                 &scratch_frame_ptr->data[0],
                 scratch_frame_ptr->actual_data_len);

         output_buf_arr[iter].actual_data_len += scratch_frame_ptr->actual_data_len;
      }
      else // PCM data
      {
         output_buf_arr[iter].actual_data_len += actual_data_len;
      }

#ifdef DEBUG_AUDIO_DAM_DRIVER
      DAM_MSG_ISLAND(drv_ptr->iid,
                     DBG_LOW_PRIO,
                     "DAM_DRIVER: Stream reader data read done. ch_id=%d, iter = %d, actual_data_len=%d ",
                     reader_handle->rd_client_ptr_arr[iter]->circ_buf_ptr->id,
                     iter,
                     output_buf_arr[iter].actual_data_len);
#endif
   }

   // Update Output buffer timestamp based on the amount of data read
   uint32_t remaining_unread_len_in_us =
      audio_dam_compute_buffer_size_in_us(reader_handle->driver_ptr,
                                          reader_handle->rd_client_ptr_arr[0]->unread_bytes,
                                          TRUE);

   uint32_t output_frame_len_us =
      audio_dam_compute_buffer_size_in_us(reader_handle->driver_ptr, output_buf_arr[0].actual_data_len, FALSE);

   *output_buf_ts_is_valid = reader_handle->rd_client_ptr_arr[0]->circ_buf_ptr->is_valid_timestamp;

   int64_t cur_writer_ts = reader_handle->rd_client_ptr_arr[0]->circ_buf_ptr->timestamp;
   *output_buf_ts        = cur_writer_ts - (remaining_unread_len_in_us + output_frame_len_us);

   *output_buf_len_in_us = output_frame_len_us;

#ifdef DEBUG_AUDIO_DAM_DRIVER
   DAM_MSG_ISLAND(drv_ptr->iid,
                  DBG_HIGH_PRIO,
                  "DAM_DRIVER: Output Buffer ts: (%ld, %lu) out_len_us:%lu writer_ts:%lu unread_len:(%lu, %lu)",
                  *output_buf_ts_is_valid,
                  *output_buf_ts,
                  output_frame_len_us,
                  cur_writer_ts,
                  reader_handle->rd_client_ptr_arr[0]->unread_bytes,
                  remaining_unread_len_in_us);
#endif
   return result;
}

static ar_result_t audio_dam_stream_batch_read(audio_dam_stream_reader_t *reader_handle,          // in
                                               uint32_t                   num_chs_to_read,        // in
                                               capi_buf_t *               output_buf_arr,         // in/out
                                               bool_t *                   output_buf_ts_is_valid, // out
                                               int64_t *                  output_buf_ts,          // out
                                               uint32_t *                 output_buf_len_in_us)                    // out
{
   ar_result_t result = AR_EOK;
   uint32_t    requested_batch_us = reader_handle->data_batching_us;

   uint32_t requested_batch = audio_dam_compute_buffer_size_in_bytes(reader_handle->driver_ptr, requested_batch_us);

   // setting pending data : pending_batch_bytes = pending_data
   if (0 == reader_handle->pending_batch_bytes)
   {
      if (reader_handle->rd_client_ptr_arr[0]->unread_bytes >= requested_batch)
      {
         reader_handle->pending_batch_bytes = requested_batch;
#ifdef DEBUG_AUDIO_DAM_DRIVER
         AR_MSG_ISLAND(DBG_MED_PRIO, "DAM_DRIVER: pending_batch_bytes = %lu ", reader_handle->pending_batch_bytes);
#endif
      }
      else
      {
         /** batching requirement not met, need more data to be buffer */
         return result;
      }
   }

   uint32_t pending_batch_bytes  = reader_handle->pending_batch_bytes;
   uint32_t actual_bytes_to_read = 0;

   // if pending bytes are less than a frame length (not full output buffer) then send partial data
   if (pending_batch_bytes >= (output_buf_arr[0].max_data_len - output_buf_arr[0].actual_data_len))
   {
      actual_bytes_to_read = output_buf_arr[0].max_data_len - output_buf_arr[0].actual_data_len;
   }
   else
   {
      actual_bytes_to_read = pending_batch_bytes;
   }

   result = audio_dam_stream_read_util_(reader_handle,
                                        num_chs_to_read,
                                        actual_bytes_to_read,
                                        output_buf_arr,
                                        output_buf_ts_is_valid,
                                        output_buf_ts,
                                        output_buf_len_in_us);

   // update bytes to read
   if (AR_EOK == result)
   {
      reader_handle->pending_batch_bytes -= output_buf_arr[0].actual_data_len;
   }

   return result;
}

ar_result_t audio_dam_stream_read(audio_dam_stream_reader_t *reader_handle,          // in
                                  uint32_t                   num_chs_to_read,        // in
                                  capi_buf_t                *output_buf_arr,         // in/out
                                  bool_t                    *output_buf_ts_is_valid, // out
                                  int64_t                   *output_buf_ts,          // out
                                  uint32_t                  *output_buf_len_in_us)   // out
{
   ar_result_t result = AR_EOK;

   if (0 == num_chs_to_read)
   {
      return AR_EFAILED;
   }

   if (audio_dam_driver_is_virtual_writer_mode(reader_handle))
   {
      return audio_dam_stream_read_from_virtual_buf(reader_handle,
                                                    num_chs_to_read,
                                                    output_buf_arr,
                                                    output_buf_ts_is_valid,
                                                    output_buf_ts,
                                                    output_buf_len_in_us);
   }
   else if (reader_handle->is_batch_streaming)
   {
      return audio_dam_stream_batch_read(reader_handle,
                                         num_chs_to_read,
                                         output_buf_arr,
                                         output_buf_ts_is_valid,
                                         output_buf_ts,
                                         output_buf_len_in_us);
   }
   else
   {
      return audio_dam_stream_read_util_(reader_handle,
                                         num_chs_to_read,
                                         0, /** zero means read as much as that fits in output*/
                                         output_buf_arr,
                                         output_buf_ts_is_valid,
                                         output_buf_ts,
                                         output_buf_len_in_us);
   }

   return result;
}

ar_result_t audio_dam_get_stream_reader_unread_bytes(audio_dam_stream_reader_t *reader_handle, uint32_t *unread_bytes)
{
   ar_result_t result = AR_EOK;

   if (NULL == reader_handle)
   {
      return AR_ENOTREADY;
   }

   if (reader_handle->virt_buf_ptr)
   {
      // for virtual buffer
      *unread_bytes = 0;
   }
   else
   {
      *unread_bytes = reader_handle->rd_client_ptr_arr[0]->unread_bytes;
   }

   return result;
}
