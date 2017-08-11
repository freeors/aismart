/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "rtc_client.hpp"

#include "gettext.hpp"
#include "video.hpp"
#include "serialization/string_utils.hpp"
#include "wml_exception.hpp"
#include "base_instance.hpp"

#include "gui/widgets/settings.hpp"

#include "api/video/i420_buffer.h"
#include "api/test/fakeconstraints.h"
#include "rtc_base/json.h"
#include "rtc_base/logging.h"
#include "media/engine/webrtcvideocapturerfactory.h"
#include "modules/video_capture/video_capture_factory.h"
#include "libyuv/convert_argb.h"
#include "rtc_base/stringutils.h"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"

#if defined(__APPLE__) && TARGET_OS_IPHONE
// iOS use nullptr for both video decoder_factory and video encoder_factory.
#elif defined(ANDROID)
#include "sdk/android/src/jni/androidmediadecoder_jni.h"
#include "sdk/android/src/jni/androidmediaencoder_jni.h"
#endif

// Names used for a IceCandidate JSON object.
static const char kCandidateSdpMidName[] = "sdpMid";
static const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
static const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
static const char kSessionDescriptionTypeName[] = "type";
static const char kSessionDescriptionSdpName[] = "sdpOffer";

std::string trtc_client::ice_connection_state_str(webrtc::PeerConnectionInterface::IceConnectionState state)
{
	switch (state) {
	case webrtc::PeerConnectionInterface::kIceConnectionNew:
		return "NEW";
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
		return "CHECKING";
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
		return "CONNECTED";
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
		return "COMPLETED";
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
		return "FAILED";
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
		return "DISCONNECTED";
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
		return "CLOSED";
    case webrtc::PeerConnectionInterface::kIceConnectionMax:
		return "COUNT";
	}
	return "UNKNOWN";
}

std::string trtc_client::ice_gathering_state_str(webrtc::PeerConnectionInterface::IceGatheringState state)
{
	switch (state) {
    case webrtc::PeerConnectionInterface::kIceGatheringNew:
		return "NEW";
    case webrtc::PeerConnectionInterface::kIceGatheringGathering:
		return "GATHERING";
    case webrtc::PeerConnectionInterface::kIceGatheringComplete:
		return "COMPLETE";
	}
	return "UNKNOWN";
}

#define DTLS_ON  true
#define DTLS_OFF false

trtc_client::trtc_client(gui2::tdialog& dialog, const tpoint& desire_size, bool local_only, int desire_fps)
	: dialog_(dialog)
	, desire_size_(desire_size)
	, desire_fps_(desire_fps)
	, local_only_(local_only)
	, caller_(true)
	, resolver_(nullptr)
	, state_(NOT_CONNECTED)
	, my_id_(-1)
	, deconstructed_(false)
	, remote_pixels_(nullptr)
	, local_pixels_(nullptr)
	, last_msg_should_size_(-1)
	, recv_data_(nullptr)
	, recv_data_size_(0)
	, recv_data_vsize_(0)
	, send_data_(nullptr)
	, send_data_size_(0)
	, send_data_vsize_(0)
	, preferred_codec_(cricket::kH264CodecName) // cricket::kVp9CodecName
	, ref_count_(1)
{
	release_textures();

	VALIDATE(desire_size_.x == nposm || desire_size_.x >= 640, null_str);
	VALIDATE(desire_size_.y == nposm || desire_size_.y >= 480, null_str);

	if (desire_size_.x != nposm && desire_size_.y != nposm) {
		// desire_size must be landscape size.
		VALIDATE(desire_size.x >= desire_size.y, null_str);
	}

	VALIDATE(instance->rtc_client() == nullptr, null_str);
	instance->set_rtc_client(this);
}

trtc_client::~trtc_client()
{
	if (state_ == CONNECTED) {
		if (peer_connection_.get()) {
			DeletePeerConnection();
			if (!local_only_) {
				// {"id":"stop"}
				Json::StyledWriter writer;
				Json::Value jmessage;

				jmessage["id"] = "stop";
				msg_2_signaling_server(control_socket_.get(), writer.write(jmessage));
			}
		}
		Close();
	}

	if (recv_data_) {
		free(recv_data_);
		recv_data_ = nullptr;
	}
	if (send_data_) {
		free(send_data_);
		send_data_ = nullptr;
	}

	VALIDATE(instance->rtc_client(), null_str);
	instance->set_rtc_client(nullptr);
}

