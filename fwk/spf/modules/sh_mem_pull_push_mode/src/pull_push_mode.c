/*========================================================================

 */ /**
This file contains function definitions internal to CAPI Pull
mode and Push mode module

  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
  SPDX-License-Identifier: BSD-3-Clause

  */
/*====================================================================== */

/* =======================================================================
INCLUDE FILES FOR MODULE
========================================================================== */

#include "posal_thread_prio.h"
#include "sh_mem_pull_push_mode_api.h"

#include "ar_msg.h"
#include "apm.h"
#include "pull_push_mode.h"
#ifndef PM_UNIT_TEST
#include "posal_timer.h"
#include "posal_memorymap.h"
#include "posal_memory.h"
#include "spf_interleaver.h"
#else
#include "test_util.h"
#endif



static capi_err_t pull_push_mode_check_send_watermark_event(capi_pm_t *pm_ptr,
                                                               uint32_t      startLevel,
                                                               uint32_t      endLevel);

capi_err_t pull_push_mode_watermark_levels_init(pull_push_mode_t *pm_ptr,
                                                   uint32_t          num_water_mark_levels,
                                                   event_cfg_sh_mem_pull_push_mode_watermark_level_t *water_mark_levels,
                                                   uint32_t                                           heap_id)
{
   uint32_t watermark_level_bytes = 0;
   pm_ptr->num_water_mark_levels  = num_water_mark_levels;

   if (NULL != pm_ptr->water_mark_levels_ptr)
   {
      posal_memory_free(pm_ptr->water_mark_levels_ptr);
   }

   if (0 < pm_ptr->num_water_mark_levels)
   {
      pm_ptr->water_mark_levels_ptr =
         (pull_push_mode_watermark_level_t *)posal_memory_malloc(pm_ptr->num_water_mark_levels *
                                                                    sizeof(pull_push_mode_watermark_level_t),
                                                                 (POSAL_HEAP_ID)heap_id);

      if (NULL == pm_ptr->water_mark_levels_ptr)
      {
         AR_MSG(DBG_ERROR_PRIO,
                "pull_push_mode_watermark_levels_init: Failed to allocate memory for water mark events,"
                "num_water_mark_levels: %d",
                pm_ptr->num_water_mark_levels);
         return CAPI_ENOMEMORY;
      }

      for (uint32_t i = 0; i < pm_ptr->num_water_mark_levels; i++)
      {
         AR_MSG(DBG_HIGH_PRIO,
                "pull_push_mode_watermark_levels_init: Water Mark Level %lu bytes",
                water_mark_levels[i].watermark_level_bytes);

         watermark_level_bytes = water_mark_levels[i].watermark_level_bytes;

         if (watermark_level_bytes <= pm_ptr->shared_circ_buf_size)
         {
            pm_ptr->water_mark_levels_ptr[i].watermark_level_bytes = watermark_level_bytes;
         }
         else
         {
            AR_MSG(DBG_ERROR_PRIO, "pull_push_mode_watermark_levels_init: Invalid water mark level.");
            return CAPI_EFAILED;
         }
      }
   }

   return CAPI_EOK;
}

