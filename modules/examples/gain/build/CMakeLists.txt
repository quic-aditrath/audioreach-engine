# Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: BSD-3-Clause-Clear

set(example_gain_sources
	${LIB_ROOT}/capi/src/capi_example_gain_module.c
	${LIB_ROOT}/capi/src/capi_example_gain_module_utils.c
	${LIB_ROOT}/lib/src/example_gain_module_lib.c
)

set(example_gain_includes
	${LIB_ROOT}/capi/inc
	${LIB_ROOT}/lib/inc
	${LIB_ROOT}/api
	${LIB_ROOT}/capi/src
	${LIB_ROOT}/lib/src
)

spf_module_sources(
	KCONFIG		CONFIG_EXAMPLE_GAIN
	NAME		example_gain
	MAJOR_VER	1
	MINOR_VER	0
	AMDB_ITYPE	"capi"
	AMDB_MTYPE	"generic"
	AMDB_MID	"0x0700106F"
	AMDB_TAG	"capi_example_gain_module"
	AMDB_MOD_NAME	"MODULE_ID_EXAMPLE_GAIN_MODULE"
	AMDB_FMT_ID1	"MEDIA_FMT_ID_EXAMPLE"
	SRCS		${example_gain_sources}
	INCLUDES	${example_gain_includes}
	H2XML_HEADERS	${LIB_ROOT}/api/example_gain_module_api.h
	CFLAGS		""
)
