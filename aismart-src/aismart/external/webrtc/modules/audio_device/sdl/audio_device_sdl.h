/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_SDL_AUDIO_DEVICE_SDL_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_SDL_AUDIO_DEVICE_SDL_H_

#include <memory>

#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"
#include "modules/audio_device/audio_device_generic.h"
#include "sound.hpp"
#include "sdl.h"
#include "sdl_mixer.h"

namespace webrtc {

class FineAudioBuffer;

// Implements full duplex 16-bit mono PCM audio support for iOS using a
// Voice-Processing (VP) I/O audio unit in Core Audio. The VP I/O audio unit
// supports audio echo cancellation. It also adds automatic gain control,
// adjustment of voice-processing quality and muting.
//
// An instance must be created and destroyed on one and the same thread.
// All supported public methods must also be called on the same thread.
// A thread checker will RTC_DCHECK if any supported method is called on an
// invalid thread.
//
// Recorded audio will be delivered on a real-time internal I/O thread in the
// audio unit. The audio unit will also ask for audio data to play out on this
// same thread.
class AudioDeviceSDL : public AudioDeviceGeneric
{
public:
	AudioDeviceSDL();
	~AudioDeviceSDL();

	void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

	InitStatus Init() override;
	int32_t Terminate() override;
	bool Initialized() const override { return initialized_; }

	int32_t InitPlayout() override;
	bool PlayoutIsInitialized() const override { return play_is_initialized_; }

	int32_t InitRecording() override;
	bool RecordingIsInitialized() const override { return rec_is_initialized_; }

	int32_t StartPlayout() override;
	int32_t StopPlayout() override;
	bool Playing() const override { return playing_; }

	int32_t StartRecording() override;
	int32_t StopRecording() override;
	bool Recording() const override { return recording_; }

	// These methods returns hard-coded delay values and not dynamic delay
	// estimates. The reason is that iOS supports a built-in AEC and the WebRTC
	// AEC will always be disabled in the Libjingle layer to avoid running two
	// AEC implementations at the same time. And, it saves resources to avoid
	// updating these delay values continuously.
	// TODO(henrika): it would be possible to mark these two methods as not
	// implemented since they are only called for A/V-sync purposes today and
	// A/V-sync is not supported on iOS. However, we avoid adding error messages
	// the log by using these dummy implementations instead.
	int32_t PlayoutDelay(uint16_t& delayMS) const override;

	// See audio_device_not_implemented.cc for trivial implementations.
	int32_t ActiveAudioLayer(
		AudioDeviceModule::AudioLayer& audioLayer) const override;
	int32_t PlayoutIsAvailable(bool& available) override;
	int32_t RecordingIsAvailable(bool& available) override;

	int16_t PlayoutDevices() override;
	int16_t RecordingDevices() override;
	int32_t PlayoutDeviceName(uint16_t index,
							char name[kAdmMaxDeviceNameSize],
							char guid[kAdmMaxGuidSize]) override;
	int32_t RecordingDeviceName(uint16_t index,
								char name[kAdmMaxDeviceNameSize],
								char guid[kAdmMaxGuidSize]) override;
	int32_t SetPlayoutDevice(uint16_t index) override;
	int32_t SetPlayoutDevice(
		AudioDeviceModule::WindowsDeviceType device) override;
	int32_t SetRecordingDevice(uint16_t index) override;
	int32_t SetRecordingDevice(
		AudioDeviceModule::WindowsDeviceType device) override;
	int32_t InitSpeaker() override;
	bool SpeakerIsInitialized() const override;
	int32_t InitMicrophone() override;
	bool MicrophoneIsInitialized() const override;
	int32_t SpeakerVolumeIsAvailable(bool& available) override;
	int32_t SetSpeakerVolume(uint32_t volume) override;
	int32_t SpeakerVolume(uint32_t& volume) const override;
	int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override;
	int32_t MinSpeakerVolume(uint32_t& minVolume) const override;
	int32_t MicrophoneVolumeIsAvailable(bool& available) override;
	int32_t SetMicrophoneVolume(uint32_t volume) override;
	int32_t MicrophoneVolume(uint32_t& volume) const override;
	int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const override;
	int32_t MinMicrophoneVolume(uint32_t& minVolume) const override;
	int32_t MicrophoneMuteIsAvailable(bool& available) override;
	int32_t SetMicrophoneMute(bool enable) override;
	int32_t MicrophoneMute(bool& enabled) const override;
	int32_t SpeakerMuteIsAvailable(bool& available) override;
	int32_t SetSpeakerMute(bool enable) override;
	int32_t SpeakerMute(bool& enabled) const override;
	int32_t StereoPlayoutIsAvailable(bool& available) override;
	int32_t SetStereoPlayout(bool enable) override;
	int32_t StereoPlayout(bool& enabled) const override;
	int32_t StereoRecordingIsAvailable(bool& available) override;
	int32_t SetStereoRecording(bool enable) override;
	int32_t StereoRecording(bool& enabled) const override;