capi_err_t pull_push_mode_init(pull_push_mode_t *pm_ptr, sh_mem_pull_push_mode_cfg_t *init_ptr)
{
   capi_err_t result = CAPI_EOK;

   if ((NULL == pm_ptr) || (NULL == init_ptr))
   {
      AR_MSG(DBG_ERROR_PRIO, "pull_push_mode_init: Invalid params!");
      return CAPI_EBADPARAM;
   }

   if ((0 != pm_ptr->pos_buf_mem_map_handle) || (0 != pm_ptr->circ_buf_mem_map_handle))
   {
      AR_MSG(DBG_ERROR_PRIO, "pull_push_mode_init: already configured with circ buf or pos buf memories");
      return CAPI_EBADPARAM;
   }

   pm_ptr->mem_map_client = apm_get_mem_map_client();

   pm_ptr->pos_buf_mem_map_handle  = init_ptr->pos_buf_mem_map_handle;
   pm_ptr->circ_buf_mem_map_handle = init_ptr->circ_buf_mem_map_handle;

   if (CAPI_FAILED(
          result = posal_memorymap_get_virtual_addr_from_shm_handle_v2(pm_ptr->mem_map_client,
                                                                       pm_ptr->pos_buf_mem_map_handle,
                                                                       init_ptr->shared_pos_buf_addr_lsw,
                                                                       init_ptr->shared_pos_buf_addr_msw,
                                                                       sizeof(sh_mem_pull_push_mode_position_buffer_t),
                                                                       TRUE, // decrement ref count in deinit
                                                                       &(pm_ptr->shared_pos_buf_ptr))))
   {
      AR_MSG(DBG_ERROR_PRIO,
             "pull_push_mode_init: Pos Buf Phy to Virt Failed, physical addr: %lx%lx, result: %d \n",
             init_ptr->shared_pos_buf_addr_lsw,
             init_ptr->shared_pos_buf_addr_msw,
             result);
      return CAPI_EFAILED;
   }

   if (CAPI_FAILED(
          result =
             posal_memorymap_get_virtual_addr_from_shm_handle_v2(pm_ptr->mem_map_client,
                                                                 pm_ptr->circ_buf_mem_map_handle,
                                                                 init_ptr->shared_circ_buf_addr_lsw,
                                                                 init_ptr->shared_circ_buf_addr_msw,
                                                                 init_ptr->shared_circ_buf_size,
                                                                 TRUE, // Need to decrement ref count to release the handle
                                                                 &(pm_ptr->shared_circ_buf_start_ptr))))
   {
      AR_MSG(DBG_ERROR_PRIO,
             "pull_push_mode_init: Circ Buf Phy to Virt Failed, physical addr: %lx%lx, result: %d \n",
             init_ptr->shared_circ_buf_addr_msw,
             init_ptr->shared_circ_buf_addr_lsw,
             result);
      return CAPI_EFAILED;
   }

   pm_ptr->shared_circ_buf_size = init_ptr->shared_circ_buf_size;

   AR_MSG(DBG_HIGH_PRIO, "pull_push_mode_init: pos buf addr: %lx", (uint32_t *)pm_ptr->shared_pos_buf_ptr);

   AR_MSG(DBG_HIGH_PRIO, "pull_push_mode_init: circ buf addr: %lx", (uint32_t *)pm_ptr->shared_circ_buf_start_ptr);

   AR_MSG(DBG_HIGH_PRIO, "pull_push_mode_init: circ buf size: %lu", pm_ptr->shared_circ_buf_size);

   // initialize the position buffer to zero
   memset(pm_ptr->shared_pos_buf_ptr, 0, sizeof(sh_mem_pull_push_mode_position_buffer_t));

   pm_ptr->num_water_mark_levels = 0;

   if (CAPI_FAILED(result = pull_push_mode_check_buf_size(pm_ptr, &pm_ptr->media_fmt)))
   {
	   AR_MSG(DBG_ERROR_PRIO, "pull_push_mode_deinit: releasing shared buffer.");
	   pull_push_mode_deinit(pm_ptr);
	   return result;
   }

   return result;
}

capi_err_t pull_push_mode_set_fwk_ext_inp_media_fmt(pull_push_mode_t *                      pm_ptr,
                                                       fwk_extn_pcm_param_id_media_fmt_extn_t *extn_ptr)
{

   if ((16 != extn_ptr->bit_width) && (24 != extn_ptr->bit_width) && (32 != extn_ptr->bit_width))
   {
      AR_MSG(DBG_ERROR_PRIO, "CAPI PM: set param received bad param for bit_width %d", extn_ptr->bit_width);
      return CAPI_EBADPARAM;
   }
   if ((PCM_LSB_ALIGNED != extn_ptr->alignment) && (PCM_MSB_ALIGNED != extn_ptr->alignment))
   {
      AR_MSG(DBG_ERROR_PRIO, "CAPI PM: set param received bad param for alignment");
      return CAPI_EBADPARAM;
   }

   if ((PCM_LITTLE_ENDIAN != extn_ptr->endianness) && (PCM_BIG_ENDIAN != extn_ptr->endianness))
   {
      AR_MSG(DBG_ERROR_PRIO, "CAPI PM: set param received bad param for endianness");
      return CAPI_EBADPARAM;
   }

   pm_ptr->media_fmt.bit_width  = extn_ptr->bit_width;
   pm_ptr->media_fmt.alignment  = extn_ptr->alignment;
   pm_ptr->media_fmt.endianness = extn_ptr->endianness;

   return CAPI_EOK;
}

