
LOCAL_PATH := $(call my-dir)

include $(LOCAL_PATH)/rubberband/Android.mk

include $(CLEAR_VARS)

LOCAL_MODULE := testRubberBand
LOCAL_MODULE_FILENAME := libtestRubberBand

TARGET_ARCH_ABI	:=  armeabi-v7a
LOCAL_ARM_NEON := true
LOCAL_ARM_MODE := arm
LOCAL_STATIC_LIBRARIES := cpufeatures

LOCAL_C_INCLUDES := $(LOCAL_PATH)/rubberband $(LOCAL_PATH)/rubberband/src

LOCAL_SRC_FILES := testRubberBand.cpp

#LOCAL_CFLAGS += -DWANT_TIMING -DFFT_MEASUREMENT

LOCAL_SHARED_LIBRARIES = rubberband
LOCAL_LDLIBS += -llog

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/cpufeatures)

