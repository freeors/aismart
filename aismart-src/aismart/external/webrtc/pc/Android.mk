SUB_PATH := $(WEBRTC_SUBPATH)/pc

LOCAL_SRC_FILES += $(SUB_PATH)/audiomonitor.cc \
	$(SUB_PATH)/audiotrack.cc \
	$(SUB_PATH)/bundlefilter.cc \
	$(SUB_PATH)/channel.cc \
	$(SUB_PATH)/channelmanager.cc \
	$(SUB_PATH)/createpeerconnectionfactory.cc \
	$(SUB_PATH)/currentspeakermonitor.cc \
    $(SUB_PATH)/datachannel.cc \
    $(SUB_PATH)/dtlssrtptransport.cc \
    $(SUB_PATH)/dtmfsender.cc \
    $(SUB_PATH)/externalhmac.cc \
	$(SUB_PATH)/iceserverparsing.cc \
    $(SUB_PATH)/jsepicecandidate.cc \
    $(SUB_PATH)/jsepsessiondescription.cc \
    $(SUB_PATH)/jseptransport.cc \
    $(SUB_PATH)/localaudiosource.cc \
    $(SUB_PATH)/mediamonitor.cc \
    $(SUB_PATH)/mediasession.cc \
    $(SUB_PATH)/mediastream.cc \
    $(SUB_PATH)/mediastreamobserver.cc \
    $(SUB_PATH)/peerconnection.cc \
    $(SUB_PATH)/peerconnectionfactory.cc \
    $(SUB_PATH)/remoteaudiosource.cc \
    $(SUB_PATH)/rtcpmuxfilter.cc \
    $(SUB_PATH)/rtcstatscollector.cc \
    $(SUB_PATH)/rtpmediautils.cc \
    $(SUB_PATH)/rtpreceiver.cc \
    $(SUB_PATH)/rtpsender.cc \
    $(SUB_PATH)/rtptransceiver.cc \
    $(SUB_PATH)/rtptransport.cc \
    $(SUB_PATH)/sctputils.cc \
    $(SUB_PATH)/sessiondescription.cc \
    $(SUB_PATH)/srtpfilter.cc \
    $(SUB_PATH)/srtpsession.cc \
    $(SUB_PATH)/statscollector.cc \
    $(SUB_PATH)/srtptransport.cc \
    $(SUB_PATH)/trackmediainfomap.cc \
    $(SUB_PATH)/transportcontroller.cc \
    $(SUB_PATH)/videocapturertracksource.cc \
    $(SUB_PATH)/videotrack.cc \
    $(SUB_PATH)/videotracksource.cc \
    $(SUB_PATH)/webrtcsdp.cc \
    $(SUB_PATH)/webrtcsessiondescriptionfactory.cc