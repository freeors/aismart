SUB_PATH := $(WEBRTC_SUBPATH)/logging

LOCAL_SRC_FILES += $(SUB_PATH)/rtc_event_log/rtc_event_log.cc \
	$(SUB_PATH)/rtc_event_log/rtc_event_log_factory.cc \
	$(SUB_PATH)/rtc_event_log/rtc_stream_config.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_alr_state.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_audio_network_adaptation.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_audio_playout.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_audio_receive_stream_config.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_audio_send_stream_config.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_bwe_update_delay_based.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_bwe_update_loss_based.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_logging_started.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_logging_stopped.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_probe_cluster_created.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_probe_result_failure.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_probe_result_success.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_rtcp_packet_incoming.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_rtp_packet_incoming.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_rtp_packet_outgoing.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_video_receive_stream_config.cc \
	$(SUB_PATH)/rtc_event_log/events/rtc_event_video_send_stream_config.cc \
	$(SUB_PATH)/rtc_event_log/output/rtc_event_log_output_file.cc