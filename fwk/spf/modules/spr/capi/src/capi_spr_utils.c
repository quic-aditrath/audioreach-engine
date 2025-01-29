/**
 *   \file capi_spr_utils.c
 *   \brief
 *        This file contains CAPI implementation of Splitter Renderer Module utilities
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#include "capi_spr_i.h"

/*==============================================================================
   Local Definitions
==============================================================================*/
#define DO_NOT_REINITIALIZE (FALSE)

/*==============================================================================
   Local Function forward declaration
==============================================================================*/
static capi_err_t spr_does_strm_reader_have_data(capi_spr_t *me_ptr, bool_t *has_data_ptr);
static capi_err_t capi_spr_check_reinit_ports(capi_spr_t *         me_ptr,
                                              capi_media_fmt_v2_t *media_fmt_ptr,
                                              bool_t               reinit_spr_ports,
                                              bool_t               check_cache_mf);
static bool_t spr_can_reinit_with_new_mf(capi_spr_t *me_ptr);
static void capi_spr_check_raise_dynamic_inplace(capi_spr_t *me_ptr);
static capi_err_t capi_spr_update_input_output_port_tp_for_spr_time_mode_update(capi_spr_t *me_ptr, bool_t is_input);
static bool capi_spr_check_is_ds_rt(capi_spr_t *me_ptr);
static capi_err_t capi_spr_change_trigger_policy_1(capi_spr_t *me_ptr);
static capi_err_t capi_spr_raise_is_signal_triggered_change(capi_spr_t *me_ptr);
static capi_err_t capi_spr_raise_upstream_downstream_rt_event(capi_spr_t *me_ptr,
                                                              bool_t      is_input,
                                                              bool_t      is_rt,
                                                              uint32_t    port_index);

/*==============================================================================
   Function Implementation
==============================================================================*/

/*------------------------------------------------------------------------------
  Function name: capi_spr_create_port_structures
  Creates memory for the module port information structures
    Note : since this is an init time function call, no SPR_MSG calls are used
 * ------------------------------------------------------------------------------*/
capi_err_t capi_spr_create_port_structures(capi_spr_t *me_ptr)
{

   if (NULL != me_ptr->in_port_info_arr || NULL != me_ptr->out_port_info_arr)
   {
      AR_MSG(DBG_ERROR_PRIO, "capi_spr: Port structures are already allocated.");
      return CAPI_EFAILED;
   }

   // TODO: Is this always set after setting the heap ID ?
   // Allocate memory for the input port structures.
   uint32_t alloc_size      = me_ptr->max_input_ports * sizeof(spr_input_port_info_t);
   me_ptr->in_port_info_arr = (spr_input_port_info_t *)posal_memory_malloc(alloc_size, me_ptr->heap_id);
   if (NULL == me_ptr->in_port_info_arr)
   {
      AR_MSG(DBG_ERROR_PRIO, "capi_spr: Couldn't allocate memory for input port info.");
      return CAPI_ENOMEMORY;
   }
   memset(me_ptr->in_port_info_arr, 0, alloc_size);

   // Allocate memory for output port info structures
   alloc_size                = me_ptr->max_output_ports * sizeof(spr_output_port_info_t);
   me_ptr->out_port_info_arr = (spr_output_port_info_t *)posal_memory_malloc(alloc_size, me_ptr->heap_id);
   if (NULL == me_ptr->out_port_info_arr)
   {
      AR_MSG(DBG_ERROR_PRIO, "capi_spr: Couldn't allocate memory for output port info.");
      return CAPI_ENOMEMORY;
   }
   memset(me_ptr->out_port_info_arr, 0, alloc_size);

   return CAPI_EOK;
}

/*------------------------------------------------------------------------------
  Function name: capi_check_and_init_input_port
   Check if media format and port config is received and then allocate stream
   writer for the particular index.
 * ------------------------------------------------------------------------------*/