trtc_client::VideoRenderer::VideoRenderer(trtc_client& client, int width, int height, webrtc::VideoTrackInterface* track_to_render, bool remote)
	: client_(client)
	, rendered_track_(track_to_render)
	, remote_(remote)
	, new_frame_(false)
	, app_width_(nposm)
	, app_height_(nposm)
	, bytesperpixel_(4) // ARGB
	, frames_(0)
{
	rtc::VideoSinkWants wants;
	wants.rotation_applied = false;
	rendered_track_->AddOrUpdateSink(this, wants);
}

trtc_client::VideoRenderer::~VideoRenderer()
{
	rendered_track_->RemoveSink(this);
}

// #include "image.hpp"

void trtc_client::VideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame)
{
	if (!client_.pixels(remote_)) {
		// must execute before lock sink_mutex, because main thread maybe waiting for sink_mutex. 
		// for exmaple, did_draw_slice_.
		instance->sdl_thread().Invoke<void>(RTC_FROM_HERE, rtc::Bind(&trtc_client::set_renderer_texture_size, &client_, remote_, video_frame.width(), video_frame.height()));
	}

	threading::lock lock2(client_.get_sink_mutex(remote_));

	if (client_.deconstructed_) {
		// PeerConnection was deleted when locking mutex.
		return;
	}

	uint8_t* pixels = client_.pixels(remote_);
	if (!pixels) {
		// maybe in change rotation
		return;
	}
	VALIDATE(app_width_ != nposm && app_height_ != nposm, null_str);

	// const webrtc::VideoFrame frame(webrtc::I420Buffer::Rotate(video_frame.video_frame_buffer(), video_frame.rotation()),
	//	webrtc::kVideoRotation_0, video_frame.timestamp_us());
	rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(video_frame.video_frame_buffer()->ToI420());

	if ((frames_ % 20) == 0) {
		SDL_Log("OnFrame, %s, size(%i x %i), frames(%i), rotation(%i)\n", remote_? "remote": "local", video_frame.width(), video_frame.height(), frames_, video_frame.rotation());
	}

	if (video_frame.rotation() != webrtc::kVideoRotation_0) {
		// buffer = webrtc::I420Buffer::Rotate(*buffer, video_frame.rotation());
	}	
	if (buffer->width() != app_width_) {
		buffer = webrtc::I420Buffer::Rotate(*buffer, webrtc::kVideoRotation_90);
	}

	int pitch = buffer->width() * bytesperpixel_;

	libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(),
        buffer->DataU(), buffer->StrideU(),
        buffer->DataV(), buffer->StrideV(),
        pixels,
		pitch,
        buffer->width(), buffer->height());

	new_frame_ = true;
	frames_ ++;
}

void trtc_client::draw_slice(SDL_Renderer* renderer, bool remote, const SDL_Rect& draw_rect, bool force_draw)
{
	tavcapture::VideoRenderer* sink = vrenderer(remote);
	if (!sink) {
		// local sink always valid, but remote sink not.
		return;
	}

	bool new_frame = sink->new_frame();

	if (force_draw || new_frame) {
		threading::lock lock(get_sink_mutex(remote));

		if (!sink->frames()) {
			return;
		}

		texture& frame_tex = frame_texture(remote);
		texture& cv_tex = cv_texture(remote);

		if (new_frame) {
			SDL_UnlockTexture(frame_tex.get());
		}

		if (did_draw_slice_) {
			ttexture_2_mat_lock lock2(frame_tex);
			did_draw_slice_(remote, lock2.mat, frame_tex, draw_rect, new_frame, cv_tex);

		} else {
			SDL_RenderCopy(renderer, frame_tex.get(), nullptr, &draw_rect);
		}

		sink->clear_new_frame();
	}
}

