/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_RTC_CLIENT_H_INCLUDED
#define LIBROSE_RTC_CLIENT_H_INCLUDED


#include "util.hpp"
#include "thread.hpp"

#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "rtc_base/nethelpers.h"
#include "rtc_base/physicalsocketserver.h"
#include "rtc_base/signalthread.h"
#include "rtc_base/sigslot.h"

#include "sdl_utils.hpp"
#include "gui/dialogs/dialog.hpp"

class DummySetSessionDescriptionObserver
	: public webrtc::SetSessionDescriptionObserver {
public:
	static DummySetSessionDescriptionObserver* Create() {
		return
			new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
	}
	virtual void OnSuccess() {
		RTC_LOG(INFO) << __FUNCTION__;
	}
	virtual void OnFailure(const std::string& error) {
		RTC_LOG(INFO) << __FUNCTION__ << " " << error;
	}

protected:
	DummySetSessionDescriptionObserver() {}
	~DummySetSessionDescriptionObserver() {}
};

class trtc_client: public webrtc::PeerConnectionObserver, public webrtc::CreateSessionDescriptionObserver, public sigslot::has_slots<>
{
public:
	static std::string ice_connection_state_str(webrtc::PeerConnectionInterface::IceConnectionState state);
	static std::string ice_gathering_state_str(webrtc::PeerConnectionInterface::IceGatheringState state);
	static std::string webrtc_set_preferred_codec(const std::string& sdp, const std::string& codec);

	trtc_client(gui2::tdialog& dialog, const tpoint& desire_size, bool local_only = false, int desire_fps = nposm);
	virtual ~trtc_client();

	threading::mutex& get_sink_mutex(bool remote) { return remote? remote_sink_mutex_: local_sink_mutex_; }
	texture& frame_texture(bool remote) { return remote? remote_tex_: local_tex_; }
	texture& cv_texture(bool remote) { return remote? remote_cv_tex_: local_cv_tex_; }

	class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
	public:
		VideoRenderer(trtc_client& client, int width, int height, webrtc::VideoTrackInterface* track_to_render, bool remote);
		virtual ~VideoRenderer();

		void set_app_size(int width, int height) 
		{
			app_width_ = width;
			app_height_ = height;
		}
		// VideoSinkInterface implementation
		void OnFrame(const webrtc::VideoFrame& frame) override;

		bool new_frame() const { return new_frame_; }
		void clear_new_frame() { new_frame_ = false; }
		int frames() const { return frames_; }

	private:
		trtc_client& client_;
		int app_width_;
		int app_height_;
		int bytesperpixel_;
		rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
		bool remote_;
		bool new_frame_;
		int frames_;
	};
	VideoRenderer* vrenderer(bool remote) const { return remote? remote_renderer_.get(): local_renderer_.get(); }

	void draw_slice(SDL_Renderer* renderer, bool remote, const SDL_Rect& draw_rect, bool force_draw);

	void set_did_draw_slice(const boost::function<void (bool remote, const cv::Mat&, const texture&, const SDL_Rect&, bool new_frame, texture&)>& did)
	{
		did_draw_slice_ = did;
	}

	virtual void pre_create_renderer();
	virtual void post_create_renderer();

protected:
	void set_renderer_texture_size(bool remote, int width, int height);
	void release_textures();

	bool InitializePeerConnection();

	void ConnectToPeer(const std::string& peer_nick, bool caller, const std::string& offer);
	std::unique_ptr<cricket::VideoCapturer> OpenVideoCaptureDevice();
	bool CreatePeerConnection(bool dtls);
	void DeletePeerConnection();
	void AddStreams();

	void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);
	void StopLocalRenderer();
	void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
	void StopRemoteRenderer();

	// CreateSessionDescriptionObserver implementation.
	void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
	void OnFailure(const std::string& error) override;

	// socket
	void Connect(const std::string& server, int port);

	void DoConnect();
	void Close();
	void InitSocketSignals();
	bool ConnectControlSocket();
	void OnConnect(rtc::AsyncSocket* socket);
	void msg_2_signaling_server(rtc::AsyncSocket* socket, const std::string& msg);

	// Returns true if the whole response has been read.
	const char* read_2_buffer(rtc::AsyncSocket* socket, bool first, size_t* content_length);

	virtual void OnRead(rtc::AsyncSocket* socket) {}
	void OnClose(rtc::AsyncSocket* socket, int err);

	void OnResolveResult(rtc::AsyncResolverInterface* resolver);

	virtual void OnServerConnectionFailure();

	void resize_recv_data(int size);
	void resize_send_data(int size);

	enum {MSG_SIGNALING_CLOSE = gui2::tdialog::POST_MSG_MIN_APP};
	bool chat_OnMessage(rtc::Message* msg);

