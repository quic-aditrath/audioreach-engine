/**
 * \file posal_atomic.h
 * \brief
 *  	 This file is for atomic operations. This file provides functions for thread-safe operations on atomic variables.
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef POSAL_ATOMIC_H
#define POSAL_ATOMIC_H

/* =======================================================================
INCLUDE FILES FOR MODULE
========================================================================== */

#include "ar_defs.h"
#include "posal_memory.h"
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

/** @addtogroup posal_atomic
@{ */

/* -----------------------------------------------------------------------
** Global definitions/forward declarations
** ----------------------------------------------------------------------- */
/** Atomic variable type. @newpage */
typedef atomic_int* posal_atomic_word_t;
typedef posal_atomic_word_t posal_atomic_word_internal_t;

/****************************************************************************
** Atomic Ops
*****************************************************************************/

/**
  Sets a value atomically.

  @datatypes
  #posal_atomic_word_t

  @param[in] pWord  Pointer to the atomic variable.
  @param[in] val    Value to which to set the atomic variable.

  @return
  An indication of success (0) or failure (nonzero).

  @dependencies
  None.
*/
static inline void posal_atomic_set(posal_atomic_word_t pWord, int val)
{
   atomic_store(pWord, val);
}


/**
  Gets the value of an atomic variable.

  @datatypes
  #posal_atomic_word_t

  @param[in] pWord  Pointer to the atomic variable.

  @return
  The new value of the atomic variable.

  @dependencies
  None. @newpage
*/
static inline int posal_atomic_get(posal_atomic_word_t pWord)
{
  return atomic_load(pWord);
}


/**
   Creates and initializes an atomic variable.

   @datatypes
   posal_atomic_word_t

   @param[in] ppatomic_word pointer to the atomic word object handle.

   @return
   0 -- Success
   @par
   Nonzero -- Failure

   @dependencies
   None. @newpage
  */
static inline ar_result_t posal_atomic_word_create(posal_atomic_word_t *ppatomic_word, POSAL_HEAP_ID heap_id)
{
  posal_atomic_word_t tmp = (posal_atomic_word_t)posal_memory_malloc(sizeof(atomic_char32_t), heap_id);
  if (NULL == tmp)
  {
    return AR_ENOMEMORY;
  }
  else
  {
    *ppatomic_word = tmp;
    return AR_EOK;
  }
}

 /**
   Deletes the atmoic word. This function must be called for each corresponding
   posal_atomic_word_create() function to clean up all resources.

   @datatypes
   posal_atomic_word_t

   @param[in] patomic_word object handle.

   @return
   None.

   @dependencies
   Before calling this function, the object must be created.
   @newpage
  */
 static inline void posal_atomic_word_destroy(posal_atomic_word_t patomic_word)
 {
   posal_memory_free(patomic_word);
 }



/**
  Increments an atomic variable.

  @datatypes
  #posal_atomic_word_t

  @param[in] pWord  Pointer to the atomic variable.

  @return
  The new value of the atomic variable.

  @dependencies
  None.
*/
static inline int posal_atomic_increment(posal_atomic_word_t pWord)
{
   atomic_fetch_add(pWord, 1);
   return (int)atomic_load(pWord);
}


/**
  Adds to an atomic variable.

  @datatypes
  #posal_atomic_word_t

  @param[in] pWord  Pointer to the atomic variable.
  @param[in] unVal  Number to add to the atomic variable.

  @return
  The new value of the atomic variable.

  @dependencies
  None. @newpage
*/
static inline void posal_atomic_add(posal_atomic_word_t pWord, uint32_t unVal)
{
   atomic_fetch_add(pWord, (int) unVal);
}


/**
  Decrements an atomic variable.

  @datatypes
  #posal_atomic_word_t

  @param[in] pWord  Pointer to the atomic variable.

  @return
  The new value of the atomic variable.

  @dependencies
  None.
*/
static inline int posal_atomic_decrement(posal_atomic_word_t pWord)
{
   atomic_fetch_sub(pWord, 1);
   return (int)atomic_load(pWord);
}


/**
  Subtracts from an atomic variable.

  @datatypes
  #posal_atomic_word_t

  @param[in] pWord  Pointer to the atomic variable.
  @param[in] unVal  Number to subtract from the atomic variable.

  @return
  The new value of the atomic variable.

  @dependencies
  None. @newpage
*/
static inline void posal_atomic_subtract(posal_atomic_word_t pWord, uint32_t unVal)
{
   atomic_fetch_sub(pWord, (int) unVal);
}


/**
  Bitwise ORs an atomic variable.

  @datatypes
  #posal_atomic_word_t

  @param[in] word_ptr  Pointer to the atomic variable.
  @param[in] val       OR bitfield.

  @return
  The new value of the atomic variable.

  @dependencies
  None.
*/
static inline void posal_atomic_or(posal_atomic_word_t word_ptr, uint32_t val)
{
   atomic_fetch_or(word_ptr, (int) val);
}


/**
  Bitwise ANDs an atomic variable.

  @datatypes
  #posal_atomic_word_t

  @param[in] word_ptr  Pointer to the atomic variable.
  @param[in] val       AND bitfield.

  @return
  The new value of the atomic variable.

  @dependencies
  None. @newpage
*/
static inline void posal_atomic_and(posal_atomic_word_t word_ptr, uint32_t val)
{
   atomic_fetch_and(word_ptr, (int) val);
}


/** @} */ /* end_addtogroup posal_atomic */

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //POSAL_ATOMIC_H
