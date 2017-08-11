SUB_PATH := $(WEBRTC_SUBPATH)/media

LOCAL_SRC_FILES += $(SUB_PATH)/base/adaptedvideotracksource.cc \
	$(SUB_PATH)/base/codec.cc \
	$(SUB_PATH)/base/h264_profile_level_id.cc \
	$(SUB_PATH)/base/mediaconstants.cc \
	$(SUB_PATH)/base/mediaengine.cc \
	$(SUB_PATH)/base/rtpdataengine.cc \
	$(SUB_PATH)/base/rtputils.cc \
	$(SUB_PATH)/base/streamparams.cc \
	$(SUB_PATH)/base/turnutils.cc \
	$(SUB_PATH)/base/videoadapter.cc \
	$(SUB_PATH)/base/videobroadcaster.cc \
	$(SUB_PATH)/base/videocapturer.cc \
	$(SUB_PATH)/base/videocommon.cc \
	$(SUB_PATH)/base/videosourcebase.cc \
	$(SUB_PATH)/base/videosourceinterface.cc \
	$(SUB_PATH)/engine/adm_helpers.cc \
	$(SUB_PATH)/engine/apm_helpers.cc \
	$(SUB_PATH)/engine/constants.cc \
	$(SUB_PATH)/engine/convert_legacy_video_factory.cc \
	$(SUB_PATH)/engine/internaldecoderfactory.cc \
	$(SUB_PATH)/engine/internalencoderfactory.cc \
	$(SUB_PATH)/engine/payload_type_mapper.cc \
	$(SUB_PATH)/engine/scopedvideodecoder.cc \
	$(SUB_PATH)/engine/scopedvideoencoder.cc \
	$(SUB_PATH)/engine/simulcast.cc \
	$(SUB_PATH)/engine/simulcast_encoder_adapter.cc \
	$(SUB_PATH)/engine/vp8_encoder_simulcast_proxy.cc \
	$(SUB_PATH)/engine/videodecodersoftwarefallbackwrapper.cc \
	$(SUB_PATH)/engine/videoencodersoftwarefallbackwrapper.cc \
	$(SUB_PATH)/engine/webrtcmediaengine.cc \
	$(SUB_PATH)/engine/webrtcvideocapturer.cc \
	$(SUB_PATH)/engine/webrtcvideocapturerfactory.cc \
	$(SUB_PATH)/engine/webrtcvideodecoderfactory.cc \
	$(SUB_PATH)/engine/webrtcvideoencoderfactory.cc \
	$(SUB_PATH)/engine/webrtcvideoengine.cc \
	$(SUB_PATH)/engine/webrtcvoiceengine.cc \
	$(SUB_PATH)/sctp/sctptransport.cc