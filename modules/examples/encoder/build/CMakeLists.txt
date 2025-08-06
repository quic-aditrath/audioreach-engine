# Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: BSD-3-Clause-Clear

set(example_encoder_sources
	${LIB_ROOT}/capi/src/capi_example_enc_module_utils.c
	${LIB_ROOT}/capi/src/capi_example_enc_module.c
	${LIB_ROOT}/lib/src/example_enc_lib.c
)

set(example_encoder_includes
	${LIB_ROOT}/capi/inc
	${LIB_ROOT}/lib/inc
	${LIB_ROOT}/api
	${LIB_ROOT}/capi/src
	${LIB_ROOT}/lib/src
)

spf_module_sources(
	KCONFIG		CONFIG_ENCODER
	NAME		example_encoder
	MAJOR_VER	1
	MINOR_VER	0
	AMDB_ITYPE	"capi"
	AMDB_MTYPE	"encoder"
	AMDB_MID	"0x0700109B"
	AMDB_TAG	"capi_example_enc_module"
	AMDB_MOD_NAME	"MODULE_ID_EXAMPLE_ENC"
	AMDB_FMT_ID1	"MEDIA_FMT_ID_EXAMPLE"
	SRCS		${example_encoder_sources}
	INCLUDES	${example_encoder_includes}
	H2XML_HEADERS	"${LIB_ROOT}/api/example_encoder_module_api.h"
	CFLAGS		""
)
