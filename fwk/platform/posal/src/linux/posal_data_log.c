/**
 *  \file posal_data_log.c
 * \brief
 *  	This file contains utilities data logging for on-targe
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

/* =======================================================================
INCLUDE FILES FOR MODULE
========================================================================== */

#include "posal_data_log_i.h"
#include "ar_error_codes.h"
#include "posal.h"


#define INTERLEAVED 0

static void *log_alloc_buffer_internal(uint32_t                buf_size,
                                uint32_t                log_code,
                                posal_data_log_format_t data_fmt,
                                bool_t                  is_data_logging);

static ar_result_t log_buffer(void *log_pkt_ptr, posal_data_log_info_t *log_info_ptr);

bool_t posal_data_log_code_status(uint32_t log_code)
{
   return log_status(log_code);
}

uint32_t posal_data_log_get_max_buf_size()
{
   /* -1, as the payload is declared as 1 byte in the pkt struct*/
   uint32_t max_data_buf_size = (MAX_LOG_PKT_SIZE - (sizeof(log_pkt_header_internal_t) - 1));
   return max_data_buf_size;
}
/**
  Allocate the log packet for logging PCM data

  @param[in]  buf_Size      Size of data to be logged in BYTES
  @param[in]  log_code      posal log code
  @param[in]  log_code      data type, PCM or bitstream
  @return
  Void pointer to the log packet, if log allocation is
  successful

  @dependencies
  None.
 */
void *posal_data_log_alloc(uint32_t buf_size, uint32_t log_code, posal_data_log_format_t data_fmt)
{
   return log_alloc_buffer_internal(buf_size, log_code, data_fmt, FALSE);
}

/**
  Configure the audio log header with stream parameters and
  commit the packet for logging.

  param[in]  log_info_ptr  : Pointer to struct containing common
  information for populating the log header for both PCM and BS
  data

  @result
  graphite_result_t

  @dependencies
  None.
 */