void trtc_client::set_renderer_texture_size(bool remote, int width, int height)
{
	SDL_Renderer* renderer = get_renderer();
	if (height > width) {
		int tmp = width;
		width = height;
		height = tmp;
	}

	bool landscape = game_config::mobile? gui2::twidget::current_landscape: true;
	if (game_config::mobile && !gui2::twidget::should_conside_orientation(gui2::settings::screen_width, gui2::settings::screen_height)) {
		landscape = true;
	}
	// calculate app_width, app_height
	const int app_video_width = landscape? width: height;
	const int app_video_height = landscape? height: width;

	// frame texture
	texture& tex = remote? remote_tex_: local_tex_;
	uint8_t** pixels = remote? &remote_pixels_: &local_pixels_;
	texture& cv_tex = remote? remote_cv_tex_: local_cv_tex_;

	VALIDATE(tex.get() == nullptr && *pixels == nullptr && cv_tex.get() == nullptr, null_str);

	tex = SDL_CreateTexture(renderer, get_screen_format().format, SDL_TEXTUREACCESS_STREAMING, app_video_width, app_video_height);
	// some platform, SDL_TEXTUREACCESS_STREAMING isn't SupportedFormat, for example opengl.
	// on those platform, allocated pixels maybe 4 bytes as if we want 3 bytes!
	// tex = SDL_CreateTexture(get_renderer(), SDL_TEXTUREACCESS_STREAMING, SDL_TEXTUREACCESS_STREAMING, width, height);

	int pitch = 0;
	SDL_LockTexture(tex.get(), NULL, (void**)pixels, &pitch);

	// opencv texture
	cv_tex = SDL_CreateTexture(renderer, get_screen_format().format, SDL_TEXTUREACCESS_STREAMING, app_video_width, app_video_height);

	vrenderer(remote)->set_app_size(app_video_width, app_video_height);

	SDL_Log("set_renderer_texture_size(%s) is called\n", remote? "remote": "local");
}

void trtc_client::release_textures()
{
	remote_tex_.reset(nullptr);
	local_tex_.reset(nullptr);

	remote_pixels_ = nullptr;
	local_pixels_ = nullptr;

	remote_cv_tex_.reset(nullptr);
	local_cv_tex_.reset(nullptr);

	if (remote_renderer_) {
		remote_renderer_->set_app_size(nposm, nposm);
	}
	if (local_renderer_) {
		local_renderer_->set_app_size(nposm, nposm);
	}
}

void trtc_client::pre_create_renderer()
{
	SDL_Log("pre_create_renderer is called\n");

	threading::lock lock1(remote_sink_mutex_);
	threading::lock lock2(local_sink_mutex_);
	release_textures();
}

void trtc_client::post_create_renderer()
{
	SDL_Log("post_create_renderer is called\n");

	VALIDATE(remote_tex_.get() == nullptr && local_tex_.get() == nullptr, null_str);
}

const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const char kStreamLabel[] = "stream_label";
const uint16_t kDefaultServerPort = 8888;

std::string GetStunConnectionString()
{
	return "stun:133.130.113.73:3478";
}

std::string GetTurnConnectionString()
{
	return "turn:test@133.130.113.73:3478";
}

// Updates the original SDP description to instead prefer the specified video
// codec. We do this by placing the specified codec at the beginning of the
// codec list if it exists in the sdp.
std::string trtc_client::webrtc_set_preferred_codec(const std::string& sdp, const std::string& codec)
{
	std::string result;

	// a=rtpmap:126 H264/90000
	const std::string codec2 = std::string(" ") + codec + "/";
	const std::string lineSeparator = "\n";
	const std::string mLineSeparator = " ";

	// Copied from PeerConnectionClient.java.
	// TODO(tkchin): Move this to a shared C++ file.
	const char* mvideo = strstr(sdp.c_str(), "\nm=video ");
	if (!mvideo) {
		// No m=video line, so can't prefer @codec
		return sdp;
	}
	mvideo += 1;
	const std::string prefix = "a=rtpmap:";
	std::string line, payload;
	const char* rtpmap = strstr(mvideo, prefix.c_str());
	const char* until;
	while (rtpmap) {
		until = rtpmap + prefix.size();
		while (*until != '\r' && *until != '\n') {
			until ++;
		}
		line.assign(rtpmap, until - rtpmap);
		if (line.find(codec2) != std::string::npos) {
			payload.assign(rtpmap + prefix.size(), strchr(rtpmap, ' ') - rtpmap - prefix.size());
			break;
		}
		rtpmap = until;
		rtpmap = strstr(rtpmap, prefix.c_str());
	}
	if (payload.empty()) {
		return sdp;
	}
	line.assign(mvideo, strchr(mvideo, '\n') - mvideo);
	std::vector<std::string> vstr = utils::split(line, ' ', utils::REMOVE_EMPTY);
	// m=video 9 UDP/TLS/RTP/SAVPF 126 120 97
	std::vector<std::string>::iterator it = std::find(vstr.begin(), vstr.end(), payload);
	const size_t require_index = 3;
	size_t index = std::distance(vstr.begin(), it);
	if (index == require_index) {
		return sdp;
	}
	vstr.erase(it);
	it = vstr.begin();
	std::advance(it, require_index);
	vstr.insert(it, payload);

	result = sdp.substr(0, mvideo - sdp.c_str());
	result += utils::join(vstr, " ");
	result += sdp.substr(mvideo - sdp.c_str() + line.size());
	
	return result;
}

