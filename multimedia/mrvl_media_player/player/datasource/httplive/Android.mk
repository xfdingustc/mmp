LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=             \
    LiveDataSource.cpp         \
    LiveSession.cpp            \
    M3UParser.cpp              \
    ChromiumHTTPDataSource.cpp \
    support.cpp

LOCAL_C_INCLUDES :=                    \
    frameworks/av/media/libstagefright \
    external/openssl/include           \
    external/chromium                  \
    external/chromium/android

LOCAL_SHARED_LIBRARIES +=     \
    libstlport                \
    libchromium_net           \
    libutils                  \
    libcutils                 \
    libstagefright_foundation \
    libstagefright            \
    libdrmframework           \
    libcrypto


include external/stlport/libstlport.mk

LOCAL_MODULE:= libmrvl_httplive

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
