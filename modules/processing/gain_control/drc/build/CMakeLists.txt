#[[
   @file CMakeLists.txt

   @brief

   @copyright
   Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
   SPDX-License-Identifier: BSD-3-Clause-Clear
]]
cmake_minimum_required(VERSION 3.10)

set(drc_includes
    ${LIB_ROOT}/api
    ${LIB_ROOT}/inc
    ${PROJECT_SOURCE_DIR}/modules/cmn/common/utils/inc
    ${PROJECT_SOURCE_DIR}/modules/cmn/api
    ${PROJECT_SOURCE_DIR}/modules/cmn/common/internal_api
)

spf_module_sources(
   KCONFIG     CONFIG_DRC
   NAME        drc
   MAJOR_VER   1
   MINOR_VER   0
   AMDB_ITYPE  "capi"
   AMDB_MTYPE  "PP"
   AMDB_MID "0x07001066"
   AMDB_TAG "capi_drc"
   AMDB_MOD_NAME  "MODULE_ID_DRC"
   AMDB_FMT_ID1   "MEDIA_FMT_ID_PCM"
   INCLUDES ${drc_includes}
   H2XML_HEADERS  "${LIB_ROOT}/api/api_drc.h"
   CFLAGS      ""
   STATIC_LIB_PATH "${LIB_ROOT}/bin/arm/libdrc.a"
)
