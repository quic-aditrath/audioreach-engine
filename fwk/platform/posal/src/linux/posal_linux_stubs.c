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
