/**
 *   \file circular_buffer_read_island.c
 *   \brief
 *        This file contains implementation of circular buffering
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#include "circular_buffer_i.h"
/*
 * Read samples from the Circular buffer
 * Full documentation in circ_buf_utils.h
 */
circbuf_result_t circ_buf_read(circ_buf_client_t *rd_client_ptr,
                               int8_t *           out_ptr,
                               uint32_t           bytes_to_read,
                               uint32_t *         actual_read_len_ptr)
{
   if ((NULL == out_ptr) || (NULL == rd_client_ptr->rw_chunk_node_ptr) || (NULL == rd_client_ptr->circ_buf_ptr) ||
       (bytes_to_read > rd_client_ptr->circ_buf_ptr->circ_buf_size))
   {
      AR_MSG_ISLAND(DBG_ERROR_PRIO,
                    "CIRC_BUF: Read failed. Invalid input params. Rcvd bytes to read = %u",
                    bytes_to_read);
      return CIRCBUF_FAIL;
   }

#ifdef DEBUG_CIRC_BUF_UTILS
   AR_MSG_ISLAND(DBG_HIGH_PRIO,
                 "CIRC_BUF: Circ Buf read, bytes_to_read= %u ; unread_bytes = %u",
                 bytes_to_read,
                 rd_client_ptr->unread_bytes);
#endif

   if (bytes_to_read > rd_client_ptr->unread_bytes)
   {
      if (rd_client_ptr->unread_bytes)
      {
         bytes_to_read = rd_client_ptr->unread_bytes;
         AR_MSG_ISLAND(DBG_LOW_PRIO,
                       "bytes_to_read: %lu greater than unread_bytes: %lu",
                       bytes_to_read,
                       rd_client_ptr->unread_bytes);
      }
      else
      {
#ifdef DEBUG_CIRC_BUF_UTILS
         AR_MSG_ISLAND(DBG_ERROR_PRIO,
                       "Circ Buf underrun, bytes to read = %u, unread bytes = %u",
                       bytes_to_read,
                       rd_client_ptr->unread_bytes);
#endif
         return CIRCBUF_UNDERRUN;
      }
   }

   uint32_t          counter             = bytes_to_read;
   spf_list_node_t **rd_chunk_ptr_ptr    = &rd_client_ptr->rw_chunk_node_ptr;
   uint32_t *        rw_chunk_offset_ptr = &rd_client_ptr->rw_chunk_offset;
   while (counter > 0)
   {
      chunk_buffer_t *chunk_ptr = (chunk_buffer_t *)(*rd_chunk_ptr_ptr)->obj_ptr;

#ifdef DEBUG_CIRC_BUF_UTILS
      AR_MSG_ISLAND(DBG_HIGH_PRIO,
                    "circ_buf_read: buf_id: 0x%x, chunk:%d, chunk_ptr: 0x%x, offset: %lu, buf_size: %lu, counter: %lu",
                    rd_client_ptr->circ_buf_ptr->id,
                    chunk_ptr->id,
                    chunk_ptr,
                    rd_client_ptr->rw_chunk_offset,
                    chunk_ptr->size,
                    counter);
#endif

      /* If the bytes to read is less than the remaining bytes in the chunk */
      if (counter < (chunk_ptr->size - *rw_chunk_offset_ptr))
      {
         /* Copy the less than chunk size counter */
         *rw_chunk_offset_ptr +=
            memscpy(&out_ptr[bytes_to_read - counter], counter, chunk_ptr->buffer_ptr + *rw_chunk_offset_ptr, counter);
         counter = 0;
      }
      else
      {
         /* Write after the previously read bytes (bytes_to_read - counter) */
         /* Assumes that out_ptr is trusted input with sufficient length */
         counter -= memscpy(&out_ptr[bytes_to_read - counter],
                            counter,
                            chunk_ptr->buffer_ptr + *rw_chunk_offset_ptr,
                            chunk_ptr->size - *rw_chunk_offset_ptr);

         /* Advance to next chunk */
         add_circ_buf_next_chunk_node(rd_client_ptr->circ_buf_ptr, rd_chunk_ptr_ptr);
         *rw_chunk_offset_ptr = 0;
      }
   }

   // Update the client's unread bytes counter.
   rd_client_ptr->unread_bytes -= bytes_to_read;

   *actual_read_len_ptr = bytes_to_read;
   return CIRCBUF_SUCCESS;
}