capi_err_t capi_check_and_init_input_port(capi_spr_t *me_ptr, uint32_t arr_index, bool_t need_to_reinitialize)
{
   // if need to reinit flag is set, destory the existing handle and intialize again.
   if (need_to_reinitialize && me_ptr->in_port_info_arr[arr_index].strm_writer_ptr)
   {
      SPR_MSG(me_ptr->miid, DBG_MED_PRIO, "Port is being reset. input port %lu", arr_index);
      spr_stream_writer_destroy(me_ptr->drv_ptr, &me_ptr->in_port_info_arr[arr_index].strm_writer_ptr);
   }

   if ((NULL == me_ptr->in_port_info_arr[arr_index].strm_writer_ptr) && (TRUE == me_ptr->flags.is_input_media_fmt_set) &&
       (DATA_PORT_STATE_CLOSED != me_ptr->in_port_info_arr[arr_index].port_state))
   {
      // Convert us to bytes.
      uint32_t pre_buffering_delay_in_bytes = 0;

      // Create unique channel id for each channel using input port index and channel map.
      uint32_t channel_ids[MAX_CHANNELS_PER_STREAM];
      for (uint32_t i = 0; i < me_ptr->operating_mf.format.num_channels; i++)
      {
         channel_ids[i] = me_ptr->operating_mf.channel_type[i];
      }

      uint32_t size = capi_cmn_us_to_bytes_per_ch(me_ptr->frame_dur_us,
                                                  me_ptr->operating_mf.format.sampling_rate,
                                                  me_ptr->operating_mf.format.bits_per_sample);
      ar_result_t result = spr_stream_writer_create(me_ptr->drv_ptr,
                                                    size, // writers base buffer size
                                                    pre_buffering_delay_in_bytes,
                                                    me_ptr->operating_mf.format.num_channels,
                                                    channel_ids,
                                                    &me_ptr->in_port_info_arr[arr_index].strm_writer_ptr);
      if (AR_EOK != result)
      {
         SPR_MSG(me_ptr->miid,
                 DBG_ERROR_PRIO,
                 "Input port can't be initiated. port_index =%d, result %d",
                 arr_index,
                 result);
         return CAPI_EFAILED;
      }
      else
      {
         SPR_MSG(me_ptr->miid,
                 DBG_MED_PRIO,
                 "Input port idx[%lu] stream writer initialized successfully.size%lu",
                 arr_index,
                 size);
      }
   }
   else
   {
      SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Input port not ready to initialize. port_index =%d.", arr_index);
   }

   return CAPI_EOK;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_check_and_init_output_port
   Check if the port config is received and then allocate stream reader
   for the particular index.
 * ------------------------------------------------------------------------------*/
capi_err_t capi_spr_check_and_init_output_port(capi_spr_t *me_ptr, uint32_t arr_index, bool_t need_to_reinitialize)
{

   // if need to reinit flag is set, destory the existing handle and initialize again.
   if (need_to_reinitialize && me_ptr->out_port_info_arr[arr_index].strm_reader_ptr)
   {
      SPR_MSG(me_ptr->miid, DBG_MED_PRIO, "Port is being reset. outport_index %lu", arr_index);
      spr_stream_reader_destroy(me_ptr->drv_ptr, &me_ptr->out_port_info_arr[arr_index].strm_reader_ptr);
   }

   if ((NULL == me_ptr->out_port_info_arr[arr_index].strm_reader_ptr) && me_ptr->flags.is_input_media_fmt_set)
   {
      // Convert downstream setup delay from us to bytes.
      uint32_t downstream_setup_duration_in_bytes = 0;

      // Get the unique channel id for each channel using input port index and channel map.
      uint32_t channel_ids[MAX_CHANNELS_PER_STREAM];
      for (uint32_t i = 0; i < me_ptr->operating_mf.format.num_channels; i++)
      {
         // Set LSB 2 bytes to input channel type.
         channel_ids[i] = me_ptr->operating_mf.channel_type[i];
      }

      ar_result_t result = spr_stream_reader_create(me_ptr->drv_ptr,
                                                    downstream_setup_duration_in_bytes,
                                                    me_ptr->operating_mf.format.num_channels,
                                                    (uint32_t *)channel_ids,
                                                    &me_ptr->out_port_info_arr[arr_index].strm_reader_ptr);
      if (AR_EOK != result)
      {
         SPR_MSG(me_ptr->miid,
                 DBG_ERROR_PRIO,
                 "Output port can't be initiated. port_index =%d, result %d",
                 me_ptr->out_port_info_arr[arr_index].port_index,
                 result);
         return CAPI_EFAILED;
      }
      else
      {
         SPR_MSG(me_ptr->miid,
                 DBG_LOW_PRIO,
                 "Output port initiated successfully. port_index =%d, result %d",
                 me_ptr->out_port_info_arr[arr_index].port_index,
                 result);
      }
   }
   else
   {
      SPR_MSG(me_ptr->miid,
              DBG_HIGH_PRIO,
              "Output port not ready to initialize. port_index =%d.",
              me_ptr->out_port_info_arr[arr_index].port_index);
   }

   return CAPI_EOK;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_destroy_input_port
   Destroy the stream writer associated with the input port & reset the memory
   contents
 * ------------------------------------------------------------------------------*/
capi_err_t capi_spr_destroy_input_port(capi_spr_t *me_ptr, uint32_t arr_index)
{
   // Destroy the stream writer for the input port
   if (me_ptr->in_port_info_arr[arr_index].strm_writer_ptr)
   {
      spr_stream_writer_destroy(me_ptr->drv_ptr, &me_ptr->in_port_info_arr[arr_index].strm_writer_ptr);
   }

   bool_t DO_NOT_APPLY_TAIL_MF = FALSE;
   capi_spr_destroy_cached_mf_list(me_ptr, DO_NOT_APPLY_TAIL_MF);

   // memset the port structure
   memset(&me_ptr->in_port_info_arr[arr_index], 0, sizeof(spr_input_port_info_t));

   return CAPI_EOK;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_destroy_output_port
   Destroy the stream writer associated with the output port & reset the memory
   contents & path delay
 * ------------------------------------------------------------------------------*/
capi_err_t capi_spr_destroy_output_port(capi_spr_t *me_ptr, uint32_t arr_index, bool_t need_to_partial_destroy)
{
   uint32_t port_id      = 0;
   uint32_t ctrl_port_id = 0;

   spr_reset_path_delay(me_ptr, &me_ptr->out_port_info_arr[arr_index]);

   if (me_ptr->out_port_info_arr[arr_index].strm_reader_ptr)
   {
      spr_stream_reader_destroy(me_ptr->drv_ptr, &me_ptr->out_port_info_arr[arr_index].strm_reader_ptr);
   }

   port_id      = me_ptr->out_port_info_arr[arr_index].port_id;
   ctrl_port_id = me_ptr->out_port_info_arr[arr_index].ctrl_port_id;
   // memset the port structure
   memset(&me_ptr->out_port_info_arr[arr_index], 0, sizeof(spr_output_port_info_t));

   /*If output port was mapped to control port and need to partial destroy as part of o/p port close, retain port id*/
   if (TRUE == need_to_partial_destroy && (0 != ctrl_port_id))
   {
      me_ptr->out_port_info_arr[arr_index].port_id      = port_id;
      me_ptr->out_port_info_arr[arr_index].ctrl_port_id = ctrl_port_id;

      SPR_MSG(me_ptr->miid,
              DBG_MED_PRIO,
              "capi_spr_destroy_output_port: Retained port_id 0x%lX and control port ID 0x%lX for arr_index : %d",
              port_id,
              ctrl_port_id,
              arr_index);
   }

   return CAPI_EOK;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_check_and_raise_output_media_format_event
   Raise capi media format event when the appropriate port states & configuration
   is received
 * ------------------------------------------------------------------------------*/
capi_err_t capi_spr_check_and_raise_output_media_format_event(capi_spr_t *me_ptr, uint32_t arr_index)
{
   capi_err_t result = CAPI_EOK;

   if (me_ptr->flags.is_input_media_fmt_set && (DATA_PORT_STATE_CLOSED != me_ptr->out_port_info_arr[arr_index].port_state))
   {
      result |= capi_cmn_output_media_fmt_event_v2(&me_ptr->event_cb_info,
                                                   &me_ptr->operating_mf,
                                                   FALSE,
                                                   me_ptr->out_port_info_arr[arr_index].port_index);
#ifdef DEBUG_SPR_MODULE
      SPR_MSG(me_ptr->miid,
              DBG_MED_PRIO,
              "raised output media format for port_index %d as follows :",
              me_ptr->out_port_info_arr[arr_index].port_index);

      capi_spr_print_media_fmt(me_ptr, &me_ptr->operating_mf);

#endif
   }

   return result;
}

/*------------------------------------------------------------------------------
  Function name: spr_get_arr_index_from_port_index
   Given port index, fetch the corresponding array index to be used for the internal
   book keeping port structures.
 * ------------------------------------------------------------------------------*/
uint32_t spr_get_arr_index_from_port_index(capi_spr_t *me_ptr, uint32_t port_index, bool_t is_input)
{
   if (!is_input)
   {
      for (uint32_t arr_index = 0; arr_index < me_ptr->max_output_ports; arr_index++)
      {
         if (DATA_PORT_STATE_CLOSED == me_ptr->out_port_info_arr[arr_index].port_state)
         {
            continue;
         }
         if (port_index == me_ptr->out_port_info_arr[arr_index].port_index)
         {
            return arr_index;
         }
      }
   }
   else
   {
      for (uint32_t arr_index = 0; arr_index < me_ptr->max_input_ports; arr_index++)
      {
         if (DATA_PORT_STATE_CLOSED == me_ptr->in_port_info_arr[arr_index].port_state)
         {
            continue;
         }
         if (port_index == me_ptr->in_port_info_arr[arr_index].port_index)
         {
            return arr_index;
         }
      }
   }

   SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Port index = %lu to index mapping not found.", port_index);
   return UMAX_32;
}

/*------------------------------------------------------------------------------
  Function name: spr_get_arr_index_from_port_id
   Given port id, fetch the corresponding array index to be used for the internal
   book keeping port structures.
 * ------------------------------------------------------------------------------*/
uint32_t spr_get_arr_index_from_port_id(capi_spr_t *me_ptr, uint32_t port_id)
{
   bool_t   is_input_port       = (port_id % 2) ? FALSE : TRUE;
   uint32_t available_arr_index = 0;

   if (!is_input_port)
   {
      available_arr_index = me_ptr->max_output_ports;
      for (uint32_t arr_index = 0; arr_index < me_ptr->max_output_ports; arr_index++)
      {
         if (port_id == me_ptr->out_port_info_arr[arr_index].port_id)
         {
            return arr_index;
         }
         else if (0 == me_ptr->out_port_info_arr[arr_index].port_id) // unused port structure
         {
            available_arr_index = arr_index;
         }
      }

      if (available_arr_index != me_ptr->max_output_ports)
      {
         me_ptr->out_port_info_arr[available_arr_index].port_id = port_id;
         return available_arr_index;
      }
   }
   else
   {
      available_arr_index = me_ptr->max_input_ports;
      for (uint32_t arr_index = 0; arr_index < me_ptr->max_input_ports; arr_index++)
      {
         if (port_id == me_ptr->in_port_info_arr[arr_index].port_id)
         {
            return arr_index;
         }
         else if (0 == me_ptr->in_port_info_arr[arr_index].port_id) // unused port structure
         {
            available_arr_index = arr_index;
         }
      }

      if (available_arr_index != me_ptr->max_input_ports)
      {
         me_ptr->in_port_info_arr[available_arr_index].port_id = port_id;
         return available_arr_index;
      }
   }

   SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Port ID = %lu to index mapping not found.", port_id);
   return UMAX_32;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_data_port_op_handler
   Utility to handle INTF_EXTN_PARAM_ID_DATA_PORT_OPERATION on the input & output
   ports
 * ------------------------------------------------------------------------------*/
capi_err_t capi_spr_data_port_op_handler(capi_spr_t *me_ptr, capi_buf_t *params_ptr)
{
   capi_err_t result               = CAPI_EOK;

   if (NULL == params_ptr->data_ptr)
   {
      SPR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "Set param id 0x%lx, received null buffer",
              INTF_EXTN_PARAM_ID_DATA_PORT_OPERATION);
      return CAPI_EBADPARAM;
   }
   if (params_ptr->actual_data_len < sizeof(intf_extn_data_port_operation_t))
   {
      SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Invalid payload size for port operation %d", params_ptr->actual_data_len);
      return CAPI_ENEEDMORE;
   }

   intf_extn_data_port_operation_t *data_ptr = (intf_extn_data_port_operation_t *)(params_ptr->data_ptr);
   if (params_ptr->actual_data_len <
       sizeof(intf_extn_data_port_operation_t) + (data_ptr->num_ports * sizeof(intf_extn_data_port_id_idx_map_t)))
   {
      SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Invalid payload size for port operation %d", params_ptr->actual_data_len);
      return CAPI_ENEEDMORE;
   }

   if (data_ptr->is_input_port && (data_ptr->num_ports > me_ptr->max_input_ports))
   {
      SPR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "Invalid input ports. num_ports =%d, max_input_ports = %d",
              data_ptr->num_ports,
              me_ptr->max_input_ports);
      return CAPI_EBADPARAM;
   }

   if (!data_ptr->is_input_port && (data_ptr->num_ports > me_ptr->max_output_ports))
   {
      SPR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "Invalid output ports. num_ports =%d, max_output_ports = %d",
              data_ptr->num_ports,
              me_ptr->max_output_ports);
      return CAPI_EBADPARAM;
   }

   for (uint32_t iter = 0; iter < data_ptr->num_ports; iter++)
   {
      uint32_t port_id    = data_ptr->id_idx[iter].port_id;
      uint32_t port_index = data_ptr->id_idx[iter].port_index;
      uint32_t arr_index  = UMAX_32;

      SPR_MSG(me_ptr->miid,
              DBG_HIGH_PRIO,
              "Port operation 0x%x performed on port_index= %lu, port_id= %lu is_input_port= %d ",
              data_ptr->opcode,
              port_index,
              data_ptr->id_idx[iter].port_id,
              data_ptr->is_input_port);

      if ((data_ptr->is_input_port && (port_index >= me_ptr->max_input_ports)) ||
          (!data_ptr->is_input_port && (port_index >= me_ptr->max_output_ports)))
      {
         SPR_MSG(me_ptr->miid,
                 DBG_ERROR_PRIO,
                 "Bad parameter in id-idx map on port %lu, port_index = %lu, max in ports = %d, "
                 "max out ports = %d ",
                 iter,
                 port_index,
                 me_ptr->max_input_ports,
                 me_ptr->max_output_ports);
         return CAPI_EBADPARAM;
      }

      // Get port structures array index from port ID.
      arr_index = spr_get_arr_index_from_port_id(me_ptr, port_id);
      if (arr_index >= me_ptr->max_output_ports)
      {
         result |= CAPI_EBADPARAM;
         break;
      }

      switch (data_ptr->opcode)
      {
         case INTF_EXTN_DATA_PORT_OPEN:
         {
            if (data_ptr->is_input_port)
            {
               if (DATA_PORT_STATE_CLOSED == me_ptr->in_port_info_arr[arr_index].port_state)
               {
                  // Update the port state in the port info structure.
                  me_ptr->in_port_info_arr[arr_index].port_state = DATA_PORT_STATE_OPENED;

                  // Cache the input port ID value.
                  me_ptr->in_port_info_arr[arr_index].port_index = port_index;
                  me_ptr->in_port_info_arr[arr_index].port_id    = port_id;

                  result |= capi_check_and_init_input_port(me_ptr, arr_index, DO_NOT_REINITIALIZE);
               }
            }
            else
            {
               if (DATA_PORT_STATE_CLOSED == me_ptr->out_port_info_arr[arr_index].port_state)
               {
                  // Update the port state in the port info structure.
                  me_ptr->out_port_info_arr[arr_index].port_state = DATA_PORT_STATE_OPENED;

                  // Cache the output port ID value.
                  me_ptr->out_port_info_arr[arr_index].port_index = port_index;
                  me_ptr->out_port_info_arr[arr_index].port_id    = port_id;

                  result |= capi_spr_set_up_output(me_ptr, port_index, DO_NOT_REINITIALIZE);

                  // output ports always are FWK_EXTN_PORT_NON_TRIGGER_BLOCKED
               }
            }
            result |= capi_spr_raise_upstream_downstream_rt_event(me_ptr, data_ptr->is_input_port /*is_input*/, TRUE /*is_rt*/, port_index);
            break;
         }
         case INTF_EXTN_DATA_PORT_START:
         {
            if (data_ptr->is_input_port)
            {
               if (me_ptr->in_port_info_arr[arr_index].port_state &
                   (DATA_PORT_STATE_OPENED | DATA_PORT_STATE_STOPPED | DATA_PORT_STATE_SUSPENDED))
               {
                  // Update the port state in the port info structure.
                  me_ptr->in_port_info_arr[arr_index].port_state = DATA_PORT_STATE_STARTED;

                  me_ptr->num_started_input_ports++;
                  //result |=
                  //   capi_spr_raise_upstream_downstream_rt_event(me_ptr, TRUE /*is_input*/, TRUE /*is_rt*/, port_index);
               }
            }
            else
            {
               if (me_ptr->out_port_info_arr[arr_index].port_state &
                   (DATA_PORT_STATE_OPENED | DATA_PORT_STATE_STOPPED | DATA_PORT_STATE_SUSPENDED))
               {
                  // if prev state is suspended do not re init
                  bool_t need_to_reinit = me_ptr->out_port_info_arr[arr_index].port_state != DATA_PORT_STATE_SUSPENDED;

                  // Update the port state in the port info structure.
                  me_ptr->out_port_info_arr[arr_index].port_state = DATA_PORT_STATE_STARTED;

                  // Update the primary output port if required
                  capi_spr_avsync_check_update_primary_output_port(me_ptr);

                  me_ptr->num_started_output_ports++;

                  result |= capi_spr_set_up_output(me_ptr, port_index, need_to_reinit /*re-initialize*/);

                  // Raise upstream realtime event from the output port
                  //result |=
                  //    capi_spr_raise_upstream_downstream_rt_event(me_ptr, FALSE /*is_input*/, TRUE /*is_rt*/, port_index);
               }
            }


            SPR_MSG(me_ptr->miid,
                    DBG_HIGH_PRIO,
                    "Calling timer enable/disable from port start counter %d, enable %d",
                    me_ptr->counter,
                    me_ptr->flags.stm_ctrl_enable);

            result |= capi_spr_check_timer_disable_update_tp(me_ptr);
            break;
         }
         case INTF_EXTN_DATA_PORT_SUSPEND:
         {
            if (data_ptr->is_input_port)
            {
               if (DATA_PORT_STATE_STARTED == me_ptr->in_port_info_arr[arr_index].port_state)
               {
                  // Update the port state in the port info structure.
                  me_ptr->in_port_info_arr[arr_index].port_state = DATA_PORT_STATE_SUSPENDED;

                  if (me_ptr->num_started_input_ports)
                  {
                     me_ptr->num_started_input_ports--;
                  }

                  // Since no input is active, reset primary output port to invalid value
                  me_ptr->primary_output_arr_idx = UMAX_32;

               }
            }
            else
            {
               if (DATA_PORT_STATE_STARTED == me_ptr->out_port_info_arr[arr_index].port_state)
               {
                  // Update the port state in the port info structure.
                  me_ptr->out_port_info_arr[arr_index].port_state = DATA_PORT_STATE_SUSPENDED;

                  if (me_ptr->num_started_output_ports)
                  {
                     me_ptr->num_started_output_ports--;
                  }

                  // Update the primary output port if required
                  capi_spr_avsync_check_update_primary_output_port(me_ptr);
               }
            }


            SPR_MSG(me_ptr->miid,
                    DBG_HIGH_PRIO,
                    "Calling timer enable/disable from port suspend, counter %d, enable %d",
                    me_ptr->counter,
                    me_ptr->flags.stm_ctrl_enable);

            result |= capi_spr_check_timer_disable_update_tp(me_ptr);
            break;
         }
         case INTF_EXTN_DATA_PORT_STOP:
         {
            if (data_ptr->is_input_port)
            {
               if (DATA_PORT_STATE_STARTED == me_ptr->in_port_info_arr[arr_index].port_state)
               {
                  // Update the port state in the port info structure.
                  me_ptr->in_port_info_arr[arr_index].port_state = DATA_PORT_STATE_STOPPED;

                  if (me_ptr->num_started_input_ports)
                  {
                     me_ptr->num_started_input_ports--;
                  }

                  // Since no input is active, reset primary output port to invalid value
                  me_ptr->primary_output_arr_idx = UMAX_32;
                  me_ptr->flags.has_rcvd_first_buf = FALSE;
                  me_ptr->flags.has_rendered_first_buf = FALSE;
                  capi_spr_avsync_reset_session_clock_params(me_ptr->avsync_ptr);

                  // Destroy hold buffer
                  capi_spr_destroy_hold_buf_list(me_ptr);

                  bool_t APPLY_TAIL_MF = TRUE;
                  capi_spr_destroy_cached_mf_list(me_ptr, APPLY_TAIL_MF);

                  me_ptr->data_trigger.is_dropping_data = FALSE;
               }
            }
            else
            {
               if (DATA_PORT_STATE_STARTED == me_ptr->out_port_info_arr[arr_index].port_state)
               {
                  // Update the port state in the port info structure.
                  me_ptr->out_port_info_arr[arr_index].port_state = DATA_PORT_STATE_STOPPED;

                  if (me_ptr->num_started_output_ports)
                  {
                     me_ptr->num_started_output_ports--;
                  }

                  // Update the primary output port if required
                  capi_spr_avsync_check_update_primary_output_port(me_ptr);

               }

               spr_stream_reader_destroy(me_ptr->drv_ptr, &me_ptr->out_port_info_arr[arr_index].strm_reader_ptr);
            }
            result |= capi_spr_check_timer_disable_update_tp(me_ptr);
            SPR_MSG(me_ptr->miid,
                    DBG_HIGH_PRIO,
                    "Calling timer enable/disable from port stop counter %d, enable %d",
                    me_ptr->counter,
                    me_ptr->flags.stm_ctrl_enable);
            break;
         }
         case INTF_EXTN_DATA_PORT_CLOSE:
         {
            if (data_ptr->is_input_port)
            {
               if (DATA_PORT_STATE_CLOSED != me_ptr->in_port_info_arr[arr_index].port_state)
               {
                  // Free the stream writer
                  capi_spr_destroy_input_port(me_ptr, arr_index);

                  me_ptr->in_port_info_arr[arr_index].port_state = DATA_PORT_STATE_CLOSED;

                  // reset input mf state at input port close
                  me_ptr->flags.is_input_media_fmt_set = FALSE;

                  capi_spr_avsync_destroy(&me_ptr->avsync_ptr, me_ptr->miid);
               }
            }
            else
            {
               if (DATA_PORT_STATE_CLOSED != me_ptr->out_port_info_arr[arr_index].port_state)
               {
                  // Free the stream writer
                  capi_spr_destroy_output_port(me_ptr, arr_index, TRUE /*need to do a partial destroy*/);

                  me_ptr->out_port_info_arr[arr_index].port_state = DATA_PORT_STATE_CLOSED;
               }
            }
            break;
         }
         default:
         {
            SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Port operation - Unsupported opcode: %lu", data_ptr->opcode);
            CAPI_SET_ERROR(result, CAPI_EUNSUPPORTED);
            break;
         }
      }

      SPR_MSG(me_ptr->miid,
              DBG_HIGH_PRIO,
              "Data port operation opcode=%lu done with result %lu. port_id=0x%lx, port_index=%lu. ",
              data_ptr->opcode,
              result,
              port_id,
              port_index);
   }

   capi_spr_check_raise_dynamic_inplace(me_ptr);

   return result;
}

/*------------------------------------------------------------------------------
  Function name: spr_check_input_for_eos_md
   Checks if the given input stream has the EOS metadata
* ------------------------------------------------------------------------------*/
bool_t spr_check_input_for_eos_md(capi_spr_t *me_ptr, capi_stream_data_t *input[])
{
   if (!input)
   {
      return FALSE;
   }
   bool_t                 is_eos_md_found = FALSE;
   capi_stream_data_v2_t *in_stream_ptr   = (capi_stream_data_v2_t *)input[0];

   if ((CAPI_STREAM_V2 != input[0]->flags.stream_data_version) || !in_stream_ptr->metadata_list_ptr)
   {
      return is_eos_md_found;
   }

   module_cmn_md_list_t *node_ptr = in_stream_ptr->metadata_list_ptr;

   while (node_ptr)
   {
      module_cmn_md_t *md_ptr = (module_cmn_md_t *)node_ptr->obj_ptr;

      if (MODULE_CMN_MD_ID_EOS == md_ptr->metadata_id)
      {
         is_eos_md_found = TRUE;
         break;
      }

      node_ptr = node_ptr->next_ptr;
   }
   return is_eos_md_found;
}

/*------------------------------------------------------------------------------
  Function name: spr_handle_metadata_util_
  Do Not call this function directly. Call spr_handle_metadata instead

  Utility function to propagate metadata from the input stream to the output
  stream during module process.
* ------------------------------------------------------------------------------*/
capi_err_t spr_handle_metadata_util_(capi_spr_t *        me_ptr,
                                    capi_stream_data_t *input[],
                                    capi_stream_data_t *output[],
                                    bool_t              is_drop_metadata)
{
   capi_err_t result = CAPI_EOK;

   capi_stream_data_v2_t *in_stream_ptr = (capi_stream_data_v2_t *)input[0];

   module_cmn_md_list_t *node_ptr = in_stream_ptr->metadata_list_ptr;
   module_cmn_md_list_t *next_ptr = NULL;

   // Drop all metadata in the list when is_drop_metadata flag is set
   // TBD : Not handling EOS metadata in drop cases for now. May need to cache and send out in output buffer as part of
   // stream flags
   if (is_drop_metadata)
   {
      while (node_ptr)
      {
         next_ptr = node_ptr->next_ptr;

         module_cmn_md_t *md_ptr = (module_cmn_md_t *)node_ptr->obj_ptr;

         // **** EOS detect and print ****
         if (MODULE_CMN_MD_ID_EOS == md_ptr->metadata_id)
         {
            SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "warning: Dropping EOS metadata : Unexpected");
         }

         me_ptr->metadata_handler.metadata_destroy(me_ptr->metadata_handler.context_ptr,
                                                   node_ptr,
                                                   TRUE /*dropped*/,
                                                   &in_stream_ptr->metadata_list_ptr);
         if (node_ptr == in_stream_ptr->metadata_list_ptr)
         {
            in_stream_ptr->metadata_list_ptr = next_ptr;
         }
         node_ptr = next_ptr;
      }
      return result;
   }

   while (node_ptr)
   {
      next_ptr = node_ptr->next_ptr;

      // need to detach node, or else, if spf_list_merge_lists is called, it will cause merging multiple times
      //
      // Err e.g. first call intending to merge only C, but merges D as well :
      //                       spf_list_merge_lists (A -> B, "C" -> D) results in A -> B -> C -> D
      //          second call: spf_list_merge_lists (A -> B -> C -> D, D) results in A -> B -> C -> D -> D
      node_ptr->next_ptr = NULL;
      node_ptr->prev_ptr = NULL;

      module_cmn_md_t *    md_ptr            = (module_cmn_md_t *)node_ptr->obj_ptr;
      module_cmn_md_eos_t *eos_md_ptr        = NULL;
      uint32_t             active_port_count = 0;
     // bool_t               eos_present       = FALSE;
      bool_t               flush_eos_present = FALSE;
      bool_t               is_dfg_present    = FALSE;

      if (MODULE_CMN_MD_ID_EOS == md_ptr->metadata_id)
      {
         bool_t out_of_band = md_ptr->metadata_flag.is_out_of_band;
         if (out_of_band)
         {
            eos_md_ptr = (module_cmn_md_eos_t *)md_ptr->metadata_ptr;
         }
         else
         {
            eos_md_ptr = (module_cmn_md_eos_t *)&(md_ptr->metadata_buf);
         }
         //eos_present       = TRUE;
         flush_eos_present = eos_md_ptr->flags.is_flushing_eos;
         me_ptr->flags.has_flushing_eos |=  flush_eos_present;

         SPR_MSG(me_ptr->miid,
                 DBG_HIGH_PRIO,
                 "SPR_MD_DBG: EOS metadata found, eos_md_ptr 0x%p, flush_eos_present = %u",
                 eos_md_ptr,
                 flush_eos_present);

         if (flush_eos_present)
         {
            uint32_t PORT_AT_GAP = 1;
            capi_spr_set_is_input_at_gap(me_ptr, PORT_AT_GAP);
#ifdef DEBUG_SPR_MODULE
            SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "input port is at gap");
#endif
         }
      }
      else if (MODULE_CMN_MD_ID_DFG == md_ptr->metadata_id)
      {
         SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "SPR_MD_DBG: DFG metadata found md_ptr %p", md_ptr);

         if (me_ptr->flags.is_timer_disabled)
         {
            me_ptr->flags.insert_int_eos_for_dfg = TRUE;
            SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "SPR_MD_DBG: DFG SPR timer is disabled and insert_int_eos_for_dfg = TRUE ");
         }

         SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "SPR_MD_DBG: DFG metadata found & absorbed, md_ptr %p", md_ptr);

         me_ptr->metadata_handler.metadata_destroy(me_ptr->metadata_handler.context_ptr,
                                                   node_ptr,
                                                   TRUE /*dropped*/,
                                                   &in_stream_ptr->metadata_list_ptr);
         is_dfg_present = TRUE;

         // If DFG came with erasure, then DFG can be set immediately. Else mark it pending till the
         // output buffer is delivered.
         // TODO: what about hold/drop case
