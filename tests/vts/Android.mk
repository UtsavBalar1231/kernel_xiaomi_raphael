#
# Copyright (C) 2018 The Android Open Source Project
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
#
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := verifyDTBO.sh
LOCAL_SRC_FILES := verifyDTBO.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_TAGS := optional
LOCAL_IS_HOST_MODULE := true
LOCAL_REQUIRED_MODULES :=                       \
    mkdtimg                                     \
    ufdt_verify_overlay_host                    \

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE := VtsVerifyDTBOTest
VTS_CONFIG_SRC_DIR := system/tools/libufdt/test/vts
-include test/vts/tools/build/Android.host_config.mk