	// VoiceProcessingAudioUnitObserver methods.
	void did_capture_audio(uint8_t* data, int len);
	void OnGetPlayoutData(uint8_t* data, int len);

private:
	size_t RequiredPlayoutBufferSizeBytes(const size_t desired_frame_size_bytes);
	void ResetPlayout();
	void GetPlayoutData(int8_t* buffer, const size_t desired_frame_size_bytes);
	void ResetRecord();
	void DeliverRecordedData(const int8_t* buffer, size_t size_in_bytes, int playout_delay_ms, int record_delay_ms);

private:
	// Ensures that methods are called from the same thread as this object is
	// created on.
	rtc::ThreadChecker thread_checker_;
	// Thread that this object is created on.
	rtc::Thread* thread_;

	// Raw pointer handle provided to us in AttachAudioBuffer(). Owned by the
	// AudioDeviceModuleImpl class and called by AudioDeviceModule::Create().
	// The AudioDeviceBuffer is a member of the AudioDeviceModuleImpl instance
	// and therefore outlives this object.
	AudioDeviceBuffer* audio_device_buffer_;

	// Set to 1 when recording is active and 0 otherwise.
	bool recording_;

	// Set to 1 when playout is active and 0 otherwise.
	bool playing_;

	// Set to true after successful call to Init(), false otherwise.
	bool initialized_;

	// Set to true after successful call to InitRecording(), false otherwise.
	bool rec_is_initialized_;

	// Set to true after successful call to InitPlayout(), false otherwise.
	bool play_is_initialized_;

	// Set to true if audio session is interrupted, false otherwise.
	bool is_interrupted_;

	// Set to true if we've activated the audio session.
	bool has_configured_session_;

	int32_t id_;
	SDL_AudioDeviceID capture_audio_id_;
	int32_t rec_channels_;
	int32_t play_channels_;

	std::unique_ptr<sound::tfrequency_lock> playout_frequency_lock_;
	std::unique_ptr<sound::thook_get_mix_data> hook_get_mix_data_;
	// Storage for output samples that are not yet asked for.
	std::unique_ptr<int8_t[]> playout_cache_buffer_;
	// Location of first unread output sample.
	size_t playout_cached_buffer_start_;
	// Number of bytes stored in output (contain samples to be played out) cache.
	size_t playout_cached_bytes_;
	// Extra audio buffer to be used by the playout side for rendering audio.
	// The buffer size is given by FineAudioBuffer::RequiredBufferSizeBytes().
	std::unique_ptr<int8_t[]> playout_audio_buffer_;
	// Number of audio bytes per 10ms. (only playout)
	size_t play_bytes_per_10_ms_;

	// Storage for input samples that are about to be delivered to the WebRTC
	// ADB or remains from the last successful delivery of a 10ms audio buffer.
	std::unique_ptr<int8_t[]> record_cache_buffer_;
	// Required (max) size in bytes of the |record_cache_buffer_|.
	size_t required_record_buffer_size_bytes_;
	// Number of bytes in input (contains recorded samples) cache.
	size_t record_cached_bytes_;
	// Read and write pointers used in the buffering scheme on the recording side.
	size_t record_read_pos_;
	size_t record_write_pos_;
	// Number of audio bytes per 10ms. (only record)
	size_t rec_bytes_per_10_ms_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_SDL_AUDIO_DEVICE_SDL_H_