capi_err_t pull_push_mode_check_buf_size(pull_push_mode_t *pm_ptr, pm_media_fmt_t *media_fmt_ptr)
{
   uint32_t unit_size = (media_fmt_ptr->bits_per_sample >> 3) * media_fmt_ptr->num_channels;
   //if media format is not received then unit_size will be zero.
   //if shared buffer info is not received that buffer size will be zero.
   if ((0 != unit_size) && (0 != pm_ptr->shared_circ_buf_size) && (pm_ptr->shared_circ_buf_size % unit_size))
   {
      AR_MSG(DBG_ERROR_PRIO,
             "CAPI PM: Circular buffer size %lu is not a multiple of unit size %lu ",
             pm_ptr->shared_circ_buf_size,
             unit_size);
      pm_ptr->is_disabled = TRUE;
      return CAPI_EFAILED;
   }

   return CAPI_EOK;
}

capi_err_t pull_push_mode_set_inp_media_fmt(pull_push_mode_t *pm_ptr,
                                            media_format_t *  media_fmt_header,
                                            pm_media_fmt_t *  dst_media_fmt_ptr)
{
   payload_media_fmt_pcm_t *media_fmt = (payload_media_fmt_pcm_t *)(media_fmt_header + 1);

   if (CAPI_FAILED(capi_cmn_validate_client_pcm_media_format(media_fmt)))
   {
      AR_MSG(DBG_ERROR_PRIO, "CAPI PM: input media format has errors");
      return CAPI_EBADPARAM;
   }

   dst_media_fmt_ptr->fmt_id            = media_fmt_header->fmt_id;
   dst_media_fmt_ptr->num_channels      = media_fmt->num_channels;
   dst_media_fmt_ptr->sample_rate       = media_fmt->sample_rate;
   dst_media_fmt_ptr->Q_format          = media_fmt->q_factor;
   dst_media_fmt_ptr->bit_width         = media_fmt->bit_width;
   dst_media_fmt_ptr->bits_per_sample   = media_fmt->bits_per_sample;
   dst_media_fmt_ptr->data_interleaving = pm_ptr->media_fmt.data_interleaving;
   if (CAPI_FAILED(pull_push_mode_check_buf_size(pm_ptr, dst_media_fmt_ptr)))
   {
      return CAPI_EFAILED;
   }
   dst_media_fmt_ptr->endianness = media_fmt->endianness;
   dst_media_fmt_ptr->alignment  = media_fmt->alignment;

   uint8_t *channel_mapping = (uint8_t *)(media_fmt + 1);

   memscpy(dst_media_fmt_ptr->channel_map,
           sizeof(dst_media_fmt_ptr->channel_map),
		   channel_mapping,
           media_fmt->num_channels * sizeof(uint8_t));

   return CAPI_EOK;
}

