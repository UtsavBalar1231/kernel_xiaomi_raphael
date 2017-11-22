# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)

libufdt_tests_cflags := -Wall -Werror
ifeq ($(HOST_OS),darwin)
libufdt_tests_cflags += -Wno-error=format
endif

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := ufdt_gen_test_dts
LOCAL_CFLAGS := $(libufdt_tests_cflags)
LOCAL_SRC_FILES := ufdt_gen_test_dts.c

include $(BUILD_HOST_EXECUTABLE)

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := ufdt_apply_overlay_host
LOCAL_CFLAGS := $(libufdt_tests_cflags)
LOCAL_SRC_FILES := ufdt_overlay_test_app.c util.c
LOCAL_STATIC_LIBRARIES := \
    libufdt \
    libfdt \
    libufdt_sysdeps
LOCAL_REQUIRED_MODULES := dtc
LOCAL_CXX_STL := none

include $(BUILD_HOST_EXECUTABLE)

$(call dist-for-goals, dist_files, $(ALL_MODULES.ufdt_apply_overlay_host.BUILT):libufdt/ufdt_apply_overlay)

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := ufdt_apply_overlay
LOCAL_CFLAGS := $(libufdt_tests_cflags)
LOCAL_SRC_FILES := ufdt_overlay_test_app.c util.c
LOCAL_STATIC_LIBRARIES := \
    libufdt \
    libfdt \
    libufdt_sysdeps
LOCAL_REQUIRED_MODULES := dtc

include $(BUILD_EXECUTABLE)

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := fdt_apply_overlay
LOCAL_CFLAGS := $(libufdt_tests_cflags)
LOCAL_SRC_FILES := fdt_overlay_test_app.c util.c
LOCAL_STATIC_LIBRARIES := \
    libfdt \
    libufdt_sysdeps
LOCAL_REQUIRED_MODULES := dtc

include $(BUILD_HOST_EXECUTABLE)

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := extract_dtb
LOCAL_CFLAGS := $(libufdt_tests_cflags)
LOCAL_SRC_FILES := extract_dtb.c util.c
LOCAL_STATIC_LIBRARIES := \
    libfdt \
    libufdt_sysdeps
LOCAL_REQUIRED_MODULES := dtc
LOCAL_CXX_STL := none

include $(BUILD_HOST_EXECUTABLE)

$(call dist-for-goals, dist_files, $(ALL_MODULES.extract_dtb.BUILT):libufdt/extract_dtb)

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := fdt_apply_overlay
LOCAL_CFLAGS := $(libufdt_tests_cflags)
LOCAL_SRC_FILES := fdt_overlay_test_app.c util.c
LOCAL_STATIC_LIBRARIES := \
    libfdt \
    libufdt_sysdeps
LOCAL_REQUIRED_MODULES := dtc

include $(BUILD_EXECUTABLE)

###################################################
