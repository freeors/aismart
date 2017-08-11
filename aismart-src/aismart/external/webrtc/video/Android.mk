SUB_PATH := $(WEBRTC_SUBPATH)/video

LOCAL_SRC_FILES += $(SUB_PATH)/call_stats.cc \
	$(SUB_PATH)/encoder_rtcp_feedback.cc \
	$(SUB_PATH)/overuse_frame_detector.cc \
	$(SUB_PATH)/payload_router.cc \
	$(SUB_PATH)/quality_threshold.cc \
	$(SUB_PATH)/receive_statistics_proxy.cc \
	$(SUB_PATH)/report_block_stats.cc \
	$(SUB_PATH)/rtp_streams_synchronizer.cc \
	$(SUB_PATH)/rtp_video_stream_receiver.cc \
	$(SUB_PATH)/send_delay_stats.cc \
	$(SUB_PATH)/send_statistics_proxy.cc \
	$(SUB_PATH)/stats_counter.cc \
	$(SUB_PATH)/stream_synchronization.cc \
	$(SUB_PATH)/transport_adapter.cc \
	$(SUB_PATH)/video_receive_stream.cc \
	$(SUB_PATH)/video_send_stream.cc \
	$(SUB_PATH)/video_stream_decoder.cc \
	$(SUB_PATH)/video_stream_encoder.cc