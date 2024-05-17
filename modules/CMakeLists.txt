# Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_ENCODER)
	add_subdirectory(examples/encoder)
endif()
if(CONFIG_ECHO_CANCELLATION)
	add_subdirectory(examples/echo_cancellation)
endif()
if(CONFIG_GAIN)
	add_subdirectory(examples/gain)
endif()