bool trtc_client::InitializePeerConnection()
{
	deconstructed_ = false;
	relay_only_ = app_relay_only();
	release_textures();

	connection_state_ = webrtc::PeerConnectionInterface::kIceConnectionNew;
	gathering_state_ = webrtc::PeerConnectionInterface::kIceGatheringNew;

	VALIDATE(peer_connection_factory_.get() == NULL, null_str);
	VALIDATE(peer_connection_.get() == NULL, null_str);
	RTC_CHECK(_networkThread.get() == NULL);
	RTC_CHECK(_workerThread.get() == NULL);
	RTC_CHECK(_signalingThread.get() == NULL);

	_networkThread = rtc::Thread::CreateWithSocketServer();
    bool result = _networkThread->Start();
    RTC_CHECK(result);

    _workerThread = rtc::Thread::Create();
    result = _workerThread->Start();
    RTC_CHECK(result);

    rtc::Thread* signalingThread;
// #ifdef _WIN32
//	signalingThread = rtc::Thread::Current();
// #else
    _signalingThread = rtc::Thread::Create();
    result = _signalingThread->Start();
    signalingThread = _signalingThread.get();
// #endif
    RTC_CHECK(result);

	cricket::WebRtcVideoEncoderFactory* encoder_factory = NULL;
	cricket::WebRtcVideoDecoderFactory* decoder_factory = NULL;
#if defined(__APPLE__) && TARGET_OS_IPHONE

#elif defined(ANDROID)
	encoder_factory = new webrtc::jni::MediaCodecVideoEncoderFactory();
    decoder_factory = new webrtc::jni::MediaCodecVideoDecoderFactory();
#endif

	peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        _networkThread.get(), _workerThread.get(), signalingThread,
        nullptr, 
		webrtc::CreateBuiltinAudioEncoderFactory(),
		webrtc::CreateBuiltinAudioDecoderFactory(),
		encoder_factory, decoder_factory);

	// peer_connection_factory_ = webrtc::CreatePeerConnectionFactory();

	if (!peer_connection_factory_.get()) {
		DeletePeerConnection();
		return false;
	}

	if (!CreatePeerConnection(DTLS_ON)) {
		DeletePeerConnection();
	}
	AddStreams();

	disable_idle_lock_.reset(new tdisable_idle_lock);
	return peer_connection_.get() != NULL;
}

bool trtc_client::CreatePeerConnection(bool dtls) 
{
	VALIDATE(peer_connection_factory_.get() != NULL, null_str);
	VALIDATE(peer_connection_.get() == NULL, null_str);

	webrtc::PeerConnectionInterface::RTCConfiguration config;
	webrtc::PeerConnectionInterface::IceServer server;
	// server.uri = GetStunConnectionString();
	server.uri = GetTurnConnectionString();
	server.username = "test";
	server.password = "123";
	// server.password = "123456";
	config.servers.push_back(server);

	webrtc::FakeConstraints constraints;
	if (dtls) {
		constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
			"true");
	} else {
		constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
			"false");
	}

	peer_connection_ = peer_connection_factory_->CreatePeerConnection(
		config, &constraints, NULL, NULL, this);
	return peer_connection_.get() != NULL;
}