ar_result_t posal_data_log_alloc_commit(posal_data_log_info_t *log_info_ptr)
{
   ar_result_t            result = AR_EOK;
   void *                 log_pkt_ptr;
   uint32_t               rem_log_buf_size, log_buf_size;
   int8_t *               curr_buf_ptr;
   audiolog_header_cmn_t *log_header_cmn;
   log_pkt_pcm_data_t *   pcm_data_pkt_ptr;
   uint32_t               total_log_buf_size;
   bool_t                 is_first_seg = TRUE;
   uint32_t               num_channels = 0;
   uint32_t               max_log_packet_size_based_on_mf = 0;
   uint8_t                interleaved;

   if (NULL == log_info_ptr)
   {
      AR_MSG(DBG_ERROR_PRIO, "Log info ptr is NULL");
      return AR_EFAILED;
   }
   if( LOG_DATA_FMT_PCM == log_info_ptr->data_fmt)
   {
      uint32_t bits_per_sample        = 0;
      uint32_t unit_size              = 0;
      num_channels                    = log_info_ptr->data_info.pcm_data_fmt.num_channels;
      bits_per_sample                 = log_info_ptr->data_info.pcm_data_fmt.bits_per_sample;
      unit_size                       = (bits_per_sample / 8) * num_channels;
      max_log_packet_size_based_on_mf = posal_data_log_get_max_buf_size() / unit_size;
      max_log_packet_size_based_on_mf = max_log_packet_size_based_on_mf * unit_size;
   }
   else
   {
      max_log_packet_size_based_on_mf = posal_data_log_get_max_buf_size();
   }

   curr_buf_ptr     = log_info_ptr->buf_ptr;
   rem_log_buf_size = log_info_ptr->buf_size;
   total_log_buf_size = log_info_ptr->buf_size;

   /*  If the buffer size to log is > max_log_packet_size_based_on_mf, logging per iteration
         is restricted to this size.
    */
   while (rem_log_buf_size > 0)
   {
      if (rem_log_buf_size < max_log_packet_size_based_on_mf)
      {
         log_buf_size = rem_log_buf_size;
      }
      else
      {
         log_buf_size = max_log_packet_size_based_on_mf;
      }
      // For data logging
      log_pkt_ptr = log_alloc_buffer_internal(log_buf_size, log_info_ptr->log_code, log_info_ptr->data_fmt, TRUE);

      if (NULL == log_pkt_ptr)
      {
#ifdef DATA_LOGGING_DBG
         AR_MSG(DBG_HIGH_PRIO,
                "Log buffer allocation failed for log code 0x%X, bufsize : %u "
                "bytes",
                log_info_ptr->log_code,
                log_buf_size);
#endif
         /* Skip these sequence count to detect packet drops */
         uint32_t skipped_log_seq_count = (rem_log_buf_size + max_log_packet_size_based_on_mf - 1) / max_log_packet_size_based_on_mf;
         *(log_info_ptr->seq_number_ptr) += skipped_log_seq_count;
         return AR_EFAILED;
      }

#ifdef DATA_LOGGING_DBG
      AR_MSG(DBG_HIGH_PRIO,
             "Log buffer alloc SUCCESS for log code 0x%X, bufsize : %u bytes",
             log_info_ptr->log_code,
             log_buf_size);
#endif

      log_info_ptr->buf_ptr  = curr_buf_ptr;
      log_info_ptr->buf_size = log_buf_size;

      pcm_data_pkt_ptr = (log_pkt_pcm_data_t *)log_pkt_ptr;
      log_header_cmn   = &(pcm_data_pkt_ptr->log_header_pcm.cmn_struct);

      /* For PCM format, with deinterleaved data if log buffer size is more than
              max value, it is segmented and each segment is logged individually.
       */
      interleaved = log_info_ptr->data_info.pcm_data_fmt.interleaved;

      if ((max_log_packet_size_based_on_mf == log_buf_size) && (total_log_buf_size > max_log_packet_size_based_on_mf) && (TRUE == is_first_seg) &&
          (LOG_DATA_FMT_PCM == log_info_ptr->data_fmt) && (!interleaved))
      {
         is_first_seg = FALSE;
      }

      if (FALSE == is_first_seg)
      {
         num_channels                    = log_info_ptr->data_info.pcm_data_fmt.num_channels;
         log_header_cmn->log_seg_number  = SEG_PKT;
         log_header_cmn->fragment_offset = (total_log_buf_size / (num_channels));
      }

      result = log_buffer(log_pkt_ptr, log_info_ptr);

      if (AR_EOK != result)
      {
         AR_MSG(DBG_HIGH_PRIO, "Error occured while populating log packet for log code 0x%X", log_info_ptr->log_code);
         /* Freeing up the allocated log buffer */
         log_free(log_pkt_ptr);
         return AR_EFAILED;
      }

      /* Increment the read/write pointers to buffer to be logged */

      rem_log_buf_size -= log_buf_size;

      if (FALSE == is_first_seg)
      {
         curr_buf_ptr += (log_buf_size / num_channels);
      }
      else
      {
         curr_buf_ptr += log_buf_size;
      }

   } /* while loop */

   return result;
}

/**
  Commit the log packet for logging PCM data

  @param[in]  buf_Size      Size of data to be logged in BYTES
  @param[in]  log_info_ptr  Logging info
  @return
   result

  @dependencies
  None.
 */
