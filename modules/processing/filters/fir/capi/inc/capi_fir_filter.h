/* ======================================================================== */
/**
   @file capi_fir_filter.h

   Header file to implement the Common Audio Processor Interface v2 for
   Fir Filter Library
*/

/* =========================================================================
 * Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
   ========================================================================== */

/*------------------------------------------------------------------------
 * Include files
 * -----------------------------------------------------------------------*/
#ifndef CAPI_FIR_FILTER_H
#define CAPI_FIR_FILTER_H

#include "capi.h"

#ifdef __cplusplus
extern "C"
{
#endif //__cplusplus


/*------------------------------------------------------------------------
 * Function declarations
 * -----------------------------------------------------------------------*/

capi_err_t capi_fir_get_static_properties (
        capi_proplist_t *init_set_properties,
        capi_proplist_t *static_properties);


capi_err_t capi_fir_init (
        capi_t*              _pif,
        capi_proplist_t      *init_set_properties);



#ifdef __cplusplus
}
#endif //__cplusplus

#endif // CAPI_FIR_FILTER_H