void trtc_client::DeletePeerConnection() 
{
	// syncnith with frame thread.
	{
		// why release local_sink_mutex_/remote_sink_mutex_ after set deconstructed_ = true?
		// ---below StopLocalRender/StopRemoteRender require frame thread exit.
		threading::lock lock1(local_sink_mutex_);
		threading::lock lock2(remote_sink_mutex_);
		deconstructed_ = true;
	}

	if (peer_connection_.get()) {
		state_ = NOT_CONNECTED;
		peer_connection_->Close();
	} else {
		VALIDATE(state_ != CONNECTED, null_str);
	}

	peer_connection_ = NULL;
	active_streams_.clear();
	StopLocalRenderer();
	StopRemoteRenderer();
	peer_connection_factory_ = NULL;

	_networkThread.reset();
	_workerThread.reset();
	_signalingThread.reset();

	disable_idle_lock_.reset();
}

void trtc_client::ConnectToPeer(const std::string& peer_nick, bool caller, const std::string& offer)
{
	VALIDATE(!peer_nick.empty(), null_str);

	if (peer_connection_.get()) {
		// main_wnd_->MessageBox("Error", "We only support connecting to one peer at a time", true);
		return;
	}

	if (InitializePeerConnection()) {
		if (caller) {
			peer_connection_->CreateOffer(this, NULL);

		} else {
			// Replace message type from "offer" to "answer"
			const std::string offer2 = webrtc_set_preferred_codec(offer, preferred_codec_);
			webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription("offer", offer2, nullptr));
			peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), session_description);
			peer_connection_->CreateAnswer(this, NULL);
		}
	} else {
		// main_wnd_->MessageBox("Error", "Failed to initialize PeerConnection", true);
	}
}

std::unique_ptr<cricket::VideoCapturer> trtc_client::OpenVideoCaptureDevice() 
{
	std::vector<std::string> device_names;
	{
		std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
			webrtc::VideoCaptureFactory::CreateDeviceInfo());
		if (!info) {
			return nullptr;
		}
		int num_devices = info->NumberOfDevices();
		for (int i = 0; i < num_devices; ++i) {
			const uint32_t kSize = 256;
			char name[kSize] = { 0 };
			char id[kSize] = { 0 };
			if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) {
				device_names.push_back(name);
			}
		}
	}

	cricket::WebRtcVideoDeviceCapturerFactory factory;
	std::unique_ptr<cricket::VideoCapturer> capturer;
	for (const auto& name : device_names) {
		capturer = factory.Create(cricket::Device(name, 0));
		if (capturer) {
			break;
		}
	}
	return capturer;
}

void trtc_client::AddStreams() 
{
	if (active_streams_.find(kStreamLabel) != active_streams_.end()) {
		return;  // Already added.
	}

	rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
	audio_track = peer_connection_factory_->CreateAudioTrack(
			kAudioLabel, peer_connection_factory_->CreateAudioSource(NULL));

	webrtc::FakeConstraints video_constraints;
	if (desire_fps_ != nposm) {
		VALIDATE(desire_fps_ >= 1, null_str);
		video_constraints.AddOptional(webrtc::MediaConstraintsInterface::kMaxFrameRate, str_cast(desire_fps_));
	}
	if (desire_size_.x != nposm) {
		video_constraints.AddOptional(webrtc::MediaConstraintsInterface::kMinWidth, str_cast(desire_size_.x));
	}
	if (desire_size_.y != nposm) {
		video_constraints.AddOptional(webrtc::MediaConstraintsInterface::kMinHeight, str_cast(desire_size_.y));
	}

	// although support set framt rate use kMinFrameRate, kMaxFrameRate, but some os not support in fact. include windows(directshow), iOS.
	// in order to cross-platform, caller don't set frame rate.
	// video_constraints.AddOptional(webrtc::MediaConstraintsInterface::kMaxFrameRate, "7");

	rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
		peer_connection_factory_->CreateVideoTrack(
			kVideoLabel,
			peer_connection_factory_->CreateVideoSource(OpenVideoCaptureDevice(),
				&video_constraints)));

	VALIDATE(video_track.get(), "create video track fail.");
