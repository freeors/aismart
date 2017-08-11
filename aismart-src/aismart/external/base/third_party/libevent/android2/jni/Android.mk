LOCAL_PATH := $(call my-dir)/../..

include $(CLEAR_VARS)

LOCAL_MODULE := event

LOCAL_PATH2 := $(LOCAL_PATH)/../../../..

LOCAL_CFLAGS := -DHAVE_CONFIG_H

# LOCAL_CFLAGS += -std=c11

# LOCAL_CPP_EXTENSION := .cxx .cpp .cc

LOCAL_C_INCLUDES := $(LOCAL_PATH2)/external \
	$(LOCAL_PATH2)/external/expat \
	$(LOCAL_PATH2)/external/boost \
	$(LOCAL_PATH2)/external/bzip2 \
	$(LOCAL_PATH2)/external/zlib \
	$(LOCAL_PATH2)/external/libyuv/include \
    $(LOCAL_PATH2)/external/boringssl/include \
    $(LOCAL_PATH2)/external/expat/lib \
    $(LOCAL_PATH2)/external/usrsctplib \
    $(LOCAL_PATH2)/external/libsrtp/include \
    $(LOCAL_PATH2)/external/libsrtp/crypto/include \
    $(LOCAL_PATH2)/external/webrtc/common_audio/signal_processing/include \
    $(LOCAL_PATH2)/external/webrtc/modules/audio_coding/codecs/isac/main/include \
	$(LOCAL_PATH2)/../linker/include/SDL2 \
	$(LOCAL_PATH2)/../linker/include/SDL2_image \
	$(LOCAL_PATH2)/../linker/include/SDL2_mixer \
	$(LOCAL_PATH2)/../linker/include/SDL2_net \
	$(LOCAL_PATH2)/../linker/include/SDL2_ttf \
	$(LOCAL_PATH2)/../linker/include/libvpx \
	$(LOCAL_PATH2)/librose \
	$(LOCAL_PATH2)/studio \
	$(LOCAL_PATH2)/external/base/third_party/libevent/android

LOCAL_SRC_FILES := \
	buffer.c \
	epoll.c \
	evbuffer.c \
	evdns.c \
	event.c \
	event_tagging.c \
	evrpc.c \
	evutil.c \
	http.c \
	log.c \
	poll.c \
	select.c \
	signal.c \
	strlcpy.c

LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)