#ifdef DEBUG_SPR_MODULE
         SPR_MSG(me_ptr->miid,
                 DBG_HIGH_PRIO,
                 "SPR_MD_DBG: DFG erasure = %d act data length %d",
                 in_stream_ptr->flags.erasure,
                 in_stream_ptr->buf_ptr->actual_data_len);
#endif

         if (in_stream_ptr->flags.erasure || (0 == in_stream_ptr->buf_ptr->actual_data_len) )
         {
            spr_avsync_set_dfg_flag(me_ptr->avsync_ptr, is_dfg_present);
         }
         else
         {
            spr_avsync_set_dfg_pending_flag(me_ptr->avsync_ptr, is_dfg_present);
         }

         uint32_t PORT_AT_GAP = 1;
         capi_spr_set_is_input_at_gap(me_ptr, PORT_AT_GAP);
#ifdef DEBUG_SPR_MODULE
         SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "input port is at gap");
#endif

         node_ptr = next_ptr;

         continue; // do not attempt to clone since the md has been destroyed
      }
      else
      {
         SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "SPR_MD_DBG: received metadata 0x%lx ", md_ptr->metadata_id);
      }

      for (uint32_t op_idx = 0; op_idx < me_ptr->max_output_ports; op_idx++)
      {
         // array index of the output port
         uint32_t arr_index = spr_get_arr_index_from_port_index(me_ptr, op_idx, FALSE);
         if (IS_INVALID_PORT_INDEX(arr_index))
         {
            continue;
         }

         if (((DATA_PORT_STATE_STOPPED | DATA_PORT_STATE_CLOSED) & me_ptr->out_port_info_arr[arr_index].port_state) ||
             (NULL == output[op_idx]) /* ||
             (eos_present  TODO: EOS disable: && me_ptr->out_port_state_arr[op_idx].is_eos_disable )*/)
         {
            // port is closed or inactive
            continue;
         }

         active_port_count++;
         SPR_MSG(me_ptr->miid, DBG_LOW_PRIO, "SPR_MD_DBG: active_port_count %lu", active_port_count);

         capi_stream_data_v2_t *stream_ptr = (capi_stream_data_v2_t *)output[op_idx];
         if (flush_eos_present)
         {
            stream_ptr->flags.marker_eos = TRUE;
         }

         if (1 == active_port_count)
         {
            // note: second arg to spf_list_merge_lists is not input stream->metadata_list_ptr because, we don't
            // want to clear it.
            module_cmn_md_list_t *temp_node_ptr = node_ptr;
            spf_list_merge_lists((spf_list_node_t **)&stream_ptr->metadata_list_ptr,
                                 (spf_list_node_t **)&temp_node_ptr);
         }
         else if (active_port_count > 0)
         {
            capi_heap_id_t hp_id = {.heap_id = (uint32_t)me_ptr->heap_id };

            result = me_ptr->metadata_handler.metadata_clone(me_ptr->metadata_handler.context_ptr,
                                                             node_ptr->obj_ptr,
                                                             &stream_ptr->metadata_list_ptr,
                                                             hp_id);
            if (CAPI_FAILED(result))
            {
               SPR_MSG(me_ptr->miid, DBG_LOW_PRIO, "SPR_MD_DBG: cloning metadata failed %lx", result);
               break;
            }
         }
      }

      if (0 == active_port_count)
      {
         // MD attached by the container to the output port needs to be dropped if there are no active ports.
         // calling destroy here otherwise MD(EOS) gets stuck.
         me_ptr->metadata_handler.metadata_destroy(me_ptr->metadata_handler.context_ptr,
                                                   node_ptr,
                                                   TRUE /* dropped */,
                                                   &in_stream_ptr->metadata_list_ptr);

         if (node_ptr == in_stream_ptr->metadata_list_ptr)
         {
            in_stream_ptr->metadata_list_ptr = next_ptr;
         }
      }

      node_ptr = next_ptr;
   }

   in_stream_ptr->flags.marker_eos  = FALSE; // always clear flush-eos flag since we propagated to output.
   in_stream_ptr->metadata_list_ptr = NULL;

   return result;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_create_trigger_policy_mem
   Creates the memory associated with the trigger policy requirements of the
   module

   Note: since this is an init time function, no SPR_MSG calls are used
* ------------------------------------------------------------------------------*/
capi_err_t capi_spr_create_trigger_policy_mem(capi_spr_t *me_ptr)
{
   // worst case each port can be its own group
   uint32_t num_groups = 1; // below code works only for 1 group

   // size for all input and output trigger groups and output non-trigger group.
   uint32_t size = ALIGN_8_BYTES(num_groups * sizeof(fwk_extn_port_trigger_group_t) +
                                 num_groups * (me_ptr->max_input_ports + me_ptr->max_output_ports) *
                                    sizeof(fwk_extn_port_trigger_affinity_t)) +
                   sizeof(fwk_extn_port_nontrigger_group_t) +
                   (me_ptr->max_input_ports + me_ptr->max_output_ports) * sizeof(fwk_extn_port_nontrigger_policy_t);

   me_ptr->data_trigger.trigger_groups_ptr =
      (fwk_extn_port_trigger_group_t *)posal_memory_malloc(size, me_ptr->heap_id);
   if (NULL == me_ptr->data_trigger.trigger_groups_ptr)
   {
      AR_MSG(DBG_ERROR_PRIO, "capi_spr: failed to allocate memory for trigger policy");
      return CAPI_EFAILED;
   }
   memset(me_ptr->data_trigger.trigger_groups_ptr, 0, size);

   uint8_t *ptr =
      (uint8_t *)me_ptr->data_trigger.trigger_groups_ptr + (num_groups * sizeof(fwk_extn_port_trigger_group_t));

   me_ptr->data_trigger.trigger_groups_ptr[0].in_port_grp_affinity_ptr = (fwk_extn_port_trigger_affinity_t *)ptr;
   ptr += sizeof(fwk_extn_port_trigger_affinity_t) * me_ptr->max_input_ports;

   me_ptr->data_trigger.trigger_groups_ptr[0].out_port_grp_affinity_ptr = (fwk_extn_port_trigger_affinity_t *)ptr;
   ptr += sizeof(fwk_extn_port_trigger_affinity_t) * me_ptr->max_output_ports;

   ptr                                        = (uint8_t *)ALIGN_8_BYTES((uint64_t)ptr);
   me_ptr->data_trigger.non_trigger_group_ptr = (fwk_extn_port_nontrigger_group_t *)ptr;
   ptr += sizeof(fwk_extn_port_nontrigger_group_t);

   me_ptr->data_trigger.non_trigger_group_ptr->in_port_grp_policy_ptr = (fwk_extn_port_nontrigger_policy_t *)ptr;
   ptr += sizeof(fwk_extn_port_nontrigger_policy_t) * me_ptr->max_input_ports;

   me_ptr->data_trigger.non_trigger_group_ptr->out_port_grp_policy_ptr = (fwk_extn_port_nontrigger_policy_t *)ptr;

   for (uint32_t i = 0; i < me_ptr->max_input_ports; i++)
   {
      me_ptr->data_trigger.non_trigger_group_ptr[0].in_port_grp_policy_ptr[me_ptr->in_port_info_arr[i].port_index] =
         FWK_EXTN_PORT_NON_TRIGGER_BLOCKED;
   }

   for (uint32_t i = 0; i < me_ptr->max_output_ports; i++)
   {
      me_ptr->data_trigger.non_trigger_group_ptr[0].out_port_grp_policy_ptr[me_ptr->out_port_info_arr[i].port_index] =
         FWK_EXTN_PORT_NON_TRIGGER_BLOCKED;
   }

   return CAPI_EOK;
}