/*
	webrtc::VideoTrackSourceInterface* source = video_track->GetSource();
	webrtc::VideoTrackSourceInterface::Stats stats;
	source->GetStats(&stats);
*/
	StartLocalRenderer(video_track);

	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
		peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);

	if (audio_track.get()) {
		stream->AddTrack(audio_track);
	}
	stream->AddTrack(video_track);
	if (!peer_connection_->AddStream(stream)) {
		RTC_LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
	}
	typedef std::pair<std::string,
		rtc::scoped_refptr<webrtc::MediaStreamInterface> >
		MediaStreamPair;
	active_streams_.insert(MediaStreamPair(stream->label(), stream));
}

//
// PeerConnectionObserver implementation.
//

void trtc_client::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
{
	// thread context: _signalingThread
	if (new_state == webrtc::PeerConnectionInterface::kClosed) {
	}
}

// Called when a remote stream is added
void trtc_client::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) 
{
	// thread context: _signalingThread
	RTC_LOG(INFO) << __FUNCTION__ << " " << stream->label();

	webrtc::VideoTrackVector tracks = stream->GetVideoTracks();
	// Only render the first track.
	if (!tracks.empty()) {
		webrtc::VideoTrackInterface* track = tracks[0];
		StartRemoteRenderer(track);
		// once release, this MediaStream will not be ~MediaStream!
		// stream.release();
	}
}

void trtc_client::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) 
{
	// thread context: _signalingThread
	RTC_LOG(INFO) << __FUNCTION__ << " " << stream->label();
}

void trtc_client::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
	// thread context: _signalingThread
	connection_state_ = new_state;
	app_OnIceConnectionChange();
}

void trtc_client::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
	// thread context: _signalingThread
	gathering_state_ = new_state;
	app_OnIceGatheringChange();
}

void trtc_client::OnIceConnectionReceivingChange(bool receiving)
{
	// thread context: _signalingThread
}

void trtc_client::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) 
{
	// thread context: _signalingThread
	const std::string& type = candidate->candidate().type();
	if (relay_only_ && type != cricket::RELAY_PORT_TYPE) {
		return;
	}
	
	std::string sdp;
	if (!candidate->ToString(&sdp)) {
		RTC_LOG(LS_ERROR) << __FUNCTION__ << " Failed to serialize candidate";
		return;
	}

	RTC_LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index() << " " << sdp;

	const std::string sdp_mid = candidate->sdp_mid();
	const int sdp_mline_index = candidate->sdp_mline_index();

	Json::StyledWriter writer;
	Json::Value jmessage, jcandidate;

	jcandidate[kCandidateSdpMidName] = candidate->sdp_mid();
	jcandidate[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
	jcandidate["candidate"] = sdp;

	jmessage["id"] = "onIceCandidate";
	jmessage["candidate"] = jcandidate;

	msg_2_signaling_server(control_socket_.get(), writer.write(jmessage));
}

#include <webrtc/pc/mediasession.h>

void trtc_client::OnSuccess(webrtc::SessionDescriptionInterface* desc) 
{
	const cricket::SessionDescription& desc2 = *desc->description();
	const cricket::ContentInfo* voice = cricket::GetFirstAudioContent(&desc2);
	const cricket::ContentInfo* video = cricket::GetFirstVideoContent(&desc2);
	const cricket::ContentInfo* data = cricket::GetFirstDataContent(&desc2);

	if (local_only_) {
		// if avcapture, don't call SetLocalDescription, don't start local network transport logic.
		return;
	}

	// thread context: _signalingThread
	peer_connection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), desc);

	std::string sdp;
	desc->ToString(&sdp);

	Json::StyledWriter writer;
	Json::Value jmessage;

	if (caller_) {
		jmessage["id"] = "call";
		jmessage["from"] = my_nick_;
		jmessage["to"] = peer_nick_;
		jmessage[kSessionDescriptionSdpName] = sdp;
	} else {
		jmessage["id"] = "incomingCallResponse";
		jmessage["from"] = peer_nick_;
		jmessage["callResponse"] = "accept";
		jmessage["sdpAnswer"] = sdp;
	}

	msg_2_signaling_server(control_socket_.get(), writer.write(jmessage));
}

void trtc_client::OnFailure(const std::string& error) 
{
	// thread context: _signalingThread
	RTC_LOG(LERROR) << error;
}

//
// PeerConnectionClientObserver implementation.
//

void trtc_client::OnServerConnectionFailure()
{
	RTC_LOG(INFO) << "Failed to connect to " << server_;
}