ar_result_t posal_data_log_commit(void *log_pkt_payload_ptr, posal_data_log_info_t *log_info_ptr)
{
   if (NULL == log_pkt_payload_ptr)
   {
      AR_MSG(DBG_ERROR_PRIO, "Error! NULL pointer passed to posal_data_log_commit");
      return AR_EBADPARAM;
   }

   switch (log_info_ptr->data_fmt)
   {
      case LOG_DATA_FMT_BITSTREAM:
      {
         // Get pointer of packet, the function takes in pointer to payload
         log_pkt_bitstream_data_t *log_pkt_ptr =
            (log_pkt_bitstream_data_t *)((uint8_t *)log_pkt_payload_ptr - sizeof(log_pkt_bitstream_data_t) +
                                         sizeof(log_pkt_ptr->payload));
         log_pkt_ptr->log_header_bs.cmn_struct.tag             = AUDIOLOG_CONTAINER_LOG_HEADER;
         log_pkt_ptr->log_header_bs.cmn_struct.size            = sizeof(audiolog_header_bitstream_data_t);
         log_pkt_ptr->log_header_bs.cmn_struct.log_session_id  = log_info_ptr->session_id;
         log_pkt_ptr->log_header_bs.cmn_struct.log_seg_number  = 0;
         log_pkt_ptr->log_header_bs.cmn_struct.segment_size    = log_info_ptr->buf_size;
         log_pkt_ptr->log_header_bs.cmn_struct.fragment_offset = 0;

         log_pkt_ptr->log_header_bs.cmn_struct.user_session_info.tag             = AUDIOLOG_CONTAINER_USER_SESSION;
         log_pkt_ptr->log_header_bs.cmn_struct.user_session_info.size            = sizeof(audiolog_user_session_t);
         log_pkt_ptr->log_header_bs.cmn_struct.user_session_info.user_session_id = 0;
         uint64_t timestamp                                                      = posal_timer_get_time();
         log_pkt_ptr->log_header_bs.cmn_struct.user_session_info.time_stamp      = timestamp;

         log_pkt_ptr->log_header_bs.bs_data_fmt.tag          = AUDIOLOG_CONTAINER_BS_DATA_FORMAT;
         log_pkt_ptr->log_header_bs.bs_data_fmt.size         = sizeof(audiolog_bitstream_data_format_t);
         log_pkt_ptr->log_header_bs.bs_data_fmt.log_tap_id   = log_info_ptr->log_tap_id;
         log_pkt_ptr->log_header_bs.bs_data_fmt.media_fmt_id = 0;
         log_commit(log_pkt_ptr);
         break;
      }
      case LOG_DATA_FMT_PCM:
      {
         // Get pointer of packet, the function takes in pointer to payload
         log_pkt_pcm_data_t *log_pkt_ptr =
            (log_pkt_pcm_data_t *)((uint8_t *)log_pkt_payload_ptr - sizeof(log_pkt_pcm_data_t) +
                                   sizeof(log_pkt_ptr->payload));
         log_pkt_ptr->log_header_pcm.cmn_struct.tag             = AUDIOLOG_CONTAINER_LOG_HEADER;
         log_pkt_ptr->log_header_pcm.cmn_struct.size            = sizeof(audiolog_header_bitstream_data_t);
         log_pkt_ptr->log_header_pcm.cmn_struct.log_session_id  = log_info_ptr->session_id;
         log_pkt_ptr->log_header_pcm.cmn_struct.log_seg_number  = 0;
         log_pkt_ptr->log_header_pcm.cmn_struct.segment_size    = log_info_ptr->buf_size;
         log_pkt_ptr->log_header_pcm.cmn_struct.fragment_offset = 0;

         log_pkt_ptr->log_header_pcm.cmn_struct.user_session_info.tag             = AUDIOLOG_CONTAINER_USER_SESSION;
         log_pkt_ptr->log_header_pcm.cmn_struct.user_session_info.size            = sizeof(audiolog_user_session_t);
         log_pkt_ptr->log_header_pcm.cmn_struct.user_session_info.user_session_id = 0;
         uint64_t timestamp                                                       = posal_timer_get_time();
         log_pkt_ptr->log_header_pcm.cmn_struct.user_session_info.time_stamp      = timestamp;

         log_pkt_ptr->log_header_pcm.pcm_data_fmt.minor_version = 1;
         log_pkt_ptr->log_header_pcm.pcm_data_fmt.tag           = AUDIOLOG_CONTAINER_BS_DATA_FORMAT;
         log_pkt_ptr->log_header_pcm.pcm_data_fmt.size          = sizeof(audiolog_pcm_data_format_t);
         log_pkt_ptr->log_header_pcm.pcm_data_fmt.log_tap_id    = log_info_ptr->log_tap_id;
         log_pkt_ptr->log_header_pcm.pcm_data_fmt.sampling_rate = log_info_ptr->data_info.pcm_data_fmt.sampling_rate;
         log_pkt_ptr->log_header_pcm.pcm_data_fmt.num_channels  = log_info_ptr->data_info.pcm_data_fmt.num_channels;
         //  log_pkt_ptr->log_header_pcm.pcm_data_fmt.pcm_width     = log_info_ptr->data_info.pcm_data_fmt.pcm_width;
         log_pkt_ptr->log_header_pcm.pcm_data_fmt.interleaved = log_info_ptr->data_info.pcm_data_fmt.interleaved;
         for (int i = 0; i < LOGGING_MAX_NUM_CH; i++)
         {
            log_pkt_ptr->log_header_pcm.pcm_data_fmt.channel_mapping[i] =
               (uint8_t)log_info_ptr->data_info.pcm_data_fmt.channel_mapping[i];
         }
         log_commit(log_pkt_ptr);
         break;
      }
      case LOG_DATA_FMT_RAW:
      {
         // Get pointer of packet, the function takes in pointer to payload
         // AR_MSG(DBG_MED_PRIO, "posal_commit(): log_pkt_payload_ptr = %p", log_pkt_payload_ptr);

         log_pkt_raw_data_t *log_pkt_ptr =
            (log_pkt_raw_data_t *)((uint8_t *)log_pkt_payload_ptr - sizeof(log_pkt_raw_data_t) +
                                   sizeof(log_pkt_ptr->payload));
         // AR_MSG(DBG_MED_PRIO, "posal_commit(): log_pkt_ptr = %p", log_pkt_ptr);
         log_commit(log_pkt_ptr);
         break;
      }
      default:
      {
         AR_MSG(DBG_ERROR_PRIO,
                "Invalid data format : %u for log code 0x%X",
                log_info_ptr->data_fmt,
                log_info_ptr->log_code);
         return AR_EFAILED;
      }
      break;
   }
   return AR_EOK;
}