void capi_spr_raise_event_data_trigger_in_st_cntr(capi_spr_t *me_ptr)
{
   capi_buf_t                                  payload;
   fwk_extn_event_id_data_trigger_in_st_cntr_t event;

   event.is_enable             = TRUE;
   event.needs_input_triggers  = TRUE;
   event.needs_output_triggers = TRUE;

   payload.data_ptr        = (int8_t *)&event;
   payload.actual_data_len = payload.max_data_len = sizeof(event);

   if (CAPI_FAILED(capi_cmn_raise_data_to_dsp_svc_event(&me_ptr->event_cb_info,
                                                        FWK_EXTN_EVENT_ID_DATA_TRIGGER_IN_ST_CNTR,
                                                        &payload)))
   {
      AR_MSG(DBG_ERROR_PRIO, "capi_spr: Failed to raise event to enable data_trigger.");
      return;
   }
   else
   {
      AR_MSG(DBG_HIGH_PRIO, "capi_spr: raised event to enable data_trigger.");
   }

   return;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_change_trigger_policy_util_
   This is a helper function, Do Not call this directly. Use capi_spr_change_trigger_policy()
   instead.

   Changes the trigger policy of the module when transitioning to/from DROP
   scenarios or during input port start/stop

   see notes in spr_data_trigger_t struct documentation
* ------------------------------------------------------------------------------*/
capi_err_t capi_spr_change_trigger_policy_util_(capi_spr_t *me_ptr, bool_t need_to_drop, bool_t force)
{

      fwk_extn_port_trigger_policy_t policy = need_to_drop? FWK_EXTN_PORT_TRIGGER_POLICY_MANDATORY : FWK_EXTN_PORT_TRIGGER_POLICY_OPTIONAL;

      for (uint32_t i = 0; i < me_ptr->max_input_ports; i++)
      {
         me_ptr->data_trigger.trigger_groups_ptr->in_port_grp_affinity_ptr[me_ptr->in_port_info_arr[i].port_index] =
            need_to_drop ? FWK_EXTN_PORT_TRIGGER_AFFINITY_PRESENT : FWK_EXTN_PORT_TRIGGER_AFFINITY_NONE;

         me_ptr->data_trigger.non_trigger_group_ptr->in_port_grp_policy_ptr[me_ptr->in_port_info_arr[i].port_index] =
            need_to_drop ? FWK_EXTN_PORT_NON_TRIGGER_INVALID : FWK_EXTN_PORT_NON_TRIGGER_BLOCKED;
      }

      capi_err_t err = CAPI_EOK;

      if (need_to_drop)
      {
         err = me_ptr->data_trigger.policy_chg_cb
                  .change_data_trigger_policy_cb_fn(me_ptr->data_trigger.policy_chg_cb.context_ptr,
                                                    me_ptr->data_trigger.non_trigger_group_ptr,
                                                    policy,
                                                    1,
                                                    me_ptr->data_trigger.trigger_groups_ptr);
      }
      else
      {
         err =
            me_ptr->data_trigger.policy_chg_cb
               .change_data_trigger_policy_cb_fn(me_ptr->data_trigger.policy_chg_cb.context_ptr, NULL, policy, 0, NULL);
      }

      SPR_MSG(me_ptr->miid,
              DBG_LOW_PRIO,
              "change trigger policy from (is_dropping) %u to %u resulted err 0x%lX ",
              me_ptr->data_trigger.is_dropping_data,
              need_to_drop,
              err);

      if (CAPI_EOK == err)
      {
         me_ptr->data_trigger.is_dropping_data = need_to_drop;
      }

      me_ptr->data_trigger.tp_enabled = TRUE;

   return CAPI_EOK;
}

/*==============================================================================
   Function : capi_spr_set_up_output

   output port must be created at open or at input media format.
   this is the only way to ensure that before start media format can propagate (at prepare).
   media format propagation is essential for determining path-delay before start.
==============================================================================*/
capi_err_t capi_spr_set_up_output(capi_spr_t *me_ptr, uint32_t outport_index, bool_t need_to_reinitialize)
{
   capi_err_t result = CAPI_EOK;

   if (!me_ptr->flags.is_input_media_fmt_set)
   {
      return result;
   }

   uint32_t arr_index = spr_get_arr_index_from_port_index(me_ptr, outport_index, FALSE);

   if (IS_INVALID_PORT_INDEX(arr_index))
   {
      SPR_MSG(me_ptr->miid, DBG_LOW_PRIO, "warning: Output index is unknown, port_index=%u", outport_index);
      return CAPI_EOK;
   }

   if (DATA_PORT_STATE_CLOSED != me_ptr->out_port_info_arr[arr_index].port_state)
   {
      if (CAPI_EOK == capi_spr_check_and_init_output_port(me_ptr, arr_index, need_to_reinitialize))
      {
         capi_spr_check_and_raise_output_media_format_event(me_ptr, arr_index);
      }
   }
   return result;
}

/*------------------------------------------------------------------------------
  Function name: spr_timer_enable
   Enables/Disables the timer trigger for the container based on the port states
* ------------------------------------------------------------------------------*/
capi_err_t spr_timer_enable(capi_spr_t *me_ptr)
{
   capi_err_t capi_result = CAPI_EOK;

   // at least one output/input must be present.
   bool_t input_and_output_started = ((me_ptr->num_started_output_ports > 0) && (me_ptr->num_started_input_ports > 0));

   if ((!me_ptr->flags.is_timer_disabled) && (me_ptr->flags.stm_ctrl_enable) && (input_and_output_started) && (NULL != me_ptr->signal_ptr))
   {
      // if path-delay becomes available after starting
      // then there could be a glitch

      if (!me_ptr->flags.timer_created_started)
      {
         // reset counter at the time of start
         me_ptr->counter = 1;

         // use deferrable timer if frame size is >= 5ms
         posal_timer_duration_t abs_timertype;
         if (me_ptr->frame_dur_us >= SPR_TIMER_DURATION_5_MS * NUM_US_PER_MS)
         {
            abs_timertype = POSAL_TIMER_ONESHOT_ABSOLUTE_DEFERRABLE;
            SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Using absolute defferable timer.");
         }
         else
         {
            abs_timertype = POSAL_TIMER_ONESHOT_ABSOLUTE;
            SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Using absolute Non-defferable timer.");
         }
         int32_t rc = posal_timer_create(&me_ptr->timer,
                                         abs_timertype,
                                         POSAL_TIMER_USER,
                                         (posal_signal_t)me_ptr->signal_ptr,
                                         POSAL_HEAP_DEFAULT);
         if (rc)
         {
            SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Timer creation failed");
            return CAPI_EFAILED;
         }
         me_ptr->flags.timer_created_started = TRUE;

         // first time set the signal. One-shot timer will start from the process()
         posal_signal_send((posal_signal_t)me_ptr->signal_ptr);

         me_ptr->absolute_start_time_us = (int64_t)posal_timer_get_time();

         SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "start time %ld us", me_ptr->absolute_start_time_us);
      }
   }
   else
   {
      if (me_ptr->flags.timer_created_started)
      {
         posal_timer_destroy(&me_ptr->timer);
         me_ptr->flags.timer_created_started = FALSE;
         SPR_MSG(me_ptr->miid, DBG_LOW_PRIO, "deleted timer");
      }
   }
   return capi_result;
}

/*==============================================================================
   Function : capi_spr_calc_set_timer
   Calculates drift and sets up next timer
==============================================================================*/
capi_err_t capi_spr_calc_set_timer(capi_spr_t *me_ptr)
{
   capi_err_t result        = CAPI_EOK;
   int64_t    prev_drift_us = 0, curr_drift_us = 0, inst_drift_us = 0, qt_adj_us = 0;
   uint64_t   calc_sr_us = 0, frame_dur_us = 0, curr_time_us = 0;

   if(me_ptr->flags.is_timer_disabled)
      return result;

   if (UMAX_32 == me_ptr->primary_output_arr_idx)
   {
      SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "capi_spr_calc_set_timer without setting up primary out port.");
      return CAPI_EFAILED;
   }

   uint32_t         ctrl_port_id  = me_ptr->out_port_info_arr[me_ptr->primary_output_arr_idx].ctrl_port_id;
   spr_ctrl_port_t *port_info_ptr = spr_get_ctrl_port_instance(me_ptr, ctrl_port_id);

   if ((port_info_ptr) && (CTRL_PORT_PEER_CONNECTED == port_info_ptr->state))
   {
      prev_drift_us = port_info_ptr->acc_drift.acc_drift_us;
      capi_spr_imcl_get_drift(me_ptr, port_info_ptr);
      curr_drift_us = port_info_ptr->acc_drift.acc_drift_us;

      if (me_ptr->counter > 1)
      {
         inst_drift_us = curr_drift_us - prev_drift_us;
      }
   }

   /* Accumulate inst drift here, will be updated with correction later to reflect pending amount */
   me_ptr->spr_pending_drift_us += inst_drift_us;

   if (me_ptr->spr_pending_drift_us > 0)
   {
      if (me_ptr->spr_pending_drift_us >= SPR_MAX_US_CORR_PER_MS_IN_QT_INT)
      {
         // should be +ve
         qt_adj_us = (int64_t)(SPR_MAX_US_CORR_PER_MS_IN_QT_INT * me_ptr->frame_dur_us) / NUM_US_PER_MS;
         qt_adj_us = MAX(qt_adj_us, SPR_MAX_US_CORR_PER_MS_IN_QT_INT);
         qt_adj_us = MIN(qt_adj_us, me_ptr->spr_pending_drift_us);
      }
   }
   /* -ve => QT is slower than hwep. Means hwep is faster than QT.
    *  So, QT period should be decreased to match hwep rate. */
   else if (me_ptr->spr_pending_drift_us < 0)
   {
      if (me_ptr->spr_pending_drift_us <= -SPR_MAX_US_CORR_PER_MS_IN_QT_INT)
      {
         // should be -ve
         qt_adj_us = -(int64_t)(SPR_MAX_US_CORR_PER_MS_IN_QT_INT * me_ptr->frame_dur_us) / NUM_US_PER_MS;
         qt_adj_us = MIN(qt_adj_us, -SPR_MAX_US_CORR_PER_MS_IN_QT_INT);
         qt_adj_us = MAX(qt_adj_us, me_ptr->spr_pending_drift_us);
      }
   }

   /* Update the pending drift with what has been corrected */
   me_ptr->spr_pending_drift_us -= qt_adj_us;
   /* Reported drift should be what has been corrected */
   me_ptr->spr_out_drift_info.spr_acc_drift.acc_drift_us += qt_adj_us;

   // when inp mf is not received use nominal frame length
   if (!me_ptr->flags.is_input_media_fmt_set)
   {
      me_ptr->absolute_start_time_us += (int64_t)me_ptr->frame_dur_us;
      frame_dur_us = 0;
   }
   else // calc based on sampling rate
   {
      calc_sr_us   = me_ptr->integ_sr_us * me_ptr->counter;
      frame_dur_us = calc_sr_us / me_ptr->operating_mf.format.sampling_rate;

      // Increment counter for next process
      me_ptr->counter++;
   }

   me_ptr->spr_out_drift_info.spr_acc_drift.time_stamp_us =
      me_ptr->absolute_start_time_us + frame_dur_us + me_ptr->spr_out_drift_info.spr_acc_drift.acc_drift_us;

   /* Get current time */
   curr_time_us = (uint64_t)posal_timer_get_time();

#ifdef DEBUG_SPR_MODULE
   SPR_MSG(me_ptr->miid,
           DBG_HIGH_PRIO,
           "counter %d: new timestamp %ld = abs_time + time_us_based_on_sr %lu + total corrected drift(output "
           "drift) %ld curr_time_us %ld inst drift %d, inst adj drift %d, pending drift %ld",
           me_ptr->counter,
           me_ptr->spr_out_drift_info.spr_acc_drift.time_stamp_us,
           frame_dur_us,
           me_ptr->spr_out_drift_info.spr_acc_drift.acc_drift_us,
           curr_time_us,
           inst_drift_us,
           qt_adj_us,
           me_ptr->spr_pending_drift_us);
#endif

   /**< Next interrupt time to be programmed is less than current time,
    *  which indicates signal miss.*/
   if (me_ptr->spr_out_drift_info.spr_acc_drift.time_stamp_us < curr_time_us)
   {
      SPR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "module signal miss, timestamp %d, curr time %d",
              me_ptr->spr_out_drift_info.spr_acc_drift.time_stamp_us,
              curr_time_us);