rtc::AsyncSocket* CreateClientSocket(int family) 
{
	rtc::Thread* thread = rtc::Thread::Current();
	VALIDATE(thread != NULL, null_str);
	return thread->socketserver()->CreateAsyncSocket(family, SOCK_STREAM);
}

void trtc_client::InitSocketSignals() 
{
	VALIDATE(control_socket_.get() != NULL, null_str);

	control_socket_->SignalCloseEvent.connect(this, &trtc_client::OnClose);
	control_socket_->SignalConnectEvent.connect(this, &trtc_client::OnConnect);
	control_socket_->SignalReadEvent.connect(this, &trtc_client::OnRead);
}

void trtc_client::Connect(const std::string& server, int port) 
{
	VALIDATE(!server.empty(), null_str);

	if (state_ != NOT_CONNECTED) {
		RTC_LOG(WARNING)
			<< "The client must not be connected before you can call Connect()";
		OnServerConnectionFailure();
		return;
	}

	if (server.empty()) {
		OnServerConnectionFailure();
		return;
	}

	if (port <= 0) {
		port = kDefaultServerPort;
	}

	server_address_.SetIP(server);
	server_address_.SetPort(port);

	if (server_address_.IsUnresolvedIP()) {
		state_ = RESOLVING;
		resolver_ = new rtc::AsyncResolver();
		resolver_->SignalDone.connect(this, &trtc_client::OnResolveResult);
		resolver_->Start(server_address_);
	} else {
		DoConnect();
	}
}

void trtc_client::OnResolveResult(rtc::AsyncResolverInterface* resolver) 
{
	if (resolver_->GetError() != 0) {
		OnServerConnectionFailure();
		resolver_->Destroy(false);
		resolver_ = NULL;
		state_ = NOT_CONNECTED;
	} else {
		server_address_ = resolver_->address();
		DoConnect();
	}
}

void trtc_client::DoConnect() 
{
	control_socket_.reset(CreateClientSocket(server_address_.ipaddr().family()));
	InitSocketSignals();

	bool ret = ConnectControlSocket();
	if (ret) {
		state_ = SIGNING_IN;
	} else {
		OnServerConnectionFailure();
	}
}

void trtc_client::Close() 
{
	if (!local_only_) {
		control_socket_->Close();
	}
	if (resolver_ != NULL) {
		resolver_->Destroy(false);
		resolver_ = NULL;
	}
	my_id_ = -1;
	state_ = NOT_CONNECTED;
}

bool trtc_client::ConnectControlSocket() 
{
	VALIDATE(control_socket_->GetState() == rtc::Socket::CS_CLOSED, null_str);
	int err = control_socket_->Connect(server_address_);
	if (err == SOCKET_ERROR) {
		Close();
		return false;
	}
	return true;
}

void trtc_client::OnConnect(rtc::AsyncSocket* socket) 
{
	Json::StyledWriter writer;
	Json::Value jmessage;

	jmessage["id"] = "register";
	jmessage["name"] = my_nick_;

	msg_2_signaling_server(socket, writer.write(jmessage));
}

void trtc_client::msg_2_signaling_server(rtc::AsyncSocket* socket, const std::string& msg)
{
	const int prefix_bytes = 2;

	VALIDATE(!msg.empty(), null_str);

	resize_send_data(prefix_bytes + msg.size());
	unsigned short n = SDL_SwapBE16((uint16_t)msg.size());
	memcpy(send_data_, &n, sizeof(short));
	memcpy(send_data_ + 2, msg.c_str(), msg.size());

	size_t sent = socket->Send(send_data_, prefix_bytes + msg.size());
	VALIDATE(sent == prefix_bytes + msg.length(), null_str);
	// RTC_UNUSED(sent);
}

