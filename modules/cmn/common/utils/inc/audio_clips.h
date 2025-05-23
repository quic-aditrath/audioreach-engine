/*
 * Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
@file audio_clips.h

This file contains declarations for clipping functions.
*/

/*===========================================================================
NOTE: The @brief description and any detailed descriptions above do not appear 
      in the PDF. 

      The elite_audio_mainpage.dox file contains all file/group descriptions 
      that are in the output PDF generated using Doxygen and Latex. To edit or 
      update any of the file/group text in the PDF, edit the 
      elite_audio_mainpage.dox file or contact Tech Pubs.
===========================================================================*/

#ifndef _CLIPS_H
#define _CLIPS_H

#include "audio_comdef.h"

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

/** @addtogroup dsp_algorithms
@{ */

/**
  Clips 16-bit samples.

  @param [in]  input        Pointer to the input buffer.
  @param [in]  lowerBound   Lower bound for clipping; valid minimum value.
  @param [out] upperBound   Output buffer; data is interleaved; valid maximum 
                            value.

  @return
  bool_t -- Success or failure.

  @dependencies
  None.
*/
bool_t clip_16
(
    int16_t          *input, 
    int16_t           lowerBound,
    int16_t           upperBound 
);

/**
  Clips 32-bit samples.

  @param [in]  input         Pointer to the input buffer.
  @param [in]  lowerBound    Lower bound for clipping; valid minimum value.
  @param [out] upperBound    Output buffer; data is interleaved; valid maxmum 
                             value.

  @return
  bool_t -- Success or failure.

  @dependencies
  None.
*/
bool_t clip_32
(
    int32_t          *input, 
    int32_t           lowerBound,
    int32_t           upperBound 
);

/** @} */ /* end_addtogroup dsp_algorithms */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _CLIPS_H*/

