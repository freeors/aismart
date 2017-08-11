set PATH=%PATH%;C:\Program Files\Android\Android Studio\jre\bin

set JNI_GENERATOR=C:\movie\chromium\base\android\jni_generator
set BASE_JNI_SRC=C:\ddksample\apps-src\apps\external\webrtc\sdk\android\src\java\org\webrtc
set BASE_JNI_DEST=C:\ddksample\apps-src\apps\external\webrtc\sdk\android\generated_base_jni\jni

rem %JNI_GENERATOR%\jni_generator.py --input_file %BASE_JNI_SRC%\Histogram.java --output_dir %BASE_JNI_DEST%
rem %JNI_GENERATOR%\jni_generator.py --input_file %BASE_JNI_SRC%\JniCommon.java --output_dir %BASE_JNI_DEST%
rem %JNI_GENERATOR%\jni_generator.py --input_file %BASE_JNI_SRC%\JniHelper.java --output_dir %BASE_JNI_DEST%
rem %JNI_GENERATOR%\jni_generator.py --input_file %BASE_JNI_SRC%\WebRtcClassLoader.java --output_dir %BASE_JNI_DEST%


set EXTERNAL_CLASSES_JNI_SRC=C:\movie\rt\java
set EXTERNAL_CLASSES_JNI_DEST=C:\ddksample\apps-src\apps\external\webrtc\sdk\android\generated_external_classes_jni\jni

rem %JNI_GENERATOR%\jni_generator.py --input_file %EXTERNAL_CLASSES_JNI_SRC%\lang\Long.class --output_dir %EXTERNAL_CLASSES_JNI_DEST%

set API_JNI_SRC=C:\ddksample\apps-src\apps\external\webrtc\sdk\android\api\org\webrtc
set SRC_JNI_SRC=C:\ddksample\apps-src\apps\external\webrtc\sdk\android\src\java\org\webrtc
set VIDEO_JNI_DEST=C:\ddksample\apps-src\apps\external\webrtc\sdk\android\generated_video_jni\jni

%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\EncodedImage.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\HardwareVideoEncoderFactory.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\MediaCodecVideoDecoder.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\MediaCodecVideoEncoder.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\SurfaceTextureHelper.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoCodecInfo.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoCodecStatus.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoDecoder.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoDecoderFactory.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoDecoderFallback.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoEncoder.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoEncoderFactory.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoEncoderFallback.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoFileRenderer.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoFrame.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoRenderer.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoSink.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoSource.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\VideoTrack.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %API_JNI_SRC%\YuvHelper.java --output_dir %VIDEO_JNI_DEST%

%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\AndroidVideoTrackSourceObserver.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\EglBase14.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\NV12Buffer.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\NV21Buffer.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\VP8Decoder.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\VP8Encoder.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\VP9Decoder.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\VP9Encoder.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\VideoDecoderWrapper.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\VideoEncoderWrapper.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\WrappedNativeI420Buffer.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\WrappedNativeVideoDecoder.java --output_dir %VIDEO_JNI_DEST%
%JNI_GENERATOR%\jni_generator.py --input_file %SRC_JNI_SRC%\WrappedNativeVideoEncoder.java --output_dir %VIDEO_JNI_DEST%