bool_t pull_push_check_media_fmt_validity(pull_push_mode_t *pm_ptr)
{
   pm_media_fmt_t *media_fmt = &pm_ptr->media_fmt;
   if (((0 == media_fmt->num_channels) || (CAPI_MAX_CHANNELS_V2 < media_fmt->num_channels)))
   {
      AR_MSG(DBG_ERROR_PRIO, "CAPI PM: Check media fmt validity Fail, num_channels %d. Max channels: %lu", media_fmt->num_channels, CAPI_MAX_CHANNELS_V2);
      return FALSE;
   }

   if ((0 == media_fmt->sample_rate) || (SAMPLE_RATE_384K < media_fmt->sample_rate))
   {
      AR_MSG(DBG_ERROR_PRIO, "CAPI PM: Check media fmt validity Fail sample rate %d", media_fmt->sample_rate);
      return FALSE;
   }

   if (((16 != media_fmt->bit_width) && (24 != media_fmt->bit_width) && (32 != media_fmt->bit_width)))
   {
      AR_MSG(DBG_ERROR_PRIO, "CAPI PM: Check media fmt validity Fail bits_per_sample %d", media_fmt->bit_width);
      return FALSE;
   }

   if ((PCM_LITTLE_ENDIAN != media_fmt->endianness) && (PCM_BIG_ENDIAN != media_fmt->endianness))
   {
      AR_MSG(DBG_ERROR_PRIO, "CAPI PM: Check media fmt validity Fail endianness %d", media_fmt->endianness);
      return FALSE;
   }

   if ((16 != media_fmt->bits_per_sample) && (24 != media_fmt->bits_per_sample) && (32 != media_fmt->bits_per_sample))
   {
      AR_MSG(DBG_ERROR_PRIO,
             "CAPI PM: Check media fmt validity Fail sample_word_size %d",
             media_fmt->bits_per_sample);
      return FALSE;
   }

   if ((PCM_LSB_ALIGNED != media_fmt->alignment) && (PCM_MSB_ALIGNED != media_fmt->alignment))
   {
      AR_MSG(DBG_ERROR_PRIO, "CAPI PM: Check media fmt validity Fail alignment  %d ", media_fmt->alignment);
      return FALSE;
   }
   AR_MSG(DBG_HIGH_PRIO, "CAPI PM: Check media fmt validity Pass");
   return TRUE;
}

void pull_push_mode_deinit(pull_push_mode_t *pm_ptr)
{
   if (NULL != pm_ptr)
   {
      // Release the shm by decrementing the ref count

      if(0 != pm_ptr->mem_map_client)
      {
         posal_memorymap_shm_decr_refcount(pm_ptr->mem_map_client, pm_ptr->pos_buf_mem_map_handle);
         posal_memorymap_shm_decr_refcount(pm_ptr->mem_map_client, pm_ptr->circ_buf_mem_map_handle);
         pm_ptr->pos_buf_mem_map_handle  = 0;
         pm_ptr->circ_buf_mem_map_handle = 0;
         pm_ptr->mem_map_client          = 0;
      }

      if (pm_ptr->num_water_mark_levels > 0)
      {
         posal_memory_free(pm_ptr->water_mark_levels_ptr);
      }
   }
}

/**
 * copies 1 ms or 5 ms worth of data into first module internal buffer.
 */