#ifdef SIM
      *((volatile uint32_t *)0) = 0; // induce a crash

#else  // handling for on-tgt

      // Counter to know how many frames needed to be added
      uint64_t curr_counter = me_ptr->counter;

      while (me_ptr->spr_out_drift_info.spr_acc_drift.time_stamp_us < curr_time_us)
      {
         // when inp mf is not received use nominal frame length
         if (!me_ptr->flags.is_input_media_fmt_set)
         {
            me_ptr->absolute_start_time_us += (int64_t)me_ptr->frame_dur_us;
            frame_dur_us = 0;
         }
         else // calc based on sampling rate
         {
            calc_sr_us   = me_ptr->integ_sr_us * me_ptr->counter;
            frame_dur_us = calc_sr_us / me_ptr->operating_mf.format.sampling_rate;

            // Increment counter which is used for calculating absolute-time based frame duration, incrementing by 1 is
            // equivalent to adding one frame dur ms
            me_ptr->counter++;
         }

         // New output timestamp
         me_ptr->spr_out_drift_info.spr_acc_drift.time_stamp_us =
            me_ptr->absolute_start_time_us + frame_dur_us + me_ptr->spr_out_drift_info.spr_acc_drift.acc_drift_us;
      }

      SPR_MSG(me_ptr->miid,
              DBG_HIGH_PRIO,
              "Counter: %ld: Added frame duration %d, %d times, to ensure next timer tick is in the future, timestamp "
              "%d, curr time "
              "%d",
              me_ptr->counter,
              frame_dur_us,
              (me_ptr->counter - curr_counter),
              me_ptr->spr_out_drift_info.spr_acc_drift.time_stamp_us,
              curr_time_us);
#endif // SIM
   }

   /** Set up timer for specified absolute duration */
   int32_t rc =
      posal_timer_oneshot_start_absolute(me_ptr->timer, me_ptr->spr_out_drift_info.spr_acc_drift.time_stamp_us);
   if (rc)
   {
      SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "one shot timer start failed result: %lu", rc);
      return CAPI_EFAILED;
   }

   return result;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_handle_media_fmt_change
   Handles run time media format change for the module.
* ------------------------------------------------------------------------------*/
capi_err_t capi_spr_handle_media_fmt_change(capi_spr_t *         me_ptr,
                                            capi_media_fmt_v2_t *media_fmt_ptr,
                                            bool_t               check_cache_mf)
{
   capi_err_t result = CAPI_EOK;

   // --------------------------------------------------------------------------------------------------
   // ---------------------------------- Media Format change in SPR ------------------------------------
   // --------------------------------------------------------------------------------------------------
   // TODO: Assumes that there is no need delay equalization for SPR output ports. The circular buffer is
   //        treated like a scratch buffer in SPR today.

   // If there is no hold buffer configured in the SPR, then any pending data in the circular buffer is dropped
   //  and the new media format is applied immediately.

   // If there is hold buffer configured, then the following steps are followed.
   // Before re-initializing the circular buffer, ensure that there is no pending data (old media format)
   // in the SPR module. The various places where data could be buffered inside the SPR module are
   //   1. Circular buffer
   //   2. Hold buffer
   //   3. Operating media format int_buffer list
   // This is to ensure that the stream writer/reader clients are re-init together and avoid maintaining
   // multiple circular buffers within SPR.

   // If media format change cannot be honored
   // 1. Cache this media format inside a list (spr_mf_handler_list_t) with object type (spr_mf_handler_t)
   // 2. Any subsequent data, that arrives at SPR, belongs to the tail of this list & is appended to the int_buffer
   //     maintained by each of the objects. This is the same as the int_buffer used for the hold buffer inside SPR
   // 3. After each process call, check if any pending media format in the head of the list can be applied.
   //     If yes, apply the media format change, raise output media format & re-init circular buffers. In the
   //     subsequent process calls, consume the data streams cached inside the int_buffer structure & continue.

   bool_t reinit_spr_ports = spr_can_reinit_with_new_mf(me_ptr);

   result |= capi_spr_check_reinit_ports(me_ptr, media_fmt_ptr, reinit_spr_ports, check_cache_mf);

   return result;
}