private:
	//
	// RefCountInterface implementation. CreateSessionDescriptionObserver derived from it.
	//
	void AddRef() const { ref_count_.IncRef(); }
	rtc::RefCountReleaseStatus Release() const 
	{
		const auto status = ref_count_.DecRef();
		if (status == rtc::RefCountReleaseStatus::kDroppedLastRef) {
			delete this;
		}
		return status;
	}

	//
	// PeerConnectionObserver implementation.
	//
	void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
	void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
	void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
	void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}
	void OnRenegotiationNeeded() override {}
	void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override; // {}
	void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override; // {}
	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
	void OnIceConnectionReceivingChange(bool receiving) override; // {}

	// thread context: main thread
	virtual bool app_relay_only() { return false; }

	// thread context: _signalingThread
	virtual void app_OnIceConnectionChange() {}
	virtual void app_OnIceGatheringChange() {}

	uint8_t* pixels(bool remote) const { return remote? remote_pixels_: local_pixels_; }
protected:
	gui2::tdialog& dialog_;
	tpoint desire_size_;
	int desire_fps_;
	bool local_only_;
	bool relay_only_;

	bool caller_;
	//
	// webrtc
	//
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
	// PeerConnectionClient* client_;
	std::deque<std::string*> pending_messages_;
	std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> > active_streams_;
	std::string server_;

	enum State {
		NOT_CONNECTED,
		RESOLVING,
		SIGNING_IN,
		CONNECTED,
		SIGNING_OUT_WAITING,
		SIGNING_OUT,
	};

	rtc::SocketAddress server_address_;
	rtc::AsyncResolver* resolver_;
	std::unique_ptr<rtc::AsyncSocket> control_socket_;
	std::string onconnect_data_;
	std::string notification_data_;
	std::string my_nick_;
	std::string peer_nick_;
	State state_;
	int my_id_;

	bool deconstructed_;

	std::unique_ptr<rtc::Thread> _networkThread;
	std::unique_ptr<rtc::Thread> _workerThread;
	std::unique_ptr<rtc::Thread> _signalingThread;

	std::unique_ptr<tdisable_idle_lock> disable_idle_lock_;
	std::string preferred_codec_;

	webrtc::PeerConnectionInterface::IceConnectionState connection_state_;
	webrtc::PeerConnectionInterface::IceGatheringState gathering_state_;

	threading::mutex remote_sink_mutex_;
	threading::mutex local_sink_mutex_;

	uint8_t* remote_pixels_;
	uint8_t* local_pixels_;

	texture remote_tex_;
	texture local_tex_;
	texture remote_cv_tex_;
	texture local_cv_tex_;

	char* send_data_;
	int send_data_size_;
	int send_data_vsize_;

	int last_msg_should_size_;

	char* recv_data_;
	int recv_data_size_;
	int recv_data_vsize_;

	boost::function<void (bool remote, const cv::Mat&, const texture&, const SDL_Rect&, bool new_frame, texture&)> did_draw_slice_;
	mutable webrtc::webrtc_impl::RefCounter ref_count_{0};

	std::unique_ptr<VideoRenderer> local_renderer_;
	std::unique_ptr<VideoRenderer> remote_renderer_;
};

class tavcapture: public trtc_client
{
public:
	tavcapture(gui2::tdialog& dialog, const tpoint& desire_size = tpoint(nposm, nposm), const int desire_fps = nposm);
};

#endif /* ! LIBROSE_RTC_CLIENT_H_INCLUDED */
