SUB_PATH := $(WEBRTC_SUBPATH)/common_video

LOCAL_SRC_FILES += $(SUB_PATH)/bitrate_adjuster.cc \
	$(SUB_PATH)/h264/h264_bitstream_parser.cc \
	$(SUB_PATH)/h264/h264_common.cc \
	$(SUB_PATH)/h264/pps_parser.cc \
	$(SUB_PATH)/h264/sps_parser.cc \
	$(SUB_PATH)/h264/sps_vui_rewriter.cc \
	$(SUB_PATH)/i420_buffer_pool.cc \
	$(SUB_PATH)/incoming_video_stream.cc \
	$(SUB_PATH)/libyuv/webrtc_libyuv.cc \
	$(SUB_PATH)/video_frame.cc \
	$(SUB_PATH)/video_frame_buffer.cc \
	$(SUB_PATH)/video_render_frames.cc