// every call at least return one message, as if exist more message in this triger.
// caller require use while to call it.
const char* trtc_client::read_2_buffer(rtc::AsyncSocket* socket, bool first, size_t* content_length)
{
	const int prefix_bytes = 2;
	const int chunk_size = 1024;
	int ret_size = 0;

	if (last_msg_should_size_ != -1 && recv_data_vsize_ >= prefix_bytes + last_msg_should_size_) {
		if (recv_data_vsize_ > prefix_bytes + last_msg_should_size_) {
			memmove(recv_data_, recv_data_ + prefix_bytes + last_msg_should_size_, recv_data_vsize_ - prefix_bytes - last_msg_should_size_);
		}
		recv_data_vsize_ -= prefix_bytes + last_msg_should_size_;
		last_msg_should_size_ = -1;
	}

	if (!first) {
		if (recv_data_vsize_ == 0) {
			return NULL;
		} else if (last_msg_should_size_ == -1 && recv_data_vsize_ >= prefix_bytes) {
			last_msg_should_size_ = SDL_SwapBE16(*(reinterpret_cast<uint16_t*>(recv_data_)));
			if (recv_data_vsize_ >= prefix_bytes + last_msg_should_size_) {
				// if residue bytes is enough to one message, return directory.
				*content_length = last_msg_should_size_;
				return recv_data_ + prefix_bytes;
			}
		}
	}

	do {
		if (recv_data_size_ < recv_data_vsize_ + chunk_size) {
			resize_recv_data(recv_data_size_ + chunk_size);
		}
		ret_size = socket->Recv(recv_data_ + recv_data_vsize_, chunk_size, nullptr);
		if (ret_size <= 0) {
			break;
		}
		recv_data_vsize_ += ret_size;
		if (last_msg_should_size_ == -1 && recv_data_vsize_ >= prefix_bytes) {
			last_msg_should_size_ = SDL_SwapBE16(*(reinterpret_cast<uint16_t*>(recv_data_)));
		}
	} while (ret_size == chunk_size);

	if (last_msg_should_size_ == -1 || recv_data_vsize_ < prefix_bytes + last_msg_should_size_) {
		return NULL;
	}

	*content_length = last_msg_should_size_;
	return recv_data_ + prefix_bytes;
}

void trtc_client::OnClose(rtc::AsyncSocket* socket, int err) 
{
	RTC_LOG(INFO) << __FUNCTION__;

	// must call it, for example remove it from ss_
	socket->Close();

	rtc::Thread* thread = rtc::Thread::Current();
	thread->Post(RTC_FROM_HERE, &dialog_, MSG_SIGNALING_CLOSE, NULL);
}

bool trtc_client::chat_OnMessage(rtc::Message* msg)
{
	if (msg->message_id == MSG_SIGNALING_CLOSE) {
		DeletePeerConnection();
		Close();
		return true;
	}
	return false;
}

void trtc_client::StartLocalRenderer(webrtc::VideoTrackInterface* local_video)
{
	local_renderer_.reset(new VideoRenderer(*this, 1, 1, local_video, false));
}

void trtc_client::StopLocalRenderer()
{
	local_renderer_.reset();
}

void trtc_client::StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video) 
{
	remote_renderer_.reset(new VideoRenderer(*this, 1, 1, remote_video, true));
}

void trtc_client::StopRemoteRenderer()
{
	remote_renderer_.reset();
}

void trtc_client::resize_recv_data(int size)
{
	size = posix_align_ceil(size, 4096);
	if (size > recv_data_size_) {
		char* tmp = (char*)malloc(size);
		if (recv_data_) {
			if (recv_data_vsize_) {
				memcpy(tmp, recv_data_, recv_data_vsize_);
			}
			free(recv_data_);
		}
		recv_data_ = tmp;
		recv_data_size_ = size;
	}
}

void trtc_client::resize_send_data(int size)
{
	size = posix_align_ceil(size, 4096);
	if (size > send_data_size_) {
		char* tmp = (char*)malloc(size);
		if (send_data_) {
			if (send_data_vsize_) {
				memcpy(tmp, send_data_, send_data_vsize_);
			}
			free(send_data_);
		}
		send_data_ = tmp;
		send_data_size_ = size;
	}
}
/*
void trtc_client::AddRef() const 
{ 
	ref_count_.IncRef(); 
}

RefCountReleaseStatus trtc_client::Release() const 
{
    const auto status = ref_count_.DecRef();
    if (status == RefCountReleaseStatus::kDroppedLastRef) {
      delete this;
    }
    return status;
}
*/


tavcapture::tavcapture(gui2::tdialog& dialog, const tpoint& desire_size, const int desire_fps)
	: trtc_client(dialog, desire_size, true, desire_fps)
{
	caller_ = true;
	state_ = CONNECTED;
	ConnectToPeer("fake", true, null_str);
}