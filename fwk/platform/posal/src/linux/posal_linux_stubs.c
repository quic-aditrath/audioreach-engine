/**
 * \file posal_linux_stubs.c
 *
 * \brief
 *  	This file contains the stub implementation for the APIs that are not yet
 *      implemented for Linux SPF
 *
 * \copyright
 *      Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *      SPDX-License-Identifier: BSD-3-Clause
 */
/* ----------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------- */
#include "posal.h"
#include "platform_cfg.h"

/* ----------------------------------------------------------------------------
 * Global Declarations/Definitions
 * ------------------------------------------------------------------------- */
/* Global island heap id variable defined here */
/** Default island heap = POSAL_HEAP_DEFAULT on Linux */
POSAL_HEAP_ID spf_mem_island_heap_id = POSAL_HEAP_DEFAULT;

/* -------------------------------------------------------------------------
 * Function Definitions
 * ------------------------------------------------------------------------- */

bool_t posal_island_get_island_status(void)
{
   return FALSE;
}

ar_result_t posal_island_trigger_island_exit(void)
{
   return AR_EUNSUPPORTED;
}

ar_result_t core_drv_reset(){
   return AR_EOK;
}

bool_t posal_data_log_code_status(uint32_t log_code){
   return false;
}

ar_result_t posal_data_log_alloc_commit(posal_data_log_info_t *log_info_ptr){
   ar_result_t result = AR_EOK;
   return result;
}

void *posal_data_log_alloc(uint32_t buf_Size, uint32_t log_code, posal_data_log_format_t data_fmt){
   return NULL;
}

uint32_t posal_data_log_get_max_buf_size(){
   return 0;
}

ar_result_t posal_data_log_commit(void *log_pkt_payload_ptr, posal_data_log_info_t *log_info_ptr){
   return AR_EOK;
}

POSAL_HEAP_ID posal_private_get_island_heap_id_v2(uint32_t island_heap_type)
{
   return spf_mem_island_heap_id;
}

mid_stack_pair_info_t *get_platform_prop_info(platform_prop_t prop, uint32_t *elem_num)
{
   *elem_num = 0;
   return NULL;
}

int32 posal_timer_oneshot_start_absolute(posal_timer_t pTimer, int64_t time){
   return AR_EOK;
}

ar_result_t posal_power_mgr_send_command(uint32_t msg_opcode, void *payload_ptr , uint32_t payload_size){
   return AR_EOK;
}
