#ifndef GEN_TOPO_GLOBAL_SHMEM_MSG_H
#define GEN_TOPO_GLOBAL_SHMEM_MSG_H

/**
 * \file gen_topo_global_shmem_msg.h
 *
 * \brief
 *
 *     Topo utility for global shared  memory message handling.
 *
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#include "posal.h"

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

typedef struct gen_topo_t        gen_topo_t;
typedef struct gen_topo_module_t gen_topo_module_t;

ar_result_t gen_topo_init_global_sh_mem_extn(void *topo_ptr, gen_topo_module_t *module_ptr);

ar_result_t gen_topo_set_global_sh_mem_msg(void    *topo_ptr,
                                           uint32_t miid,
                                           uint32_t shmem_id,
                                           void    *virt_addr_ptr,
                                           uint32_t payload_size);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif // #ifndef GEN_TOPO_GLOBAL_SHMEM_MSG_H