capi_err_t pull_mode_read_input(capi_pm_t *capi_ptr, capi_buf_t *module_buf_ptr)
{
   capi_err_t                               result      = CAPI_EOK;
   pull_push_mode_t *                       me_ptr      = &(capi_ptr->pull_push_mode_info);
   sh_mem_pull_push_mode_position_buffer_t *pos_buf_ptr = me_ptr->shared_pos_buf_ptr;

   if (NULL == me_ptr->shared_circ_buf_start_ptr)
   {
      AR_MSG(DBG_ERROR_PRIO, "pull_mode_read_input: circ buf is not set-up");
      return CAPI_EFAILED;
   }

   if (NULL == pos_buf_ptr)
   {
      AR_MSG(DBG_HIGH_PRIO, "pull_mode_read_input: NULL position buffer !");
      return CAPI_EFAILED;
   }

   int8_t *      read_ptr;
   uint32_t      rem_lin_size; // remaining linear size.
   uint32_t      read_index = pos_buf_ptr->index, temp_rd_ind;
   capi_buf_t input_scratch_buf;
   uint32_t bytes_to_copy_now = 0;
   uint32_t num_bytes_copied = 0;
   module_buf_ptr[0].actual_data_len = 0;
   uint32_t num_channels = me_ptr->media_fmt.num_channels;

   uint32_t word_size                   = me_ptr->media_fmt.bits_per_sample;
   uint32_t empty_inp_bytes_per_buf = module_buf_ptr[0].max_data_len;
   uint32_t total_copy_size = (CAPI_INTERLEAVED == me_ptr->media_fmt.data_interleaving)
                                 ? empty_inp_bytes_per_buf
                                 : empty_inp_bytes_per_buf * num_channels;

   while (1)
   {
      read_ptr          = (int8_t *)(me_ptr->shared_circ_buf_start_ptr + read_index);
      temp_rd_ind       = read_index;
      rem_lin_size      = me_ptr->shared_circ_buf_size - read_index;
      bytes_to_copy_now = MIN((total_copy_size - num_bytes_copied), rem_lin_size);

      if (CAPI_FAILED(result = posal_cache_invalidate_v2(&read_ptr, bytes_to_copy_now)))
      {
         AR_MSG(DBG_ERROR_PRIO, "pull_mode_read_input: Failure cache invalidate.");
         return result;
      }

      if (CAPI_INTERLEAVED == me_ptr->media_fmt.data_interleaving)
      {
         module_buf_ptr[0].actual_data_len +=
            memscpy((int8_t *)(module_buf_ptr[0].data_ptr + module_buf_ptr[0].actual_data_len),
                    module_buf_ptr[0].max_data_len - module_buf_ptr[0].actual_data_len,
                    (int8_t *)read_ptr,
                    bytes_to_copy_now);
      }
      else
      {
         input_scratch_buf.max_data_len    = bytes_to_copy_now;
         input_scratch_buf.actual_data_len = bytes_to_copy_now;
         input_scratch_buf.data_ptr        = read_ptr;

         for (int32_t i = 0; i < num_channels; i++)
         {
            me_ptr->scratch_buf[i].data_ptr     = module_buf_ptr[i].data_ptr + module_buf_ptr[i].actual_data_len;
            me_ptr->scratch_buf[i].max_data_len = module_buf_ptr[i].max_data_len - module_buf_ptr[i].actual_data_len;
         }

         if (CAPI_FAILED(result = spf_intlv_to_deintlv(&input_scratch_buf,
                                                       me_ptr->scratch_buf,
                                                       num_channels,
                                                       word_size)))
         {
            AR_MSG(DBG_ERROR_PRIO, "pull_mode_read_input: Int - De-Int conversion failed");
            return result;
         }

         for (int32_t i = 0; i < num_channels; i++)
         {
            module_buf_ptr[i].actual_data_len += me_ptr->scratch_buf[i].actual_data_len;
         }
         empty_inp_bytes_per_buf -= me_ptr->scratch_buf[0].actual_data_len;
      }

      num_bytes_copied += bytes_to_copy_now;
      read_index += bytes_to_copy_now;

      pull_push_mode_check_send_watermark_event(capi_ptr, temp_rd_ind, read_index);
      if (read_index == me_ptr->shared_circ_buf_size)
      {
         read_index = 0;
      }

      if (num_bytes_copied == total_copy_size)
      {
         break;
      }
   }
   // Update the position buffer
   uint64_t time               = posal_timer_get_time();
   uint32_t prev_frame_counter = pos_buf_ptr->frame_counter;
   /** below is a critical section which is executed at high prio so that if the client is also trying to read
    *  we will try to complete is ASAP & they don't have to wait for longer
    */
   posal_thread_prio_t     priority = posal_thread_prio_get();

   posal_thread_set_prio(capi_ptr->pull_push_mode_info.ist_priority);
   pos_buf_ptr->frame_counter    = 0;
   pos_buf_ptr->index            = read_index;
   pos_buf_ptr->timestamp_us_lsw = (uint32_t)time;
   pos_buf_ptr->timestamp_us_msw = (uint32_t)(time >> 32);
   pos_buf_ptr->frame_counter    = prev_frame_counter + 1;
   posal_thread_set_prio(priority);
   return result;
}

