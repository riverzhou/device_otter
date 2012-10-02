LOCAL_PATH := $(call my-dir)

# HAL module implementation, not prelinked and stored in
# hw/<HWCOMPOSE_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/../vendor/lib/hw
LOCAL_SHARED_LIBRARIES := liblog libEGL libcutils libutils libhardware libhardware_legacy 
LOCAL_SRC_FILES := hwc.c 

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := hwcomposer.t_otter
LOCAL_CFLAGS := -DLOG_TAG=\"ti_hwc\"

include $(BUILD_SHARED_LIBRARY)