/**
  Free the log packet

  @param[in]  log_pkt_payload_ptr  Log packet pointer
  @return
   None

  @dependencies
  None.
 */
void posal_data_log_free(void *log_pkt_payload_ptr)
{
   if (NULL == log_pkt_payload_ptr)
   {
      AR_MSG(DBG_ERROR_PRIO, "Error! NULL pointer passed to posal_data_log_free");
   }
   else
   {
      log_pkt_bitstream_data_t *log_pkt_ptr =
         (log_pkt_bitstream_data_t *)((uint8_t *)log_pkt_payload_ptr - sizeof(log_pkt_bitstream_data_t) +
                                      sizeof(log_pkt_ptr->payload));
      log_free(log_pkt_ptr);
   }
}

static void *log_alloc_buffer_internal(uint32_t                buf_size,
                                uint32_t                log_code,
                                posal_data_log_format_t data_fmt,
                                bool_t                  is_data_logging)
{
   uint32_t log_pkt_size;
   uint32_t log_header_size;
   void *   log_pkt_ptr         = NULL;
   void *   log_pkt_payload_ptr = NULL;

   /* First calculate the log header size */
   switch (data_fmt)
   {
      case LOG_DATA_FMT_PCM:
      {
         /* -1, as the payload is declared as 1 byte in the pkt struct*/
         log_header_size = (sizeof(log_pkt_pcm_data_t) - 1);
         break;
      }
      case LOG_DATA_FMT_BITSTREAM:
      {
         log_header_size = (sizeof(log_pkt_bitstream_data_t) - 1);
         break;
      }
      case LOG_DATA_FMT_RAW:
      {
         log_header_size = (sizeof(log_pkt_raw_data_t) - 1);
         break;
      }
      default:
      {
         AR_MSG(DBG_ERROR_PRIO, "Invalid data format : %u for log code 0x%X", data_fmt, log_code);
         return NULL;
      }
      break;
   }
   /* Caclulate total packet size including the data payload */
   log_pkt_size = (log_header_size + buf_size);

   /* Allocate the log packet
    * log_alloc() returns pointer to the allocated log packet
    * Returns NULL if log code is disabled on the GUI
    */

   log_pkt_ptr = log_alloc(log_code, log_pkt_size);
   AR_MSG(DBG_LOW_PRIO, "posal_data_alloc(): log_pkt_ptr= %p, Req Pkt Size= %ubytes log_code= 0x%X", log_pkt_ptr, log_pkt_size, log_code);
   //return pkt pointer for data logging
   if (NULL == log_pkt_ptr)
   {
      AR_MSG(DBG_MED_PRIO, "posal_data_alloc(): Invalid log ptr allocation log_pkt_ptr = %p", log_pkt_ptr);
      return NULL;
   }
   // return pkt pointer for data logging
   if (is_data_logging)
   {
      return log_pkt_ptr;
   }

   if (LOG_DATA_FMT_PCM == data_fmt)
   {
      log_pkt_payload_ptr = &(((log_pkt_pcm_data_t *)log_pkt_ptr)->payload);
   }
   else if (LOG_DATA_FMT_BITSTREAM == data_fmt)
   {
      log_pkt_payload_ptr = &(((log_pkt_bitstream_data_t *)log_pkt_ptr)->payload);
   }
   else
   {
      log_pkt_payload_ptr = &(((log_pkt_raw_data_t *)log_pkt_ptr)->payload);
   }
   AR_MSG(DBG_MED_PRIO, "posal_data_alloc(): log_pkt_payload_ptr = %p", log_pkt_payload_ptr);
   return log_pkt_payload_ptr;
}

