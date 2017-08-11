SUB_PATH := $(WEBRTC_SUBPATH)/call

LOCAL_SRC_FILES += $(SUB_PATH)/audio_send_stream.cc \
	$(SUB_PATH)/bitrate_allocator.cc \
	$(SUB_PATH)/call.cc \
	$(SUB_PATH)/callfactory.cc \
	$(SUB_PATH)/flexfec_receive_stream_impl.cc \
	$(SUB_PATH)/rtcp_demuxer.cc \
	$(SUB_PATH)/rtp_config.cc \
	$(SUB_PATH)/rtp_demuxer.cc \
	$(SUB_PATH)/rtp_rtcp_demuxer_helper.cc \
	$(SUB_PATH)/rtp_stream_receiver_controller.cc \
	$(SUB_PATH)/rtp_transport_controller_send.cc \
	$(SUB_PATH)/rtx_receive_stream.cc \
	$(SUB_PATH)/syncable.cc \
	$(SUB_PATH)/video_config.cc \
	$(SUB_PATH)/video_receive_stream.cc \
	$(SUB_PATH)/video_send_stream.cc