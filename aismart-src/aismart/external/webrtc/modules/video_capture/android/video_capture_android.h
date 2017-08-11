/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_ANDROID_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_ANDROID_H_

#include "sdk/android/src/jni/videoframe.h"
#include "sdk/android/src/jni/surfacetexturehelper.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/timestampaligner.h"
#include "media/base/videoadapter.h"
#include "rtc_base/timeutils.h"
#include "modules/utility/include/helpers_android.h"
#include "modules/utility/include/jvm_android.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "modules/video_capture/video_capture_impl.h"
#include "common_video/include/i420_buffer_pool.h"

namespace webrtc {
namespace videocapturemodule {

class VideoCaptureAndroid : public VideoCaptureImpl 
{
public:
  // Wraps the Java specific parts of the AudioManager into one helper class.
  // Stores method IDs for all supported methods at construction and then
  // allows calls like JavaAudioManager::Close() while hiding the Java/JNI
  // parts that are associated with this call.
  class JavaVideoCapturer {
   public:
    JavaVideoCapturer(NativeRegistration* native_registration,
                     std::unique_ptr<GlobalRef> audio_manager);
    ~JavaVideoCapturer();

    bool Init();
    void Close();
    bool StartCapture(int width, int height, int fps);

   private:
    std::unique_ptr<GlobalRef> audio_manager_;
    jmethodID init_;
    jmethodID dispose_;
    jmethodID start_capture_;
  };

  explicit VideoCaptureAndroid();
  virtual ~VideoCaptureAndroid();

  static rtc::scoped_refptr<VideoCaptureModule> Create(
      const char* device_unique_id_utf8);

  // Implementation of VideoCaptureImpl.
  int32_t StartCapture(const VideoCaptureCapability& capability) override;
  int32_t StopCapture() override;
  bool CaptureStarted() override;
  int32_t CaptureSettings(VideoCaptureCapability& settings) override;

  void set_surface_texture_helper(const rtc::scoped_refptr<jni::SurfaceTextureHelper>& helper);
  void initializeVideoCapturer(JNIEnv* jni, jobject j_video_capturer, jobject j_frame_observer);
  void onByteBufferFrameCaptured(const void* frame_data, int length,
                             int width, int height, VideoRotation rotation, int64_t timestamp_ns);
  void onTextureFrameCaptured(int width, int height, int rotation, int64_t timestamp_ns, const jni::NativeHandleImpl& handle);

private:
	bool InitCaptureAndroidJNI();
	void UninitCaptureAndroidJNI();

	static void JNICALL CapturerStarted(JNIEnv* env, jobject obj, jlong j_source, jboolean success);
	static void JNICALL CapturerStopped(JNIEnv* env, jobject obj, jlong j_source);
	static void JNICALL OnByteBufferFrameCaptured(JNIEnv* jni, jclass, jlong j_source, jbyteArray j_frame, jint length, jint width, jint height, jint rotation, jlong timestamp);
	static void JNICALL OnTextureFrameCaptured(JNIEnv* env, jobject obj, jlong j_source, jint j_width, jint j_height, jint j_oes_texture_id, jfloatArray j_transform_matrix, jint j_rotation, jlong j_timestamp);
	static void JNICALL CreateSurfaceTextureHelper(JNIEnv* env, jobject obj, jlong j_source, jobject j_egl_context);
	static void JNICALL InitializeVideoCapturer(JNIEnv* jni, jclass, jlong j_source, jobject j_video_capturer, jobject j_frame_observer);

	bool AdaptFrame(int width,
                                         int height,
                                         int64_t time_us,
                                         int* out_width,
                                         int* out_height,
                                         int* crop_width,
                                         int* crop_height,
                                         int* crop_x,
                                         int* crop_y);

private:
  bool is_capturing_;
  VideoCaptureCapability capability_;

  // Stores thread ID in the constructor.
  // We can then use ThreadChecker::CalledOnValidThread() to ensure that
  // other methods are called from the same thread.
  rtc::ThreadChecker thread_checker_;

  // Calls AttachCurrentThread() if this thread is not attached at construction.
  // Also ensures that DetachCurrentThread() is called at destruction.
  std::unique_ptr<AttachCurrentThreadIfNeeded> attach_thread_if_needed_;

  // Wraps the JNI interface pointer and methods associated with it.
  // std::unique_ptr<JNIEnvironment> j_environment_;

  // Contains factory method for creating the Java object.
  std::unique_ptr<NativeRegistration> j_native_registration_;

  // Wraps the Java specific parts of the AudioManager.
  std::unique_ptr<JavaVideoCapturer> j_video_capturer_;

  // Set to true by Init() and false by Close().
  bool initialized_;

  rtc::scoped_refptr<jni::SurfaceTextureHelper> surface_texture_helper_;
  rtc::TimestampAligner timestamp_aligner_;
  cricket::VideoAdapter video_adapter_;

  NV12ToI420Scaler nv12toi420_scaler_;
  I420BufferPool buffer_pool_;
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_IOS_VIDEO_CAPTURE_IOS_H_