capi_err_t push_mode_write_output(capi_pm_t *capi_ptr, capi_buf_t *module_buf_ptr, uint64_t timestamp, bool_t received_eos_marker)
{
   capi_err_t     result = CAPI_EOK;
   uint32_t          rem_lin_size;
   int8_t *          write_ptr = NULL, *read_ptr = NULL;
   uint32_t          bytes_to_copy, bytes_copied = 0, bytes_copied_per_channel = 0, bytes_copied_per_channel_now = 0;
   pull_push_mode_t *me_ptr                             = &(capi_ptr->pull_push_mode_info);
   sh_mem_pull_push_mode_position_buffer_t *pos_buf_ptr = me_ptr->shared_pos_buf_ptr;
   capi_buf_t                            output_scratch_buf;

   if (NULL == me_ptr->shared_circ_buf_start_ptr)
   {
      AR_MSG(DBG_ERROR_PRIO, "push_mode_write_output: circ buf is not set-up");
      return CAPI_EFAILED;
   }

   if (NULL == pos_buf_ptr)
   {
      AR_MSG(DBG_HIGH_PRIO, "push_mode_write_output: NULL position buffer !");
      return CAPI_EFAILED;
   }

   uint32_t write_index     = pos_buf_ptr->index, temp_wr_ind;
   uint32_t word_size       = me_ptr->media_fmt.bits_per_sample;
   uint32_t input_copy_size = 0;

   while (module_buf_ptr[0].actual_data_len > 0)
   {
      write_ptr = (int8_t *)(me_ptr->shared_circ_buf_start_ptr + write_index);

      rem_lin_size = me_ptr->shared_circ_buf_size - write_index;

      if (CAPI_INTERLEAVED == me_ptr->media_fmt.data_interleaving)
      {

         bytes_to_copy = MIN(module_buf_ptr[0].actual_data_len, rem_lin_size);
         read_ptr = module_buf_ptr[0].data_ptr + bytes_copied;

         memscpy(write_ptr, bytes_to_copy, read_ptr, bytes_to_copy);
         posal_cache_flush_v2(&write_ptr, bytes_to_copy);

         module_buf_ptr[0].actual_data_len -= bytes_to_copy;
         bytes_copied += bytes_to_copy;
      }
      else
      {
         input_copy_size = (module_buf_ptr[0].actual_data_len * me_ptr->media_fmt.num_channels);

         bytes_to_copy = MIN(input_copy_size, rem_lin_size);

         for (int32_t i = 0; i < me_ptr->media_fmt.num_channels; i++)
         {
            me_ptr->scratch_buf[i].data_ptr        = module_buf_ptr[i].data_ptr + bytes_copied_per_channel;
            me_ptr->scratch_buf[i].actual_data_len = module_buf_ptr[i].actual_data_len;
         }

         output_scratch_buf.max_data_len    = rem_lin_size;
         output_scratch_buf.actual_data_len = 0;
         output_scratch_buf.data_ptr        = write_ptr;

         if (CAPI_FAILED(result = spf_deintlv_to_intlv(me_ptr->scratch_buf,
                                                         &output_scratch_buf,
                                                         me_ptr->media_fmt.num_channels,
                                                         word_size)))
         {
            AR_MSG(DBG_ERROR_PRIO, "push_mode_write_output: De-Int - Int conversion failed");
            return result;
         }
         posal_cache_flush_v2(&write_ptr, bytes_to_copy);

         bytes_copied_per_channel_now = bytes_to_copy / me_ptr->media_fmt.num_channels;

         for (int32_t i = 0; i < me_ptr->media_fmt.num_channels; i++)
         {
            module_buf_ptr[i].actual_data_len -= bytes_copied_per_channel_now;
         }

         bytes_copied_per_channel += bytes_copied_per_channel_now;
      }
      temp_wr_ind = write_index;
      write_index += bytes_to_copy;
      pull_push_mode_check_send_watermark_event(capi_ptr, temp_wr_ind, write_index);
      if (write_index >= me_ptr->shared_circ_buf_size)
      {
         write_index = 0;
      }
   }

   // in CAPI we need to report number of bytes consumed (when we return)
   if (CAPI_INTERLEAVED == me_ptr->media_fmt.data_interleaving)
   {
      module_buf_ptr[0].actual_data_len = bytes_copied;
   }
   else
   {
      for (int32_t i = 0; i < me_ptr->media_fmt.num_channels; i++)
      {
         module_buf_ptr[i].actual_data_len = bytes_copied_per_channel;
      }
   }

   // Update the position buffer
   uint32_t prev_frame_counter = pos_buf_ptr->frame_counter;

   /** below is a critical section which is executed at high prio so that if the client is also trying to read
    *  we will try to complete is ASAP & they don't have to wait for longer
    */
   posal_thread_prio_t     priority = posal_thread_prio_get();

   posal_thread_set_prio(capi_ptr->pull_push_mode_info.ist_priority);
   pos_buf_ptr->frame_counter    = 0;
   pos_buf_ptr->index            = write_index;
   pos_buf_ptr->timestamp_us_lsw = (uint32_t)timestamp;
   pos_buf_ptr->timestamp_us_msw = (uint32_t)(timestamp >> 32);
   pos_buf_ptr->frame_counter    = prev_frame_counter + 1;

   // raise EOS marker event
   if(received_eos_marker)
   {
      event_sh_mem_push_mode_eos_marker_t payload;
	  payload.index            = write_index;
	  payload.timestamp_us_lsw = (uint32_t)timestamp;
	  payload.timestamp_us_msw = (uint32_t)(timestamp >> 32);
      result = capi_pm_raise_event_to_clients(capi_ptr, EVENT_ID_SH_MEM_PUSH_MODE_EOS_MARKER, &payload, sizeof(event_sh_mem_push_mode_eos_marker_t));
      if (CAPI_FAILED(result))
      {
         AR_MSG(DBG_ERROR_PRIO, "pm_check_send_eos_marker_event: Failed to send water mark event!");
      }
      else
      {
         AR_MSG(DBG_HIGH_PRIO,
               "pm_check_send_eos_marker_event: Sent EOS Marker event to the client wrtie_index %lu, timestamp_us_lsw %lu, timestamp_us_msw %lu",
               write_index,
               (uint32_t)timestamp,
               (uint32_t)(timestamp >> 32));
      }
   }
   posal_thread_set_prio(priority);

   return result;
}

