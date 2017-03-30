# Copyright 2016 The Android Open Source Project

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
