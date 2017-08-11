SUB_PATH := $(WEBRTC_SUBPATH)/api

LOCAL_SRC_FILES += $(SUB_PATH)/audio_codecs/g711/audio_decoder_g711.cc \
	$(SUB_PATH)/audio_codecs/g711/audio_encoder_g711.cc \
	$(SUB_PATH)/audio_codecs/g722/audio_decoder_g722.cc \
	$(SUB_PATH)/audio_codecs/g722/audio_encoder_g722.cc \
	$(SUB_PATH)/audio_codecs/isac/audio_decoder_isac_fix.cc \
	$(SUB_PATH)/audio_codecs/isac/audio_encoder_isac_fix.cc \
	$(SUB_PATH)/audio_codecs/L16/audio_decoder_L16.cc \
	$(SUB_PATH)/audio_codecs/L16/audio_encoder_L16.cc \
	$(SUB_PATH)/audio_codecs/audio_decoder.cc \
	$(SUB_PATH)/audio_codecs/audio_encoder.cc \
	$(SUB_PATH)/audio_codecs/audio_format.cc \
	$(SUB_PATH)/audio_codecs/builtin_audio_decoder_factory.cc \
	$(SUB_PATH)/audio_codecs/builtin_audio_encoder_factory.cc \
	$(SUB_PATH)/candidate.cc \
	$(SUB_PATH)/jsep.cc \
	$(SUB_PATH)/mediaconstraintsinterface.cc \
	$(SUB_PATH)/mediastreaminterface.cc \
	$(SUB_PATH)/mediatypes.cc \
	$(SUB_PATH)/optional.cc \
	$(SUB_PATH)/proxy.cc \
	$(SUB_PATH)/rtcerror.cc \
	$(SUB_PATH)/rtp_headers.cc \
	$(SUB_PATH)/rtpparameters.cc \
	$(SUB_PATH)/statstypes.cc \
	$(SUB_PATH)/video_codecs/video_encoder.cc \
	$(SUB_PATH)/video/i420_buffer.cc \
	$(SUB_PATH)/video/video_content_type.cc \
	$(SUB_PATH)/video/video_frame.cc \
	$(SUB_PATH)/video/video_frame_buffer.cc \
	$(SUB_PATH)/video/video_timing.cc