static capi_err_t pull_push_mode_check_send_watermark_event(capi_pm_t *capi_ptr,
                                                               uint32_t      startLevel,
                                                               uint32_t      endLevel)
{
   capi_err_t     result = CAPI_EOK;
   pull_push_mode_t *pm_ptr = &(capi_ptr->pull_push_mode_info);

   if (pm_ptr->num_water_mark_levels == 0)
   {
      return CAPI_EOK;
   }

   uint32_t level;

   for (uint32_t i = 0; i < pm_ptr->num_water_mark_levels; i++)
   {
      level = pm_ptr->water_mark_levels_ptr[i].watermark_level_bytes;

      /** If a certain level is within start and end, then we are crossing the level, hence raise an event*/
      if ((level > startLevel) && (level <= endLevel))
      {
         // form the payload for the event
         event_sh_mem_pull_push_mode_watermark_level_t water_mark_event;
         water_mark_event.watermark_level_bytes = level;

         result = capi_pm_raise_event_to_clients(capi_ptr, EVENT_ID_SH_MEM_PULL_PUSH_MODE_WATERMARK, &water_mark_event, sizeof(event_sh_mem_pull_push_mode_watermark_level_t));

         if (CAPI_FAILED(result))
         {
            AR_MSG(DBG_ERROR_PRIO, "pm_check_send_watermark_event: Failed to send water mark event!");
         }
         else
         {
            AR_MSG(DBG_LOW_PRIO,
                   "pm_check_send_watermark_event: Sent Water Mark event to the client start %lu, level %lu, end %lu",
                   startLevel,
                   level,
                   endLevel);
         }
      }
   }
   return result;
}
