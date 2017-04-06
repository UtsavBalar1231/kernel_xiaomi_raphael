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

common_src_files := \
    ufdt_overlay.c \
    ufdt_convert.c \
    ufdt_node.c \
    ufdt_prop_dict.c

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libufdt
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := $(common_src_files)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := \
    libfdt \
    libufdt_sysdeps

include $(BUILD_STATIC_LIBRARY)

####################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libufdt
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := $(common_src_files)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := \
    libfdt \
    libufdt_sysdeps

include $(BUILD_HOST_STATIC_LIBRARY)

###################################################

include $(call first-makefiles-under, $(LOCAL_PATH))
