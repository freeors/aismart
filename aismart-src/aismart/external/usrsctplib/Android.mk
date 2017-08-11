SUB_PATH := external/usrsctplib

LOCAL_SRC_FILES += \
	$(SUB_PATH)/netinet/sctp_asconf.c \
	$(SUB_PATH)/netinet/sctp_auth.c \
	$(SUB_PATH)/netinet/sctp_bsd_addr.c \
	$(SUB_PATH)/netinet/sctp_callout.c \
	$(SUB_PATH)/netinet/sctp_cc_functions.c \
	$(SUB_PATH)/netinet/sctp_crc32.c \
	$(SUB_PATH)/netinet/sctp_indata.c \
	$(SUB_PATH)/netinet/sctp_input.c \
	$(SUB_PATH)/netinet/sctp_output.c \
	$(SUB_PATH)/netinet/sctp_pcb.c \
	$(SUB_PATH)/netinet/sctp_peeloff.c \
	$(SUB_PATH)/netinet/sctp_sha1.c \
	$(SUB_PATH)/netinet/sctp_ss_functions.c \
	$(SUB_PATH)/netinet/sctp_sysctl.c \
	$(SUB_PATH)/netinet/sctp_timer.c \
	$(SUB_PATH)/netinet/sctp_userspace.c \
	$(SUB_PATH)/netinet/sctp_usrreq.c \
	$(SUB_PATH)/netinet/sctputil.c \
	$(SUB_PATH)/netinet6/sctp6_usrreq.c \
	$(SUB_PATH)/user_environment.c \
	$(SUB_PATH)/user_mbuf.c \
	$(SUB_PATH)/user_recv_thread.c \
	$(SUB_PATH)/user_socket.c