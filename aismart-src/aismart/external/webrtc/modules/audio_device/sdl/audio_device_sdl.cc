/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/sdl/audio_device_sdl.h"

#include "rtc_base/atomicops.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "modules/audio_device/fine_audio_buffer.h"

#include "filesystem.hpp"
#include "rose_config.hpp"

namespace webrtc {

#define LOGI() RTC_LOG(LS_INFO) << "AudioDeviceSDL::"

// Hardcoded delay estimates based on real measurements.
// TODO(henrika): these value is not used in combination with built-in AEC.
// Can most likely be removed.
const uint16_t kFixedPlayoutDelayEstimate = 30;
const uint16_t kFixedRecordDelayEstimate = 30;

const uint32_t N_REC_SAMPLES_PER_SEC = 48000;
const uint32_t N_PLAY_SAMPLES_PER_SEC = 48000;

const uint32_t N_REC_CHANNELS = 1;  // default is mono recording
const uint32_t N_PLAY_CHANNELS = 2; // default is stereo playout

// NOTE - CPU load will not be correct for other sizes than 10ms
const uint32_t REC_BUF_SIZE_IN_SAMPLES = (N_REC_SAMPLES_PER_SEC/100);
const uint32_t PLAY_BUF_SIZE_IN_SAMPLES = (N_PLAY_SAMPLES_PER_SEC/100);

AudioDeviceSDL::AudioDeviceSDL()
	: id_(0)
	, rec_channels_(N_REC_CHANNELS)
	, play_channels_(N_PLAY_CHANNELS)
	, audio_device_buffer_(nullptr)
	, recording_(0)
	, playing_(0)
	, initialized_(false)
	, rec_is_initialized_(false)
	, play_is_initialized_(false)
	, is_interrupted_(false)
	, has_configured_session_(false)
	, capture_audio_id_(0)
	, playout_cached_buffer_start_(0)
	, playout_cached_bytes_(0)
	, play_bytes_per_10_ms_(0)
    , required_record_buffer_size_bytes_(0)
    , record_cached_bytes_(0)
    , record_read_pos_(0)
    , record_write_pos_(0)
	, rec_bytes_per_10_ms_(0)
{
	thread_ = rtc::Thread::Current();
	LOGI() << "ctor, " << thread_->name();
}

AudioDeviceSDL::~AudioDeviceSDL() 
{
	LOGI() << "~dtor, " << thread_->name().c_str();

	RTC_CHECK(thread_checker_.CalledOnValidThread());
	Terminate();
}

void AudioDeviceSDL::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{
	LOGI() << "AttachAudioBuffer, " << thread_->name().c_str();

	audio_device_buffer_ = audioBuffer;

	// inform the AudioBuffer about default settings for this implementation
	audio_device_buffer_->SetRecordingSampleRate(N_REC_SAMPLES_PER_SEC);
	audio_device_buffer_->SetPlayoutSampleRate(N_PLAY_SAMPLES_PER_SEC);
    audio_device_buffer_->SetRecordingChannels(N_REC_CHANNELS);
	audio_device_buffer_->SetPlayoutChannels(N_PLAY_CHANNELS);
}

AudioDeviceGeneric::InitStatus AudioDeviceSDL::Init()
{
	LOGI() << "Init, " << thread_->name().c_str();
	if (initialized_) {
		return InitStatus::OK;
	}

	// Ensure that the audio device buffer (ADB) knows about the internal audio
	// parameters. Note that, even if we are unable to get a mono audio session,
	// we will always tell the I/O audio unit to do a channel format conversion
	// to guarantee mono on the "input side" of the audio unit.
	initialized_ = true;
	return InitStatus::OK;
}

int32_t AudioDeviceSDL::Terminate() 
{
	LOGI() << "Terminate, " << thread_->name().c_str();
	if (!initialized_) {
		return 0;
	}
	StopPlayout();
	StopRecording();
	initialized_ = false;
	return 0;
}

int32_t AudioDeviceSDL::InitPlayout() 
{
	LOGI() << "InitPlayout, " << thread_->name().c_str();
	RTC_CHECK(initialized_);
	RTC_CHECK(!play_is_initialized_);
	RTC_CHECK(!playing_);

	if (!play_is_initialized_) {
#ifdef ANDROID
		const int least_samples = 4096;
#else
		// iOS require at least 1024.
		const int least_samples = 1024;
#endif
		playout_frequency_lock_.reset(new sound::tfrequency_lock(N_PLAY_SAMPLES_PER_SEC, least_samples));
		play_bytes_per_10_ms_ = PLAY_BUF_SIZE_IN_SAMPLES * 2 * play_channels_;
		// WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, id_, "InitPlayout failed");
		// return -1;
	}
	play_is_initialized_ = true;
	return 0;
}

void did_capture_audio(void* context, uint8_t* stream, int len)
{
	AudioDeviceSDL* device = reinterpret_cast<AudioDeviceSDL*>(context);
	device->did_capture_audio(stream, len);
}

int32_t AudioDeviceSDL::InitRecording() 
{
	LOGI() << "InitRecording, " << thread_->name().c_str();
	if (!rec_is_initialized_) {
		SDL_AudioSpec desired, mixer;
		memset(&desired, 0, sizeof(SDL_AudioSpec));
		/* Set the desired format and frequency */
		desired.freq = N_REC_SAMPLES_PER_SEC;
		desired.format = AUDIO_S16LSB;
		desired.channels = rec_channels_;
		// desired.samples = 2 * rec_channels_ * REC_BUF_SIZE_IN_SAMPLES;
		desired.samples = 1;
		while (desired.samples < REC_BUF_SIZE_IN_SAMPLES) {
			desired.samples *= 2;
		}
		desired.callback = webrtc::did_capture_audio;
		desired.userdata = this;

		capture_audio_id_ = SDL_OpenAudioDevice(NULL, 1, &desired, &mixer, 0/*SDL_AUDIO_ALLOW_ANY_CHANGE*/);
		if (capture_audio_id_ == 0) {
			// WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, id_, "InitRecording failed");
            return -1;
		}
		rec_bytes_per_10_ms_ = REC_BUF_SIZE_IN_SAMPLES * 2 * rec_channels_;
		// Allocate extra space on the recording side to reduce the number of memmove() calls.
		required_record_buffer_size_bytes_ = 5 * (mixer.size + rec_bytes_per_10_ms_);
    }

	rec_is_initialized_ = true;
	return 0;
}

void get_mix_data(uint8_t* stream, int len, void* context)
{
	AudioDeviceSDL* device = reinterpret_cast<AudioDeviceSDL*>(context);
	device->OnGetPlayoutData(stream, len);
}

size_t AudioDeviceSDL::RequiredPlayoutBufferSizeBytes(const size_t desired_frame_size_bytes) 
{
	// It is possible that we store the desired frame size - 1 samples. Since new
	// audio frames are pulled in chunks of 10ms we will need a buffer that can
	// hold desired_frame_size - 1 + 10ms of data. We omit the - 1.
	return desired_frame_size_bytes + play_bytes_per_10_ms_;
}

void AudioDeviceSDL::ResetPlayout() 
{
	playout_cached_buffer_start_ = 0;
	playout_cached_bytes_ = 0;
	playout_cache_buffer_.reset(new int8_t[play_bytes_per_10_ms_]);
	playout_audio_buffer_.reset(new int8_t[RequiredPlayoutBufferSizeBytes(sound::get_buffer_size() * 2 * play_channels_)]);
}

int32_t AudioDeviceSDL::StartPlayout() 
{
	LOGI() << "StartPlayout, " << thread_->name().c_str();
	RTC_CHECK(play_is_initialized_);
	RTC_CHECK(!playing_);

	ResetPlayout();
	hook_get_mix_data_.reset(new sound::thook_get_mix_data(get_mix_data, this));
	SDL_PauseAudio(0);
	playing_ = true;
	return 0;
}

int32_t AudioDeviceSDL::StopPlayout()
{
	LOGI() << "StopPlayout, " << thread_->name().c_str();
	if (!play_is_initialized_) {
		return 0;
	}
	if (!playing_) {
		play_is_initialized_ = false;
		return 0;
	}
	hook_get_mix_data_.reset();
	playout_frequency_lock_.reset();
	playout_cache_buffer_.reset();
	playout_audio_buffer_.reset();

	play_is_initialized_ = false;
	playing_ = false;
	return 0;
}

void AudioDeviceSDL::ResetRecord() 
{
	record_cached_bytes_ = 0;
	record_read_pos_ = 0;
	record_write_pos_ = 0;
	RTC_CHECK(required_record_buffer_size_bytes_ > rec_bytes_per_10_ms_);
	record_cache_buffer_.reset(new int8_t[required_record_buffer_size_bytes_]);
}

int32_t AudioDeviceSDL::StartRecording()
{
	LOGI() << "StartREcording, " << thread_->name().c_str();
	RTC_CHECK(rec_is_initialized_);
	RTC_CHECK(!recording_);

	ResetRecord();
	SDL_PauseAudioDevice(capture_audio_id_, 0);
	recording_ = true;
	return 0;
}

int32_t AudioDeviceSDL::StopRecording() 
{
	LOGI() << "StopRecording, " << thread_->name().c_str();
	if (!rec_is_initialized_) {
		return 0;
	}
	if (!recording_) {
		rec_is_initialized_ = false;
		return 0;
	}
	SDL_CloseAudioDevice(capture_audio_id_);

	record_cache_buffer_.reset();
	capture_audio_id_ = 0;
	rec_is_initialized_ = false;
	recording_ = false;
	return 0;
}

int32_t AudioDeviceSDL::PlayoutDelay(uint16_t& delayMS) const {
  delayMS = kFixedPlayoutDelayEstimate;
  return 0;
}


void AudioDeviceSDL::DeliverRecordedData(const int8_t* buffer,
                                          size_t size_in_bytes,
                                          int playout_delay_ms,
                                          int record_delay_ms) 
{
	// it copy from FineAudioBuffer::DeliverRecordedData in fine_audio_buffer.cc
	const size_t samples_per_10_ms_ = REC_BUF_SIZE_IN_SAMPLES;

  // Check if the temporary buffer can store the incoming buffer. If not,
  // move the remaining (old) bytes to the beginning of the temporary buffer
  // and start adding new samples after the old samples.
  if (record_write_pos_ + size_in_bytes > required_record_buffer_size_bytes_) {
    if (record_cached_bytes_ > 0) {
      memmove(record_cache_buffer_.get(),
              record_cache_buffer_.get() + record_read_pos_,
              record_cached_bytes_);
    }
    record_write_pos_ = record_cached_bytes_;
    record_read_pos_ = 0;
  }
  // Add recorded samples to a temporary buffer.
  memcpy(record_cache_buffer_.get() + record_write_pos_, buffer, size_in_bytes);
  record_write_pos_ += size_in_bytes;
  record_cached_bytes_ += size_in_bytes;
  // Consume samples in temporary buffer in chunks of 10ms until there is not
  // enough data left. The number of remaining bytes in the cache is given by
  // |record_cached_bytes_| after this while loop is done.
  while (record_cached_bytes_ >= rec_bytes_per_10_ms_) {
    audio_device_buffer_->SetRecordedBuffer(
        record_cache_buffer_.get() + record_read_pos_, samples_per_10_ms_);
    audio_device_buffer_->SetVQEData(playout_delay_ms, record_delay_ms, 0);
    audio_device_buffer_->DeliverRecordedData();
    // Read next chunk of 10ms data.
    record_read_pos_ += rec_bytes_per_10_ms_;
    // Reduce number of cached bytes with the consumed amount.
    record_cached_bytes_ -= rec_bytes_per_10_ms_;
  }
}

void AudioDeviceSDL::did_capture_audio(uint8_t* data, int len)
{
	DeliverRecordedData((int8_t*)data, len, kFixedPlayoutDelayEstimate, kFixedRecordDelayEstimate);
}

void AudioDeviceSDL::GetPlayoutData(int8_t* buffer, const size_t desired_frame_size_bytes) 
{
	// it copy from FineAudioBuffer::GetPlayoutData in fine_audio_buffer.cc
	const size_t samples_per_10_ms_ = PLAY_BUF_SIZE_IN_SAMPLES;

  if (desired_frame_size_bytes <= playout_cached_bytes_) {
    memcpy(buffer, &playout_cache_buffer_.get()[playout_cached_buffer_start_],
           desired_frame_size_bytes);
    playout_cached_buffer_start_ += desired_frame_size_bytes;
    playout_cached_bytes_ -= desired_frame_size_bytes;
    RTC_CHECK_LT(playout_cached_buffer_start_ + playout_cached_bytes_,
                 play_bytes_per_10_ms_);
    return;
  }
  memcpy(buffer, &playout_cache_buffer_.get()[playout_cached_buffer_start_],
         playout_cached_bytes_);
  // Push another n*10ms of audio to |buffer|. n > 1 if
  // |desired_frame_size_bytes_| is greater than 10ms of audio. Note that we
  // write the audio after the cached bytes copied earlier.
  int8_t* unwritten_buffer = &buffer[playout_cached_bytes_];
  int bytes_left =
      static_cast<int>(desired_frame_size_bytes - playout_cached_bytes_);
  // Ceiling of integer division: 1 + ((x - 1) / y)
  size_t number_of_requests = 1 + (bytes_left - 1) / (play_bytes_per_10_ms_);
  for (size_t i = 0; i < number_of_requests; ++i) {
    audio_device_buffer_->RequestPlayoutData(samples_per_10_ms_);
    int num_out = audio_device_buffer_->GetPlayoutData(unwritten_buffer);
    if (static_cast<size_t>(num_out) != samples_per_10_ms_) {
      RTC_CHECK_EQ(num_out, 0);
      playout_cached_bytes_ = 0;
      return;
    }
    unwritten_buffer += play_bytes_per_10_ms_;
    RTC_CHECK_GE(bytes_left, 0);
    bytes_left -= static_cast<int>(play_bytes_per_10_ms_);
  }
  RTC_CHECK_LE(bytes_left, 0);
  // Put the samples that were written to |buffer| but are not used in the
  // cache.
  size_t cache_location = desired_frame_size_bytes;
  int8_t* cache_ptr = &buffer[cache_location];
  playout_cached_bytes_ = number_of_requests * play_bytes_per_10_ms_ -
                          (desired_frame_size_bytes - playout_cached_bytes_);
  // If playout_cached_bytes_ is larger than the cache buffer, uninitialized
  // memory will be read.
  RTC_CHECK_LE(playout_cached_bytes_, play_bytes_per_10_ms_);
  RTC_CHECK_EQ(static_cast<size_t>(-bytes_left), playout_cached_bytes_);
  playout_cached_buffer_start_ = 0;
  memcpy(playout_cache_buffer_.get(), cache_ptr, playout_cached_bytes_);
}

void AudioDeviceSDL::OnGetPlayoutData(uint8_t* stream, int len)
{
	RTC_CHECK(len == sound::get_buffer_size() * 2 * play_channels_);
	int8_t* source = playout_audio_buffer_.get();
	GetPlayoutData(source, len);
	memcpy(stream, source, len);
}


int32_t AudioDeviceSDL::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const {
  audioLayer = AudioDeviceModule::kPlatformDefaultAudio;
  return 0;
}

int16_t AudioDeviceSDL::PlayoutDevices() {
  // TODO(henrika): improve.
  return (int16_t)1;
}

int16_t AudioDeviceSDL::RecordingDevices() {
  // TODO(henrika): improve.
  return (int16_t)1;
}

int32_t AudioDeviceSDL::InitSpeaker() {
  return 0;
}

bool AudioDeviceSDL::SpeakerIsInitialized() const {
  return true;
}

int32_t AudioDeviceSDL::SpeakerVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceSDL::SetSpeakerVolume(uint32_t volume) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::SpeakerVolume(uint32_t& volume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::MaxSpeakerVolume(uint32_t& maxVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::MinSpeakerVolume(uint32_t& minVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::SpeakerMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceSDL::SetSpeakerMute(bool enable) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::SpeakerMute(bool& enabled) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::SetPlayoutDevice(uint16_t index) {
  return 0;
}

int32_t AudioDeviceSDL::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType) {
  // RTC_NOTREACHED() << "Not implemented";
  return 0;
}

int32_t AudioDeviceSDL::InitMicrophone() {
  return 0;
}

bool AudioDeviceSDL::MicrophoneIsInitialized() const {
  return true;
}

int32_t AudioDeviceSDL::MicrophoneMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceSDL::SetMicrophoneMute(bool enable) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::MicrophoneMute(bool& enabled) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::StereoRecordingIsAvailable(bool& available) 
{
	available = true;
	// available = false;
	return 0;
}

int32_t AudioDeviceSDL::SetStereoRecording(bool enable) 
{
	if (enable) {
		rec_channels_ = 2;
	} else {
		rec_channels_ = 1;
	}
    return 0;
}

int32_t AudioDeviceSDL::StereoRecording(bool& enabled) const 
{
	if (rec_channels_ == 2) {
        enabled = true;
	} else {
        enabled = false;
	}
    return 0;
}

int32_t AudioDeviceSDL::StereoPlayoutIsAvailable(bool& available) 
{
	// rose use stereo playback, so here must set stereo.
	available = true;
	return 0;
}

int32_t AudioDeviceSDL::SetStereoPlayout(bool enable) 
{
	if (enable) {
        play_channels_ = 2;
	} else {
        play_channels_ = 1;
	}
    return 0;
}

int32_t AudioDeviceSDL::StereoPlayout(bool& enabled) const 
{
	if (play_channels_ == 2) {
        enabled = true;
	} else {
        enabled = false;
	}
	return 0;
}

int32_t AudioDeviceSDL::MicrophoneVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceSDL::SetMicrophoneVolume(uint32_t volume) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::MicrophoneVolume(uint32_t& volume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::MaxMicrophoneVolume(uint32_t& maxVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::MinMicrophoneVolume(uint32_t& minVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::PlayoutDeviceName(uint16_t index,
                                          char name[kAdmMaxDeviceNameSize],
                                          char guid[kAdmMaxGuidSize]) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::RecordingDeviceName(uint16_t index,
                                            char name[kAdmMaxDeviceNameSize],
                                            char guid[kAdmMaxGuidSize]) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceSDL::SetRecordingDevice(uint16_t index) {
  return 0;
}

int32_t AudioDeviceSDL::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType) {
  // RTC_NOTREACHED() << "Not implemented";
  return 0;
}

int32_t AudioDeviceSDL::PlayoutIsAvailable(bool& available) {
  available = true;
  return 0;
}

int32_t AudioDeviceSDL::RecordingIsAvailable(bool& available) {
  available = true;
  return 0;
}

}  // namespace webrtc