static ar_result_t log_buffer(void *log_pkt_ptr, posal_data_log_info_t *log_info_ptr)
{
   ar_result_t                       result = AR_EOK;
   audiolog_header_cmn_t *           log_header_cmn;
   audiolog_pcm_data_format_t *      pcm_data_fmt;
   audiolog_bitstream_data_format_t *bs_data_fmt;
   posal_data_log_pcm_info_t *       pcm_data_info_ptr;
   uint32_t                          i;
   uint8_t *                         log_dst_ptr = NULL;
   log_pkt_pcm_data_t *              pcm_data_pkt_ptr;
   log_pkt_bitstream_data_t *        bs_data_pkt_ptr;
   bool_t                            is_seg_pkt                  = FALSE;
   uint32_t                          num_channels                = 0;
   uint32_t                          num_bytes_per_channel       = 0;
   uint32_t                          total_num_bytes_per_channel = 0;

   if ((NULL == log_info_ptr) || (NULL == log_pkt_ptr))
   {
      AR_MSG(DBG_ERROR_PRIO, "Log pkt ptr or log info ptr is NULL");
      return AR_EFAILED;
   }

   /* Check if the data format is either PCM or BITSTREAM */
   if ((log_info_ptr->data_fmt != LOG_DATA_FMT_PCM) && (log_info_ptr->data_fmt != LOG_DATA_FMT_BITSTREAM))
   {
      AR_MSG(DBG_ERROR_PRIO,
             "Invalid data format : %u for log code 0x%X",
             log_info_ptr->data_fmt,
             log_info_ptr->log_code);
      return AR_EFAILED;
   }

   pcm_data_pkt_ptr = (log_pkt_pcm_data_t *)log_pkt_ptr;

   /* Common for both PCM and BS data */
   log_header_cmn = &(pcm_data_pkt_ptr->log_header_pcm.cmn_struct);

   if (SEG_PKT == log_header_cmn->log_seg_number)
   {
      is_seg_pkt = TRUE;

      total_num_bytes_per_channel = log_header_cmn->fragment_offset;
   }

   /* Fill in the common header for PCM and bitstream data */

   /*************** AUDIO_LOG_HEADER ***************/
   log_header_cmn->tag = AUDIOLOG_CONTAINER_LOG_HEADER;

   if (LOG_DATA_FMT_PCM == log_info_ptr->data_fmt)
   {
      log_header_cmn->size = sizeof(audiolog_header_pcm_data_t);
   }
   else /* bitstream format */
   {
      log_header_cmn->size = sizeof(audiolog_header_bitstream_data_t);
   }

   /* Session ID is any random but unique value corresponding to a log code.
         For same log code and same tapping point, parser generates a new
      file
         with change in session ID. In general, session ID should be made
      function
         of stream/device parameters, so that a new log files gets generated if
      any
         of the parameter changes.
    */

   log_header_cmn->log_session_id = log_info_ptr->session_id;

   /* Increment sequence count on every log */
   log_header_cmn->log_seg_number = (*(log_info_ptr->seq_number_ptr))++;

   /* Size (in BYTES) of actual buffer to be logged */
   log_header_cmn->segment_size = log_info_ptr->buf_size;

   /* Un-used for now, set to 0*/
   log_header_cmn->fragment_offset = 0;

   /*************** AUDIO_LOG_USER_SESSION ***************/
   log_header_cmn->user_session_info.tag  = AUDIOLOG_CONTAINER_USER_SESSION;
   log_header_cmn->user_session_info.size = sizeof(audiolog_user_session_t);
   /* User session ID is un-used parameter */
   log_header_cmn->user_session_info.user_session_id = 0;
   log_header_cmn->user_session_info.time_stamp      = log_info_ptr->log_time_stamp;

   switch (log_info_ptr->data_fmt)
   {
      case LOG_DATA_FMT_PCM:
      {
         /*************** AUDIOLOG_PCM_DATA_FORAMT ***************/

         pcm_data_fmt = (audiolog_pcm_data_format_t *)(&(pcm_data_pkt_ptr->log_header_pcm.pcm_data_fmt));

         pcm_data_info_ptr = (posal_data_log_pcm_info_t *)(&log_info_ptr->data_info.pcm_data_fmt);

         pcm_data_fmt->minor_version = 1;
         pcm_data_fmt->tag           = AUDIOLOG_CONTAINER_PCM_DATA_FORMAT;
         pcm_data_fmt->size          = sizeof(audiolog_pcm_data_format_t);
         pcm_data_fmt->log_tap_id    = log_info_ptr->log_tap_id;
         pcm_data_fmt->sampling_rate = pcm_data_info_ptr->sampling_rate;
         pcm_data_fmt->num_channels  = pcm_data_info_ptr->num_channels;
         pcm_data_fmt->pcm_width     = pcm_data_info_ptr->bits_per_sample;
         pcm_data_fmt->interleaved   = (pcm_data_info_ptr->interleaved == 0) ? 1 : 0;

         if (NULL != pcm_data_info_ptr->channel_mapping)
         {
            for (i = 0; i < pcm_data_info_ptr->num_channels; i++)
            {
               pcm_data_fmt->channel_mapping[i] = (uint8_t)(pcm_data_info_ptr->channel_mapping[i]);
            }
         }
         else /* Provide the default mapping */
         {
            for (i = 0; i < pcm_data_info_ptr->num_channels; i++)
            {
               pcm_data_fmt->channel_mapping[i] = i + 1;
            }
         }

         /* Set the remaining channels elements as un-used */
         for (i = pcm_data_info_ptr->num_channels; i < LOGGING_MAX_NUM_CH; i++)
         {
            pcm_data_fmt->channel_mapping[i] = 0;
         }

         log_dst_ptr = ((log_pkt_pcm_data_t *)log_pkt_ptr)->payload;
      }
      break;
      case LOG_DATA_FMT_BITSTREAM:
      {
         /********** AUDIOLOG_BITSTREAM_DATA_FORAMT *************/

         bs_data_pkt_ptr = (log_pkt_bitstream_data_t *)log_pkt_ptr;

         bs_data_fmt = (audiolog_bitstream_data_format_t *)(&(bs_data_pkt_ptr->log_header_bs.bs_data_fmt));

         bs_data_fmt->tag          = AUDIOLOG_CONTAINER_BS_DATA_FORMAT;
         bs_data_fmt->size         = sizeof(audiolog_bitstream_data_format_t);
         bs_data_fmt->log_tap_id   = log_info_ptr->log_tap_id;
         bs_data_fmt->media_fmt_id = log_info_ptr->data_info.media_fmt_id;

         log_dst_ptr = ((log_pkt_bitstream_data_t *)log_pkt_ptr)->payload;
      }
      break;
      case LOG_DATA_FMT_RAW:
      {
         log_dst_ptr = ((log_pkt_raw_data_t *)log_pkt_ptr)->payload;
      }
      break;
   }

   /* Populate the log packet payload with the buffer to be logged */

   if (FALSE == is_seg_pkt)
   {
      memscpy(log_dst_ptr, log_info_ptr->buf_size, log_info_ptr->buf_ptr, log_info_ptr->buf_size);
   }
   else /* True */
   {
      pcm_data_info_ptr = (posal_data_log_pcm_info_t *)(&log_info_ptr->data_info.pcm_data_fmt);

      num_channels          = pcm_data_info_ptr->num_channels;
      num_bytes_per_channel = (log_info_ptr->buf_size / num_channels);

      for (i = 0; i < num_channels; i++)
      {
         memscpy(log_dst_ptr, num_bytes_per_channel, log_info_ptr->buf_ptr, num_bytes_per_channel);
         log_dst_ptr += num_bytes_per_channel;
         log_info_ptr->buf_ptr += total_num_bytes_per_channel;
      }
   }

   /* Call the API to commit the log packet */
   log_commit(log_pkt_ptr);

   return result;
}
