SUB_PATH := $(WEBRTC_SUBPATH)/sdk/android

LOCAL_SRC_FILES += $(SUB_PATH)/src/jni/androidhistogram.cc \
	$(SUB_PATH)/src/jni/class_loader.cc \
    $(SUB_PATH)/src/jni/jni_common.cc \
    $(SUB_PATH)/src/jni/jni_generator_helper.cc \
    $(SUB_PATH)/src/jni/jni_helpers.cc \
    $(SUB_PATH)/src/jni/androidmediadecoder.cc \
    $(SUB_PATH)/src/jni/androidmediaencoder.cc \
    $(SUB_PATH)/src/jni/androidvideotracksource.cc \
    $(SUB_PATH)/src/jni/encodedimage.cc \
    $(SUB_PATH)/src/jni/hardwarevideoencoderfactory.cc \
    $(SUB_PATH)/src/jni/nv12buffer.cc \
    $(SUB_PATH)/src/jni/nv21buffer.cc \
    $(SUB_PATH)/src/jni/pc/video.cc \
    $(SUB_PATH)/src/jni/surfacetexturehelper.cc \
    $(SUB_PATH)/src/jni/video_renderer.cc \
    $(SUB_PATH)/src/jni/videocodecinfo.cc \
    $(SUB_PATH)/src/jni/videocodecstatus.cc \
    $(SUB_PATH)/src/jni/videodecoderfactorywrapper.cc \
    $(SUB_PATH)/src/jni/videodecoderfallback.cc \
    $(SUB_PATH)/src/jni/videodecoderwrapper.cc \
    $(SUB_PATH)/src/jni/videoencoderfactorywrapper.cc \
    $(SUB_PATH)/src/jni/videoencoderfallback.cc \
    $(SUB_PATH)/src/jni/videoencoderwrapper.cc \
    $(SUB_PATH)/src/jni/videofilerenderer.cc \
    $(SUB_PATH)/src/jni/videoframe.cc \
    $(SUB_PATH)/src/jni/videotrack.cc \
    $(SUB_PATH)/src/jni/vp8codec.cc \
    $(SUB_PATH)/src/jni/vp9codec.cc \
    $(SUB_PATH)/src/jni/wrapped_native_i420_buffer.cc \
    $(SUB_PATH)/src/jni/wrappednativecodec.cc \
    $(SUB_PATH)/src/jni/yuvhelper.cc \
    $(SUB_PATH)/src/jni/jni_onload.cc