/*------------------------------------------------------------------------------
  Function name: spr_does_strm_reader_have_data
   Query if any of the stream reader has any unread bytes left
* ------------------------------------------------------------------------------*/
static capi_err_t spr_does_strm_reader_have_data(capi_spr_t *me_ptr, bool_t *has_data_ptr)
{
   if (!me_ptr || !has_data_ptr)
   {
      return CAPI_EBADPARAM;
   }

   *has_data_ptr = FALSE;

   uint32_t num_unread_bytes = 0;

   for (uint32_t outport_index = 0; outport_index < me_ptr->max_output_ports; outport_index++)
   {
      uint32_t arr_index           = spr_get_arr_index_from_port_index(me_ptr, outport_index, FALSE);
      uint32_t output_unread_bytes = 0;
      if (IS_INVALID_PORT_INDEX(arr_index))
      {
         SPR_MSG(me_ptr->miid, DBG_MED_PRIO, "warning: Output index is unknown, port_index=%u", outport_index);
         continue;
      }

      spr_output_port_info_t *out_port_ptr = &me_ptr->out_port_info_arr[arr_index];

      // If stream reader exists and the port is in active state, look for unread bytes
      if (out_port_ptr->strm_reader_ptr && (out_port_ptr->port_state & (DATA_PORT_STATE_STARTED)))
      {
         if (AR_DID_FAIL(spr_stream_reader_get_unread_bytes(out_port_ptr->strm_reader_ptr, &output_unread_bytes)))
         {
            continue;
         }
         else
         {
            if (output_unread_bytes)
            {
               *has_data_ptr = TRUE;
               SPR_MSG(me_ptr->miid,
                       DBG_HIGH_PRIO,
                       "port_index %d, output_unread_bytes = %d, num_unread_bytes = %d",
                       me_ptr->out_port_info_arr[arr_index].port_index,
                       output_unread_bytes,
                       num_unread_bytes);
               num_unread_bytes = output_unread_bytes;
            }
         }
      }
   }

   return CAPI_EOK;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_check_reinit_ports
   During a media format change, validate if the ports can be re-init. If so,
   proceed right away. Else, cache the media format & wait to drain any pending
   data
* ------------------------------------------------------------------------------*/
static capi_err_t capi_spr_check_reinit_ports(capi_spr_t *         me_ptr,
                                              capi_media_fmt_v2_t *media_fmt_ptr,
                                              bool_t               reinit_spr_ports,
                                              bool_t               check_cache_mf)
{
   capi_err_t  result       = CAPI_EOK;
   ar_result_t ar_result    = AR_EOK;
   uint32_t    in_arr_index = 0;

   // No data is pending, re-init right away
   if (reinit_spr_ports)
   {
      if (!me_ptr->flags.is_input_media_fmt_set)
      {
         me_ptr->flags.is_input_media_fmt_set = TRUE;
      }

      me_ptr->operating_mf = *media_fmt_ptr;

      result |= capi_check_and_init_input_port(me_ptr, in_arr_index, reinit_spr_ports);

      for (uint32_t outport_index = 0; outport_index < me_ptr->max_output_ports; outport_index++)
      {
         result |= capi_spr_set_up_output(me_ptr, outport_index, reinit_spr_ports);
      }
      // TODO: Error state handling for media format failure

      // Calculate the integer sampler rate us here to prevent recalc in process, changes if inp mf
      // e.g. for frameduration 2100us at 48K, this will be 100*10^6 which will be equivalent to 2083us.
      me_ptr->integ_sr_us =
         NUM_US_PER_SEC *
         (((me_ptr->operating_mf.format.sampling_rate / NUM_MS_PER_SEC) * me_ptr->frame_dur_us) / NUM_US_PER_MS);

      capi_spr_update_frame_duration_in_bytes(me_ptr);
   }
   else // There is some data left within the SPR module
   {
      if (check_cache_mf)
      {
         // Cache the media fmt & incoming data till the old data is drained.
         spr_mf_handler_t *mf_handler_ptr =
            (spr_mf_handler_t *)posal_memory_malloc(sizeof(spr_mf_handler_t), (POSAL_HEAP_ID)me_ptr->heap_id);

         if (!mf_handler_ptr)
         {
            return CAPI_ENOMEMORY; // RR TODO: ERROR state?
         }

         memset(mf_handler_ptr, 0, sizeof(spr_mf_handler_t));

         mf_handler_ptr->media_fmt             = *media_fmt_ptr;
         spr_input_port_info_t *input_port_ptr = &me_ptr->in_port_info_arr[in_arr_index];

         if (AR_EOK != (ar_result = spf_list_insert_tail((spf_list_node_t **)&input_port_ptr->mf_handler_list_ptr,
                                                         (void *)mf_handler_ptr,
                                                         me_ptr->heap_id,
                                                         TRUE)))
         {
            // TODO: Maintain error state
            posal_memory_free(mf_handler_ptr);
            return CAPI_EFAILED;
         }
#ifdef DEBUG_SPR_MODULE
         SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Caching media format");
         capi_spr_print_media_fmt(me_ptr, media_fmt_ptr);
#endif
      }
   }

   return result;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_check_apply_mf_change
   From the process context, checks & applies the cached media format if any
* ------------------------------------------------------------------------------*/
capi_err_t capi_spr_check_apply_mf_change(capi_spr_t *me_ptr, bool_t *did_mf_change)
{
   spr_input_port_info_t *input_port_ptr = &me_ptr->in_port_info_arr[0];
   capi_err_t             result         = CAPI_EOK;

   if (!input_port_ptr)
   {
      return result;
   }

   if (!did_mf_change)
   {
      return CAPI_EBADPARAM;
   }

   *did_mf_change = FALSE;

   // If there is no pending media format continue
   if (!input_port_ptr->mf_handler_list_ptr)
   {
      return result;
   }

   // If media format change can be applied, raise output media format & re-create stream writers & readers
   spr_mf_handler_t *mf_handler_ptr =
      (spr_mf_handler_t *)capi_spr_get_list_head_obj_ptr(input_port_ptr->mf_handler_list_ptr);

   if (!mf_handler_ptr)
   {
      SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed to get MF handler node");
      return CAPI_EFAILED;
   }

   // If this media format is already applied, then there is some data still left in the int buffer list for this mf
   if (mf_handler_ptr->is_applied)
   {
      return result;
   }

   bool_t reinit_spr_ports = spr_can_reinit_with_new_mf(me_ptr);
   bool_t DO_NOT_CACHE_MF  = FALSE;

   if (!reinit_spr_ports)
   {
      return result;
   }

   result = capi_spr_check_reinit_ports(me_ptr, &mf_handler_ptr->media_fmt, reinit_spr_ports, DO_NOT_CACHE_MF);

   if (CAPI_FAILED(result))
   {
      SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed to apply MF change to SPR with error 0x%x", result);
      // TODO: Error state handling for media format change
   }
   else
   {
      SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "Applied MF change to SPR successfully!");
      mf_handler_ptr->is_applied = TRUE;
      *did_mf_change             = TRUE;
   }

   return CAPI_EOK;
}

/*------------------------------------------------------------------------------
  Function name: spr_has_old_mf_data_pending
   Checks if the old media format has any data pending in the cached list
* ------------------------------------------------------------------------------*/
static bool_t spr_has_old_mf_data_pending(capi_spr_t *me_ptr)
{
   if (spr_has_cached_mf(me_ptr))
   {
      spr_mf_handler_t *mf_handler_ptr =
         (spr_mf_handler_t *)capi_spr_get_list_head_obj_ptr(me_ptr->in_port_info_arr->mf_handler_list_ptr);

      // If there is any stream left in the applied media format list, drain that first
      if (mf_handler_ptr && mf_handler_ptr->is_applied && mf_handler_ptr->int_buf.buf_list_ptr)
      {
         return TRUE;
      }
   }
   return FALSE;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_check_reinit_ports
   During a media format change, validates if the module can reinit right away
   or not
* ------------------------------------------------------------------------------*/
static bool_t spr_can_reinit_with_new_mf(capi_spr_t *me_ptr)
{
   // if hold buffer is not configured, then media format is applied right away.
   // any data in the circular buffer would be dropped
   if (!spr_avsync_is_hold_buf_configured(me_ptr->avsync_ptr))
   {
#ifdef DEBUG_SPR_MODULE
      SPR_MSG(me_ptr->miid, DBG_MED_PRIO, "hold buffer duration is not configured, applying media format right away");
#endif
      return TRUE;
   }

   // If there is a hold buffer duration configured, then check the state of buffering inside spr before
   // applying the media format change
   bool_t strm_reader_has_data = FALSE, is_data_held = FALSE, reinit_spr_ports = FALSE, is_old_mf_data_pending = FALSE;

   (void)spr_does_strm_reader_have_data(me_ptr, &strm_reader_has_data);
   is_data_held           = spr_avsync_does_hold_buf_exist(me_ptr->avsync_ptr);
   is_old_mf_data_pending = spr_has_old_mf_data_pending(me_ptr);

   // If there is no pending data, then reinit SPR data ports right away
   if ((!strm_reader_has_data) && (!is_data_held) && (!is_old_mf_data_pending))
   {
      reinit_spr_ports = TRUE;
   }

#ifdef DEBUG_SPR_MODULE
   SPR_MSG(me_ptr->miid,
           DBG_MED_PRIO,
           "handle input mf change, reinit_spr_ports = %d, strm_reader_has_data = %d, hold_buffer_has_data "
           "= %d is_old_mf_data_pending = %d",
           reinit_spr_ports,
           strm_reader_has_data,
           is_data_held,
           is_old_mf_data_pending);
#endif

   return reinit_spr_ports;
}

/*------------------------------------------------------------------------------
  Function name: spr_cached_mf_is_input_strm_head
   Check if the given input stream is the head of the cached media format list
* ------------------------------------------------------------------------------*/
bool_t spr_cached_mf_is_input_strm_head(capi_spr_t *me_ptr, void *input_strm_ptr)
{
   if (spr_has_cached_mf(me_ptr))
   {
      spr_mf_handler_t *mf_obj_ptr = (spr_mf_handler_t *)me_ptr->in_port_info_arr->mf_handler_list_ptr->mf_handler_ptr;

      if (spr_int_buf_is_head_node(mf_obj_ptr->int_buf.buf_list_ptr, input_strm_ptr))
      {
         return TRUE;
      }
   }

   return FALSE;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_destroy_cached_mf_list
   Destroys the cached media format list of the module. Before applying the tail mf
   ensure that the hold buffer list is destroyed
* ------------------------------------------------------------------------------*/
void capi_spr_destroy_cached_mf_list(capi_spr_t *me_ptr, bool_t apply_tail_mf)
{
   if (!me_ptr)
   {
      return;
   }
   if (!me_ptr->in_port_info_arr)
   {
      return;
   }

   if (!me_ptr->in_port_info_arr->mf_handler_list_ptr)
   {
      return;
   }

   capi_media_fmt_v2_t tail_media_fmt;

   if (apply_tail_mf)
   {
      spf_list_node_t *tail_ptr = NULL;
      (void)spf_list_get_tail_node((spf_list_node_t *)me_ptr->in_port_info_arr->mf_handler_list_ptr, &tail_ptr);

      if (!tail_ptr)
      {
         SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed to fetch the tail node of the MF list ptr");
         apply_tail_mf = FALSE;
      }
      else
      {
         spr_mf_handler_t *tail_mf_ptr = (spr_mf_handler_t *)(tail_ptr->obj_ptr);
         tail_media_fmt                = tail_mf_ptr->media_fmt;
      }
   }

   spr_mf_handler_t *obj_ptr =
      (spr_mf_handler_t *)spf_list_pop_head((spf_list_node_t **)&me_ptr->in_port_info_arr->mf_handler_list_ptr, TRUE);
   while (obj_ptr)
   {

      (void)capi_spr_destroy_int_buf_list(me_ptr, &obj_ptr->int_buf, &obj_ptr->media_fmt);
      posal_memory_free(obj_ptr);

      obj_ptr =
         (spr_mf_handler_t *)spf_list_pop_head((spf_list_node_t **)&me_ptr->in_port_info_arr->mf_handler_list_ptr,
                                               TRUE);
   }

   me_ptr->in_port_info_arr->mf_handler_list_ptr = NULL;

   if (apply_tail_mf)
   {
      capi_spr_handle_media_fmt_change(me_ptr, &tail_media_fmt, FALSE);
   }
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_add_input_to_mf_list
   Adds the input stream to the tail of the cached mf list
* ------------------------------------------------------------------------------*/
capi_err_t capi_spr_add_input_to_mf_list(capi_spr_t *me_ptr, capi_stream_data_v2_t *input_ptr)
{
   capi_err_t result = CAPI_EOK;

   spf_list_node_t *tail_ptr = NULL;
   ar_result_t ar_result = spf_list_get_tail_node((spf_list_node_t *)me_ptr->in_port_info_arr[0].mf_handler_list_ptr,
                                                  (spf_list_node_t **)&tail_ptr);
   // TODO: Check handle error state
   if (AR_DID_FAIL(ar_result) || !tail_ptr)
   {
      SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed to get tail list from mf_handler_list with error 0x%x", ar_result);
      return CAPI_EFAILED;
   }

   spr_mf_handler_t *mf_handler_ptr = (spr_mf_handler_t *)(tail_ptr->obj_ptr);

   result = capi_spr_create_int_buf_node(me_ptr,
                                         &mf_handler_ptr->int_buf,
                                         input_ptr,
                                         me_ptr->heap_id,
                                         &mf_handler_ptr->media_fmt);

   if (CAPI_FAILED(result))
   {
      SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed to create int buf node with error 0x%x", result);
   }

   return result;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_update_frame_duration_in_bytes
    Calculates the frame duration in bytes for the module.
* ------------------------------------------------------------------------------*/
void capi_spr_update_frame_duration_in_bytes(capi_spr_t *me_ptr)
{
   if (!me_ptr)
   {
      return;
   }

   if (!me_ptr->flags.is_input_media_fmt_set || (0 == me_ptr->frame_dur_us))
   {
      return;
   }

   // num_samples * frame_duration * bytes_per_sample
   me_ptr->frame_dur_bytes_per_ch =
      (((me_ptr->operating_mf.format.sampling_rate / NUM_MS_PER_SEC) * me_ptr->frame_dur_us) / NUM_US_PER_MS) *
      (CAPI_CMN_BITS_TO_BYTES(me_ptr->operating_mf.format.bits_per_sample));

   SPR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Frame duration in bytes per channel %d", me_ptr->frame_dur_bytes_per_ch);

   capi_cmn_update_port_data_threshold_event(&me_ptr->event_cb_info,
                                             (me_ptr->frame_dur_bytes_per_ch *
                                              me_ptr->operating_mf.format.num_channels),
                                             TRUE,
                                             me_ptr->in_port_info_arr[0].port_index);
}

#ifdef DEBUG_SPR_MODULE
/*------------------------------------------------------------------------------
  Function name: capi_spr_print_media_fmt
    Debug utility to print the media format from the SPR module
* ------------------------------------------------------------------------------*/
void capi_spr_print_media_fmt(capi_spr_t *me_ptr, capi_media_fmt_v2_t *media_fmt_ptr)
{
   SPR_MSG(me_ptr->miid,
           DBG_MED_PRIO,
           "media format: data_format %lu, bitstream format 0x%lX , ch=%lu, SR=%lu, bit_width=%lu, "
           "Q_fct=%lu, is_signed %lu",
           media_fmt_ptr->header.format_header.data_format,
           media_fmt_ptr->format.bitstream_format,
           media_fmt_ptr->format.num_channels,
           media_fmt_ptr->format.sampling_rate,
           media_fmt_ptr->format.bits_per_sample,
           media_fmt_ptr->format.q_factor,
           media_fmt_ptr->format.data_is_signed);
}
#endif

/*------------------------------------------------------------------------------
  Function name: capi_spr_process_register_event_to_dsp_client
    Processes registration/de-registration for events associated with SPR.
* ------------------------------------------------------------------------------*/
capi_err_t capi_spr_process_register_event_to_dsp_client(capi_spr_t *                            me_ptr,
                                                         capi_register_event_to_dsp_client_v2_t *reg_event_ptr)
{
   capi_err_t capi_result = CAPI_EOK;

   if (!me_ptr || !reg_event_ptr)
   {
      return CAPI_EBADPARAM;
   }

   switch (reg_event_ptr->event_id)
   {
      case EVENT_ID_SPR_UNDERRUN:
      {
         if (reg_event_ptr->is_register)
         {
            if (0 == me_ptr->underrun_event_info.dest_address)
            {
               if (me_ptr->flags.is_timer_disabled)
               {
                  SPR_MSG(me_ptr->miid,
                          DBG_ERROR_PRIO,
                          "Failing client 0x%x trying to register for EVENT_ID_SPR_UNDERRUN."
                          "As spr is running in non time stamp honor mode",
                          reg_event_ptr->dest_address);
                  capi_result |= CAPI_EFAILED;
               }
               else
               {
                  SPR_MSG(me_ptr->miid,
                          DBG_HIGH_PRIO,
                          "Registering for client 0x%x with token %d for EVENT_ID_SPR_UNDERRUN",
                          reg_event_ptr->dest_address,
                          reg_event_ptr->token);

                  me_ptr->underrun_event_info.event_id     = reg_event_ptr->event_id;
                  me_ptr->underrun_event_info.dest_address = reg_event_ptr->dest_address;
                  me_ptr->underrun_event_info.token        = reg_event_ptr->token;
               }
            }
            else if (me_ptr->underrun_event_info.dest_address == reg_event_ptr->dest_address)
            {
               SPR_MSG(me_ptr->miid,
                       DBG_ERROR_PRIO,
                       "warning: Client 0x%x already registered for EVENT_ID_SPR_UNDERRUN. Ignoring!",
                       reg_event_ptr->dest_address);
            }
            else
            {
               SPR_MSG(me_ptr->miid,
                       DBG_ERROR_PRIO,
                       "Failing client 0x%x trying to register for EVENT_ID_SPR_UNDERRUN. Previously registered with "
                       "0x%x",
                       reg_event_ptr->dest_address,
                       me_ptr->underrun_event_info.dest_address);
               capi_result |= CAPI_EFAILED;
            }
         }
         else // de-register
         {
            if (me_ptr->underrun_event_info.dest_address == reg_event_ptr->dest_address)
            {
               SPR_MSG(me_ptr->miid,
                       DBG_HIGH_PRIO,
                       "De-registering for client 0x%x with token %d for EVENT_ID_SPR_UNDERRUN",
                       me_ptr->underrun_event_info.dest_address,
                       me_ptr->underrun_event_info.token);

               me_ptr->underrun_event_info.dest_address = 0;
               me_ptr->underrun_event_info.token        = 0;
               me_ptr->underrun_event_info.event_id     = 0;
            }
            else if (0 == me_ptr->underrun_event_info.dest_address)
            {
               SPR_MSG(me_ptr->miid,
                       DBG_ERROR_PRIO,
                       "Failing client 0x%x trying to de-register for EVENT_ID_SPR_UNDERRUN. No registrations found",
                       reg_event_ptr->dest_address);
               capi_result |= CAPI_EFAILED;
            }
            else
            {
               SPR_MSG(me_ptr->miid,
                       DBG_ERROR_PRIO,
                       "Failing client 0x%x trying to de-register for EVENT_ID_SPR_UNDERRUN. Previously registered "
                       "with 0x%x",
                       reg_event_ptr->dest_address,
                       me_ptr->underrun_event_info.dest_address);
               capi_result |= CAPI_EFAILED;
            }
         }
         break;
      }
      default:
      {
         SPR_MSG(me_ptr->miid,
                 DBG_ERROR_PRIO,
                 "Unsupported event id 0x%x received, is_reg %d",
                 reg_event_ptr->event_id,
                 reg_event_ptr->is_register);
         capi_result |= CAPI_EFAILED;
         break;
      }
   }

   return capi_result;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_check_raise_underrun_event
    Identifies the reason for the underrun and raises the event to the client
    if registered.
* ------------------------------------------------------------------------------*/
void capi_spr_check_raise_underrun_event(capi_spr_t *me_ptr, underrun_status_t *status_ptr, uint32_t output_arr_idx)
{
   if (!me_ptr || !status_ptr)
   {
      return;
   }

   event_id_spr_underrun_t event_payload;
   capi_err_t              result = CAPI_EOK;

   // Determine the reason for the underrun in SPR as follows :-
   //   1. If AVSync is enabled, then check the previous_render_decision cached. If the render decision
   //       was HOLD/DROP, mark this as underrun due to that.
   //   2. If the render decision is set as INVALID/ AVSync is disabled, then it is always due to
   //       lack of data from upstream
   //   3. If input port is at gap, then do not report the event as underrun. This happens due to SPR
   //        receiving EOS/DFG (both intended actions from the client)

   if (capi_spr_check_if_input_is_at_gap(me_ptr))
   {
      *status_ptr = INPUT_AT_GAP;
#ifdef DEBUG_SPR_MODULE
      SPR_MSG(me_ptr->miid, DBG_LOW_PRIO, "underrun due to input at gap (eos/pause). Ignoring event");
#endif
      return;
   }
   else if (is_spr_avsync_enabled(me_ptr->avsync_ptr))
   {
      render_decision_t decision = spr_avsync_get_render_decision(me_ptr->avsync_ptr);

      if (HOLD == decision)
      {
         *status_ptr          = DATA_HELD;
         event_payload.status = UNDERRUN_STATUS_INPUT_HOLD;
      }
      else if (DROP == decision)
      {
         *status_ptr          = DATA_DROPPED;
         event_payload.status = UNDERRUN_STATUS_INPUT_DROP;
      }
      else
      {
         *status_ptr          = DATA_NOT_AVAILABLE;
         event_payload.status = UNDERRUN_STATUS_INPUT_NOT_AVAILABLE;
      }
   }
   else
   {
      *status_ptr          = DATA_NOT_AVAILABLE;
      event_payload.status = UNDERRUN_STATUS_INPUT_NOT_AVAILABLE;
   }

   if (0 == me_ptr->underrun_event_info.dest_address)
   {
#ifdef DEBUG_SPR_MODULE
      SPR_MSG(me_ptr->miid, DBG_LOW_PRIO, "No client registered for underrun event. Returning");
#endif
      return;
   }

   if (output_arr_idx != me_ptr->primary_output_arr_idx)
   {
#ifdef DEBUG_SPR_MODULE
      SPR_MSG(me_ptr->miid, DBG_LOW_PRIO, "Not primary output port idx. Returning");
#endif
      return;
   }

   capi_event_info_t event_info;
   event_info.port_info.is_valid = FALSE;

   capi_event_data_to_dsp_client_v2_t evt = { 0 };
   evt.event_id                           = EVENT_ID_SPR_UNDERRUN;
   evt.token                              = me_ptr->underrun_event_info.token;
   evt.dest_address                       = me_ptr->underrun_event_info.dest_address;
   evt.payload.actual_data_len            = sizeof(event_id_spr_underrun_t);
   evt.payload.data_ptr                   = (int8_t *)&event_payload;
   evt.payload.max_data_len               = sizeof(event_id_spr_underrun_t);

   event_info.port_info.is_valid      = FALSE;
   event_info.payload.actual_data_len = sizeof(capi_event_data_to_dsp_client_v2_t);
   event_info.payload.data_ptr        = (int8_t *)&evt;
   event_info.payload.max_data_len    = sizeof(capi_event_data_to_dsp_client_v2_t);

   result = me_ptr->event_cb_info.event_cb(me_ptr->event_cb_info.event_context,
                                           CAPI_EVENT_DATA_TO_DSP_CLIENT_V2,
                                           &event_info);

   SPR_MSG(me_ptr->miid,
           DBG_MED_PRIO,
           "Raised EVENT_ID_SPR_UNDERRUN with status %d (nodata1, hold2, drop3) with result 0x%x",
           event_payload.status,
           result);
}

/*------------------------------------------------------------------------------
  Function name: can_capi_spr_be_inplace
    Evaluates if SPR module can be dynamic inplace. If there is only one output
    port that has been opened & in start state, consider inplace.
* ------------------------------------------------------------------------------*/
static void capi_spr_check_raise_dynamic_inplace(capi_spr_t *me_ptr)
{
   if(!me_ptr)
   {
      return;
   }

   bool_t can_spr_be_inplace = FALSE;
   uint32_t num_open_ports = 0, num_started_ports = 0;

   for(uint32_t i = 0;  i< me_ptr->max_output_ports; i++)
   {
      if (DATA_PORT_STATE_STARTED == me_ptr->out_port_info_arr[i].port_state)
      {
         num_started_ports++;
      }

      if (DATA_PORT_STATE_CLOSED != me_ptr->out_port_info_arr[i].port_state)
      {
         num_open_ports++;
      }
   }

   if((1 == num_open_ports) && (1 == num_started_ports))
   {
      can_spr_be_inplace = TRUE;
   }

   if(can_spr_be_inplace != me_ptr->flags.is_inplace)
   {
      capi_err_t result = capi_cmn_raise_dynamic_inplace_event(&me_ptr->event_cb_info, can_spr_be_inplace);

      if(CAPI_EOK == result)
      {
         me_ptr->flags.is_inplace = can_spr_be_inplace;
      }
      else
      {
         me_ptr->flags.is_inplace = FALSE;
      }

      SPR_MSG(me_ptr->miid,
              DBG_HIGH_PRIO,
              "raised dynamic inplace event with value = %d result = 0x%x",
              me_ptr->flags.is_inplace,
              result);

      if(!me_ptr->flags.is_inplace)
      {
         //Evaluate simple process criteria right away
         bool_t TS_DONT_CARE = FALSE;
         capi_spr_evaluate_simple_process_criteria(me_ptr, TS_DONT_CARE);
      }
      else
      {
         // If first buffer hasnt gone out, then can defer to process call.
         // If first buffer has gone out, then based on previous state, check if circ buffer is being used & take a call.
         if(me_ptr->flags.has_rendered_first_buf)
         {
            // If there is a hold buffer duration configured, then check the state of buffering inside spr before
            // applying the media format change
            bool_t strm_reader_has_data = FALSE, is_data_held = FALSE, reinit_spr_ports = FALSE, is_old_mf_data_pending = FALSE;

            (void)spr_does_strm_reader_have_data(me_ptr, &strm_reader_has_data);
            is_data_held           = spr_avsync_does_hold_buf_exist(me_ptr->avsync_ptr);
            is_old_mf_data_pending = spr_has_old_mf_data_pending(me_ptr);

            // If there is no pending data, then reinit SPR data ports right away
            if ((!strm_reader_has_data) && (!is_data_held) && (!is_old_mf_data_pending))
            {
               reinit_spr_ports = TRUE;
            }

         //#ifdef DEBUG_SPR_MODULE
            SPR_MSG(me_ptr->miid,
                    DBG_MED_PRIO,
                    "handle inplace change, reinit_spr_ports = %d, strm_reader_has_data = %d, hold_buffer_has_data "
                    "= %d is_old_mf_data_pending = %d",
                    reinit_spr_ports,
                    strm_reader_has_data,
                    is_data_held,
                    is_old_mf_data_pending);
         //#endif

            SPR_MSG(me_ptr->miid,
                  DBG_HIGH_PRIO,
                  "first buffer already rendered is_inplace = %d can reinit = %d",
                  me_ptr->flags.is_inplace,
                  spr_can_reinit_with_new_mf(me_ptr));
         }
      }
   }
}


/*------------------------------------------------------------------------------
  Function name: capi_spr_raise_upstream_downstream_rt_event
  Raise upstream or downstream real time event .
 * ------------------------------------------------------------------------------*/
static capi_err_t capi_spr_raise_upstream_downstream_rt_event(capi_spr_t *me_ptr,
                                                              bool_t      is_input,
                                                              bool_t      is_rt,
                                                              uint32_t    port_index)
{
   capi_err_t                               capi_result = CAPI_EOK;
   capi_buf_t                               event_payload;
   intf_extn_param_id_is_rt_port_property_t event;
   event.is_rt      = is_rt;
   event.port_index = port_index;
   event.is_input   = is_input;

   event_payload.data_ptr        = (int8_t *)&event;
   event_payload.actual_data_len = event_payload.max_data_len = sizeof(event);

   capi_result = capi_cmn_raise_data_to_dsp_svc_event(&me_ptr->event_cb_info,
                                                      INTF_EXTN_EVENT_ID_IS_RT_PORT_PROPERTY,
                                                      &event_payload);
   SPR_MSG(me_ptr->miid,
           DBG_HIGH_PRIO,
           "capi_spr_utils: capi_spr_raise_upstream_downstream_rt_event "
           "is_input %lu, port index %lu, raised an event is_rt %lu resulted %lu ",
           event.is_input,
           event.port_index,
           event.is_rt,
           capi_result);
   return capi_result;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_set_data_port_property
  Handles data port property propagation and raises event
* ------------------------------------------------------------------------------*/
capi_err_t capi_spr_set_data_port_property(capi_spr_t *me_ptr, capi_buf_t *params_ptr)
{
   capi_err_t capi_result = CAPI_EOK;
   if (NULL == params_ptr->data_ptr)
   {
      SPR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "Set param id 0x%lx, received null buffer",
              INTF_EXTN_PARAM_ID_DATA_PORT_OPERATION);
      return CAPI_EBADPARAM;
   }
   if (params_ptr->actual_data_len < sizeof(param_id_spr_delay_path_end_t))
   {
      SPR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "capi_spr_utils : Invalid payload size for setting end of delay path %d",
              params_ptr->actual_data_len);
      return CAPI_ENEEDMORE;
   }

   intf_extn_param_id_is_rt_port_property_t *data_ptr =
      (intf_extn_param_id_is_rt_port_property_t *)(params_ptr->data_ptr);

   // Cache the downstream RT if the RT/NRT flag is propagated on the output port
   if (!data_ptr->is_input)
   {
      uint32_t arr_index = spr_get_arr_index_from_port_index(me_ptr, data_ptr->port_index, data_ptr->is_input);
      if (arr_index >= me_ptr->max_output_ports)
      {
        SPR_MSG(me_ptr->miid,
                DBG_ERROR_PRIO,
                "capi_spr_utils: Output arr_index %u is unknown, port_index=%u",
                arr_index,
                data_ptr->port_index);
         return CAPI_EFAILED;
      }
      me_ptr->out_port_info_arr[arr_index].is_down_stream_rt = data_ptr->is_rt;
      capi_result = capi_spr_check_timer_disable_update_tp(me_ptr);

      /* DS RT/NRT   No change in Timer                      Timer Enabled                           Timer Disabled
       * RT         (Timer is Enabled)No need to raise      Raise RT is done in above func          NA
       * NRT        (Timer is Enabled)No need to raise      NA                                      Raise NRT is done in above func
       *            (Timer is Disabled)No need to raise
       * Here above func is capi_spr_check_timer_disable_update_tp
       */
   }
   else
   {
   //If there is an output port then propagate event from input and vice versa.
   capi_result =
      capi_spr_raise_upstream_downstream_rt_event(me_ptr, !(data_ptr->is_input), data_ptr->is_rt, data_ptr->port_index);
   }
   return capi_result;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_update_input_output_port_tp_for_spr_time_mode_update
  Updates the input/output port trigger policy whenever spr_timer_disable is updated
  The trigger policy is updated only if port state is in stared state
* ------------------------------------------------------------------------------*/
static capi_err_t capi_spr_update_input_output_port_tp_for_spr_time_mode_update(capi_spr_t *me_ptr,  bool_t is_input)
{
   capi_err_t capi_result = CAPI_EOK;

   if(!is_input)
   {
     for (uint32_t arr_index = 0; arr_index < me_ptr->max_output_ports; arr_index++)
     {
        uint32_t port_index = me_ptr->out_port_info_arr[arr_index].port_index;

        if (IS_INVALID_PORT_INDEX(port_index))
        {
           SPR_MSG(me_ptr->miid,
                   DBG_ERROR_PRIO,
                   "capi_spr_utils : capi_spr_update_input_output_port_tp_for_spr_time_mode_update ",
                   "is_input : %d Invalid port index : %u for array index : %u",
                   is_input,
                   port_index,
                   arr_index);
           return CAPI_EBADPARAM;
        }

        if ((DATA_PORT_STATE_STARTED == me_ptr->out_port_info_arr[arr_index].port_state) ||
            (DATA_PORT_STATE_OPENED == me_ptr->out_port_info_arr[arr_index].port_state) )
        {
           if (me_ptr->data_trigger.non_trigger_group_ptr)
           {
              me_ptr->data_trigger.non_trigger_group_ptr[0].out_port_grp_policy_ptr[port_index] =
                 me_ptr->flags.is_timer_disabled ? FWK_EXTN_PORT_NON_TRIGGER_INVALID : FWK_EXTN_PORT_NON_TRIGGER_BLOCKED;
           }
           if (me_ptr->data_trigger.trigger_groups_ptr)
           {
              me_ptr->data_trigger.trigger_groups_ptr[0].out_port_grp_affinity_ptr[port_index] =
                 me_ptr->flags.is_timer_disabled ? FWK_EXTN_PORT_TRIGGER_AFFINITY_PRESENT : FWK_EXTN_PORT_TRIGGER_AFFINITY_NONE;
           }
        }
     }
   }
   else
   {
     for (uint32_t arr_index = 0; arr_index < me_ptr->max_input_ports; arr_index++)
     {
        uint32_t port_index = me_ptr->in_port_info_arr[arr_index].port_index;

        if (IS_INVALID_PORT_INDEX(port_index))
        {
          SPR_MSG(me_ptr->miid,
                  DBG_ERROR_PRIO,
                  "capi_spr_utils : capi_spr_update_input_output_port_tp_for_spr_time_mode_update ",
                  "is_input : %d Invalid port index : %u for array index : %u",
                  is_input,
                  port_index,
                  arr_index);
          return CAPI_EBADPARAM;
        }

        if ((DATA_PORT_STATE_STARTED == me_ptr->in_port_info_arr[arr_index].port_state) ||
              (DATA_PORT_STATE_OPENED == me_ptr->in_port_info_arr[arr_index].port_state) )
        {
           if (me_ptr->data_trigger.non_trigger_group_ptr)
           {
              me_ptr->data_trigger.non_trigger_group_ptr[0].in_port_grp_policy_ptr[port_index] =
                 me_ptr->flags.is_timer_disabled ? FWK_EXTN_PORT_NON_TRIGGER_INVALID : FWK_EXTN_PORT_NON_TRIGGER_BLOCKED;
           }
           if (me_ptr->data_trigger.trigger_groups_ptr)
           {
              me_ptr->data_trigger.trigger_groups_ptr[0].in_port_grp_affinity_ptr[port_index] =
                 me_ptr->flags.is_timer_disabled ? FWK_EXTN_PORT_TRIGGER_AFFINITY_PRESENT : FWK_EXTN_PORT_TRIGGER_AFFINITY_NONE;
           }
        }
     }
   }

   return capi_result;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_check_is_ds_rt
  Checks downstream realtime flag for the ports which are in start state
  Here, downstream is considered NRT only when downstream explictly raises the event
 * ------------------------------------------------------------------------------*/
static bool capi_spr_check_is_ds_rt(capi_spr_t *me_ptr)
{
   for (uint32_t arr_index = 0; arr_index < me_ptr->max_output_ports; arr_index++)
   {
      //if (DATA_PORT_STATE_STARTED == me_ptr->out_port_info_arr[arr_index].port_state)
      if ((DATA_PORT_STATE_STARTED == me_ptr->out_port_info_arr[arr_index].port_state) ||
          (DATA_PORT_STATE_OPENED == me_ptr->out_port_info_arr[arr_index].port_state) )
      {
        // If port starts and even if single port has downstream RT, returns RT as TRUE
         if (me_ptr->out_port_info_arr[arr_index].is_down_stream_rt)
         {
            return TRUE;
         }
      }
   }
   return FALSE;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_raise_RT_flag_when_timer_mode_updated
  Raise RT flag event whenever spr_timer_disable is updated
 * ------------------------------------------------------------------------------*/
capi_err_t capi_spr_raise_RT_flag_when_timer_mode_updated(capi_spr_t *me_ptr, bool_t is_input, bool_t is_rt)
{
   capi_err_t capi_result = CAPI_EOK;

   if (!is_input)
   {
      for (uint32_t arr_index = 0; arr_index < me_ptr->max_output_ports; arr_index++)
      {
        if ((DATA_PORT_STATE_STARTED == me_ptr->out_port_info_arr[arr_index].port_state) ||
            (DATA_PORT_STATE_OPENED == me_ptr->out_port_info_arr[arr_index].port_state) )
         {
            capi_result |= capi_spr_raise_upstream_downstream_rt_event(me_ptr,
                                                                       is_input,
                                                                       is_rt,
                                                                       me_ptr->out_port_info_arr[arr_index].port_index);
         }
      }
   }
   else
   {
      for (uint32_t arr_index = 0; arr_index < me_ptr->max_input_ports; arr_index++)
      {
        if ((DATA_PORT_STATE_STARTED == me_ptr->in_port_info_arr[arr_index].port_state) ||
              (DATA_PORT_STATE_OPENED == me_ptr->in_port_info_arr[arr_index].port_state) )
         {
            capi_result |= capi_spr_raise_upstream_downstream_rt_event(me_ptr,
                                                                       is_input,
                                                                       is_rt,
                                                                       me_ptr->in_port_info_arr[arr_index].port_index);
         }
      }
   }
   return capi_result;
}

static capi_err_t capi_spr_raise_allow_duty_cycling(capi_spr_t *me_ptr, bool_t allow_duty_cycling)
{
   capi_err_t result = CAPI_EOK;

   intf_extn_event_id_allow_duty_cycling_v2_t event_payload;
   SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO, "SPR Raise allow_duty_cycling:%d", allow_duty_cycling);

   event_payload.allow_duty_cycling = allow_duty_cycling;

   /* Create event */
   capi_event_data_to_dsp_service_t to_send;
   to_send.param_id                = INTF_EXTN_EVENT_ID_ALLOW_DUTY_CYCLING;
   to_send.payload.actual_data_len = sizeof(intf_extn_event_id_allow_duty_cycling_v2_t);
   to_send.payload.max_data_len    = sizeof(intf_extn_event_id_allow_duty_cycling_v2_t);
   to_send.payload.data_ptr        = (int8_t *)&event_payload;

   /* Create event info */
   capi_event_info_t event_info;
   event_info.port_info.is_input_port = FALSE;
   event_info.port_info.is_valid      = FALSE;
   event_info.payload.actual_data_len = sizeof(to_send);
   event_info.payload.max_data_len    = sizeof(to_send);
   event_info.payload.data_ptr        = (int8_t *)&to_send;

   result = me_ptr->event_cb_info.event_cb(me_ptr->event_cb_info.event_context, CAPI_EVENT_DATA_TO_DSP_SERVICE, &event_info);

   if (CAPI_EOK != result)
   {
       SPR_MSG(me_ptr->miid, DBG_ERROR_PRIO, "Failed to raise INTF_EXTN_EVENT_ID_ALLOW_DUTY_CYCLING event");
   }
   else
   {
       SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO,"Raised INTF_EXTN_EVENT_ID_ALLOW_DUTY_CYCLING event allow_duty_cycling:%d", event_payload.allow_duty_cycling);
   }
   return result;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_check_timer_disable_update_tp
  This function checks weather SPR timer can be disabled and
  updates the input and output port trigger policies during the transitions
 * ------------------------------------------------------------------------------*/
capi_err_t capi_spr_check_timer_disable_update_tp(capi_spr_t *me_ptr)
{
   capi_err_t capi_result           = CAPI_EOK;
#ifdef ARSPF_WIN_PORTING
   uint16_t   is_timer_disabled_loc = TRUE;  // Temporary disable for testing
#else
   uint16_t   is_timer_disabled_loc = FALSE;  //
#endif

   bool_t input_and_output_started = ((me_ptr->num_started_output_ports > 0) && (me_ptr->num_started_input_ports > 0));

   do
   {
      // Check the container is duty cycling enabled or not
      if (!me_ptr->flags.is_cntr_duty_cycling)
      {
         break;
      }
      // Checking at-least one input and one output port is enabled to disable timer
      if (!input_and_output_started)
      {
         break;
      }
      //is_timer_disabled_loc = TRUE; //To disable timer force fully for debugging

      // Only one output port active at the output of SPR
      if (1 != me_ptr->num_started_output_ports)
      {
         break;
      }

      // No timestamp honoring mode from HLOS client
      if (me_ptr->avsync_ptr && FALSE == me_ptr->avsync_ptr->client_config.allow_non_timestamp_honor_mode)
      {
         break;
      }

      // No absolute run mode
      if (me_ptr->avsync_ptr && ((SPR_RENDER_MODE_DELAYED == me_ptr->avsync_ptr->client_config.render_mode) ||
                                 (SPR_RENDER_MODE_ABSOLUTE_TIME == me_ptr->avsync_ptr->client_config.render_mode)))
      {
         break;
      }

      // NRT flag propagation from down stream
      if (TRUE == capi_spr_check_is_ds_rt(me_ptr))
      {
         break;
      }

      // Check for underflow events registration
      if (me_ptr->underrun_event_info.dest_address != 0)
      {
         break;
      }
      is_timer_disabled_loc = TRUE;
   } while (0);

#ifdef DEBUG_SPR_MODULE
   SPR_MSG(me_ptr->miid,
         DBG_HIGH_PRIO,
           "capi_spr_utils: capi_spr_check_timer_disable_update_tp "
           "is_timer_disabled_loc %lu, is_cntr_duty_cycling:%lu input_and_output_started %lu, num_started_output_ports "
           "%lu",
           is_timer_disabled_loc,
           me_ptr->flags.is_cntr_duty_cycling,
           input_and_output_started,
           me_ptr->num_started_output_ports);

   if (me_ptr->avsync_ptr)
   {
      SPR_MSG(me_ptr->miid,
            DBG_HIGH_PRIO,
              "capi_spr_utils: capi_spr_check_timer_disable_update_tp "
              "allow_non_timestamp_honor_mode %lu, render mode %lu",
              me_ptr->avsync_ptr->client_config.allow_non_timestamp_honor_mode,
              me_ptr->avsync_ptr->client_config.render_mode);
   }

   SPR_MSG(me_ptr->miid,
           DBG_HIGH_PRIO,
           "capi_spr_utils: capi_spr_check_is_ds_rt %lu",
           (uint32_t)capi_spr_check_is_ds_rt(me_ptr));

   SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO,
         "capi_spr_utils: capi_spr_check_timer_disable_update_tp underrun_event_info.dest_address %lu",
         me_ptr->underrun_event_info.dest_address);
#endif

   if (is_timer_disabled_loc != me_ptr->flags.is_timer_disabled)
   {
      SPR_MSG(me_ptr->miid,
              DBG_HIGH_PRIO,
              "capi_spr_utils: capi_spr_check_timer_disable_update_tp "
              "is_timer_disabled_loc %lu, is_cntr_duty_cycling:%lu input_and_output_started %lu, "
              "num_started_output_ports %lu",
              is_timer_disabled_loc,
              me_ptr->flags.is_cntr_duty_cycling,
              input_and_output_started,
              me_ptr->num_started_output_ports);

      if (me_ptr->avsync_ptr)
      {
         SPR_MSG(me_ptr->miid,
                 DBG_HIGH_PRIO,
                 "capi_spr_utils: capi_spr_check_timer_disable_update_tp "
                 "allow_non_timestamp_honor_mode %lu, render mode %lu",
                 me_ptr->avsync_ptr->client_config.allow_non_timestamp_honor_mode,
                 me_ptr->avsync_ptr->client_config.render_mode);
      }

      SPR_MSG(me_ptr->miid, DBG_HIGH_PRIO,
            "capi_spr_utils: capi_spr_check_is_ds_rt %lu",
   		 (uint32_t)capi_spr_check_is_ds_rt(me_ptr));

      me_ptr->flags.is_timer_disabled = is_timer_disabled_loc;
      capi_spr_raise_is_signal_triggered_change(me_ptr);
      capi_spr_change_trigger_policy_1(me_ptr);

      //TODO below if else can be combined, Keeping as it is for initial version
      if (is_timer_disabled_loc)
      {
         // is_timer_disabled_loc = TRUE ( FALSE to TRUE)
         // SPR raises NRT event at output port
         //(SPR will be able to disable timer only when downstream is NRT, So NRT should have propagated input port)

         // Upstream NRT
         capi_result |= capi_spr_raise_RT_flag_when_timer_mode_updated(me_ptr, FALSE /* is_input */, FALSE /* is_rt */);
         // Downstream NRT
         capi_result |= capi_spr_raise_RT_flag_when_timer_mode_updated(me_ptr, TRUE /* is_input */, FALSE /* is_rt */);
      }
      else
      {
         // is_timer_disabled_loc = FALSE (TRUE to FALSE)
         // SPR raises RT event at output port  and input port
         // Upstream RT
         capi_result |= capi_spr_raise_RT_flag_when_timer_mode_updated(me_ptr, FALSE /* is_input */, TRUE /* is_rt */);
         // Downstream RT
         capi_result |= capi_spr_raise_RT_flag_when_timer_mode_updated(me_ptr, TRUE /* is_input */, TRUE /* is_rt */);
      }
      //Allow duty cycling if timer is disabled (allow_duty_cycling=is_timer_disabled_loc)
      capi_spr_raise_allow_duty_cycling(me_ptr, is_timer_disabled_loc);

      SPR_MSG(me_ptr->miid,
              DBG_HIGH_PRIO,
              "capi_spr_utils: capi_spr_check_timer_disable_update_tp "
              "Updated is_timer_disabled_loc %lu, resulted %lu",
              is_timer_disabled_loc,
              capi_result);
   }

   // This function to be called to delete the timer if timer is created before or create timer if its not created before
   spr_timer_enable(me_ptr); // start or stop timer
   return capi_result;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_change_trigger_policy_1
  Raises Change trigger policy
 * ------------------------------------------------------------------------------*/
static capi_err_t capi_spr_change_trigger_policy_1(capi_spr_t *me_ptr)
{
   capi_err_t capi_result = CAPI_EOK;
   fwk_extn_port_trigger_policy_t policy = FWK_EXTN_PORT_TRIGGER_POLICY_MANDATORY;
   //bool_t input_and_output_started = ((me_ptr->num_started_output_ports > 0) && (me_ptr->num_started_input_ports > 0));

   capi_result |= capi_spr_update_input_output_port_tp_for_spr_time_mode_update(me_ptr, FALSE /*is_input*/);
   capi_result |= capi_spr_update_input_output_port_tp_for_spr_time_mode_update(me_ptr, TRUE /*is_input*/);

   if (CAPI_EOK != capi_result)
   {
      SPR_MSG(me_ptr->miid,
              DBG_ERROR_PRIO,
              "capi_spr_update_input_output_port_tp_for_spr_time_mode_update resulted error : %lu",
              capi_result);
      return capi_result;
   }

   if (me_ptr->data_trigger.tp_enabled)
   {
     if(me_ptr->flags.is_timer_disabled)
     {
       capi_result |= me_ptr->data_trigger.policy_chg_cb
                  .change_data_trigger_policy_cb_fn(me_ptr->data_trigger.policy_chg_cb.context_ptr,
                                                 me_ptr->data_trigger.non_trigger_group_ptr,
                                                 policy,
                                                 1,
                                                 me_ptr->data_trigger.trigger_groups_ptr);
     }
     else
     {
        //capi_spr_change_trigger_policy_util_(capi_spr_t *me_ptr, bool_t need_to_drop - False , bool_t force - False)
       capi_result |= me_ptr->data_trigger.policy_chg_cb
                  .change_data_trigger_policy_cb_fn(me_ptr->data_trigger.policy_chg_cb.context_ptr,
                                                    NULL,
                                                    policy,
                                                    1,
                                                    NULL);
     }

      SPR_MSG(me_ptr->miid,
              DBG_LOW_PRIO,
              "change trigger policy for timer disable : %u resulted err 0x%lX ",
              me_ptr->flags.is_timer_disabled,
              capi_result);
      me_ptr->data_trigger.tp_enabled = TRUE;
   }
   return capi_result;
}

/*------------------------------------------------------------------------------
  Function name: capi_spr_raise_is_signal_triggered_change
  Raises an event to update the is_signal_triggered_active flag
 * ------------------------------------------------------------------------------*/
static capi_err_t capi_spr_raise_is_signal_triggered_change(capi_spr_t *me_ptr)
{
   capi_err_t capi_result = CAPI_EOK;

   capi_buf_t                                      payload;
   intf_extn_event_id_is_signal_triggered_active_t event;
   event.is_signal_triggered_active = !(me_ptr->flags.is_timer_disabled);

   payload.data_ptr        = (int8_t *)&event;
   payload.actual_data_len = payload.max_data_len = sizeof(event);

   capi_result |= capi_cmn_raise_data_to_dsp_svc_event(&me_ptr->event_cb_info,
                                                       FWK_EXTN_EVENT_ID_IS_SIGNAL_TRIGGERED_ACTIVE,
                                                       &payload);

   SPR_MSG(me_ptr->miid,
           DBG_LOW_PRIO,
           "capi_spr_raise_is_signal_triggered_change raised is_signal_triggered_active  %u  resulted err 0x%lX ",
           event.is_signal_triggered_active,
           capi_result);

   return capi_result;
}
