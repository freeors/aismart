SUB_PATH := $(WEBRTC_SUBPATH)/system_wrappers

LOCAL_SRC_FILES += $(SUB_PATH)/source/aligned_malloc.cc \
	$(SUB_PATH)/source/atomic32.cc \
	$(SUB_PATH)/source/clock.cc \
	$(SUB_PATH)/source/cpu_features.cc \
	$(SUB_PATH)/source/cpu_info.cc \
	$(SUB_PATH)/source/event.cc \
	$(SUB_PATH)/source/event_timer_posix.cc \
	$(SUB_PATH)/source/field_trial_default.cc \
	$(SUB_PATH)/source/file_impl.cc \
	$(SUB_PATH)/source/metrics_default.cc \
	$(SUB_PATH)/source/rtp_to_ntp_estimator.cc \
	$(SUB_PATH)/source/rw_lock.cc \
	$(SUB_PATH)/source/rw_lock_posix.cc \
	$(SUB_PATH)/source/sleep.cc \
	$(SUB_PATH)/source/timestamp_extrapolator.cc