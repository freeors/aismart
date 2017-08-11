SUB_PATH := $(WEBRTC_SUBPATH)/voice_engine

LOCAL_SRC_FILES += $(SUB_PATH)/audio_level.cc \
	$(SUB_PATH)/channel.cc \
	$(SUB_PATH)/channel_manager.cc \
	$(SUB_PATH)/channel_proxy.cc \
	$(SUB_PATH)/shared_data.cc \
	$(SUB_PATH)/transport_feedback_packet_loss_tracker.cc \
	$(SUB_PATH)/utility.cc \
	$(SUB_PATH)/voe_base_impl.cc \
	$(SUB_PATH)/voice_engine_impl.cc