# Copyright 2017 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := mkdtimg
LOCAL_SRC_FILES := \
	mkdtimg.c \
	mkdtimg_cfg_create.c \
	mkdtimg_core.c \
	mkdtimg_create.c \
	mkdtimg_dump.c \
	dt_table.c
LOCAL_STATIC_LIBRARIES := \
	libfdt \
	libufdt_sysdeps
LOCAL_REQUIRED_MODULES := dtc
LOCAL_CXX_STL := none

include $(BUILD_HOST_EXECUTABLE)

###################################################

$(call dist-for-goals, dist_files, $(ALL_MODULES.mkdtimg.BUILT):libufdt/mkdtimg)
