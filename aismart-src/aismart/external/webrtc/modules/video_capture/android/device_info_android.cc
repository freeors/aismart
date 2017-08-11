/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// #include <AVFoundation/AVFoundation.h>

#include <string>

#include "modules/video_capture/android/device_info_android.h"
// #include "modules/video_capture/android/device_info_android_jni.h"
#include "modules/video_capture/video_capture_impl.h"
#include "rtc_base/logging.h"

using namespace webrtc;
using namespace videocapturemodule;
/*
static NSArray *camera_presets = @[AVCaptureSessionPreset352x288,
                                   AVCaptureSessionPreset640x480,
                                   AVCaptureSessionPreset1280x720,
                                   AVCaptureSessionPreset1920x1080];
*/

#define ANDROID_UNSUPPORTED()                                                        \
  RTC_LOG(LS_ERROR) << __FUNCTION__ << " is not supported on the Android platform."; \
  return -1;

VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo() {
  return new DeviceInfoAndroid();
}

DeviceInfoAndroid::DeviceInfoAndroid()
    : DeviceInfoImpl() {
  this->Init();
}

DeviceInfoAndroid::~DeviceInfoAndroid() {}

int32_t DeviceInfoAndroid::Init() 
{
	// Fill in all device capabilities.

	// int deviceCount = [DeviceInfoIosObjC captureDeviceCount];
	int deviceCount = 1;

	for (int i = 0; i < deviceCount; i++) {
		// AVCaptureDevice *avDevice = [DeviceInfoIosObjC captureDeviceForIndex:i];
		VideoCaptureCapabilities capabilityVector;

		// for (NSString *preset in camera_presets) {
		for (int ncap = 0; ncap < 1; ncap ++) {
			VideoCaptureCapability capability;

			capability.width = 352;
			capability.height = 288;
			capability.maxFPS = 29;
			capability.videoType = VideoType::kYUY2;
			capability.interlaced = false;
			capabilityVector.push_back(capability);

			capability.width = 640;
			capability.height = 480;
			capability.maxFPS = 29;
			capability.videoType = VideoType::kYUY2;
			capability.interlaced = false;
			capabilityVector.push_back(capability);

			capability.width = 1280;
			capability.height = 720;
			capability.maxFPS = 29;
			capability.videoType = VideoType::kYUY2;
			capability.interlaced = false;
			capabilityVector.push_back(capability);
		}

		char deviceNameUTF8[256];
		char deviceId[256];
		this->GetDeviceName(i, deviceNameUTF8, 256, deviceId, 256);
		std::string deviceIdCopy(deviceId);
		std::pair<std::string, VideoCaptureCapabilities> mapPair = std::pair<std::string, VideoCaptureCapabilities>(deviceIdCopy, capabilityVector);
		_capabilitiesMap.insert(mapPair);
	}

  return 0;
}

uint32_t DeviceInfoAndroid::NumberOfDevices() 
{
	// return [DeviceInfoIosObjC captureDeviceCount];
	return 1;
}

#define default_deviceUniqueIdUTF8	"camera#1"

int32_t DeviceInfoAndroid::GetDeviceName(uint32_t deviceNumber,
                                     char* deviceNameUTF8,
                                     uint32_t deviceNameUTF8Length,
                                     char* deviceUniqueIdUTF8,
                                     uint32_t deviceUniqueIdUTF8Length,
                                     char* productUniqueIdUTF8,
                                     uint32_t productUniqueIdUTF8Length) 
{
/*
  NSString* deviceName = [DeviceInfoIosObjC deviceNameForIndex:deviceNumber];

  NSString* deviceUniqueId =
      [DeviceInfoIosObjC deviceUniqueIdForIndex:deviceNumber];

  strncpy(deviceNameUTF8, [deviceName UTF8String], deviceNameUTF8Length);
  deviceNameUTF8[deviceNameUTF8Length - 1] = '\0';

  strncpy(deviceUniqueIdUTF8,
          [deviceUniqueId UTF8String],
          deviceUniqueIdUTF8Length);
  deviceUniqueIdUTF8[deviceUniqueIdUTF8Length - 1] = '\0';
*/
	strcpy(deviceNameUTF8, "camera");
	strcpy(deviceUniqueIdUTF8, default_deviceUniqueIdUTF8);

	if (productUniqueIdUTF8) {
		productUniqueIdUTF8[0] = '\0';
	}

  return 0;
}

int32_t DeviceInfoAndroid::NumberOfCapabilities(const char* deviceUniqueIdUTF8) {
  int32_t numberOfCapabilities = 0;
  std::string deviceUniqueId(deviceUniqueIdUTF8);
  std::map<std::string, VideoCaptureCapabilities>::iterator it =
      _capabilitiesMap.find(deviceUniqueId);

  if (it != _capabilitiesMap.end()) {
    numberOfCapabilities = it->second.size();
  }
  return numberOfCapabilities;
}

int32_t DeviceInfoAndroid::GetCapability(const char* deviceUniqueIdUTF8,
                                     const uint32_t deviceCapabilityNumber,
                                     VideoCaptureCapability& capability) {
  std::string deviceUniqueId(deviceUniqueIdUTF8);
  std::map<std::string, VideoCaptureCapabilities>::iterator it =
      _capabilitiesMap.find(deviceUniqueId);

  if (it != _capabilitiesMap.end()) {
    VideoCaptureCapabilities deviceCapabilities = it->second;

    if (deviceCapabilityNumber < deviceCapabilities.size()) {
      VideoCaptureCapability cap;
      cap = deviceCapabilities[deviceCapabilityNumber];
      capability = cap;
      return 0;
    }
  }

  return -1;
}

int32_t DeviceInfoAndroid::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8,
    const char* dialogTitleUTF8,
    void* parentWindow,
    uint32_t positionX,
    uint32_t positionY) {
  ANDROID_UNSUPPORTED();
}

int32_t DeviceInfoAndroid::GetOrientation(const char* deviceUniqueIdUTF8,
                                      VideoRotation& orientation) {
  if (strcmp(deviceUniqueIdUTF8, "Front Camera") == 0) {
    orientation = kVideoRotation_0;
  } else {
    // orientation = kVideoRotation_90;
	orientation = kVideoRotation_0;
  }
  return orientation;
}

int32_t DeviceInfoAndroid::CreateCapabilityMap(const char* deviceUniqueIdUTF8) 
{
  std::string deviceName(deviceUniqueIdUTF8);
  std::map<std::string, std::vector<VideoCaptureCapability>>::iterator it =
      _capabilitiesMap.find(deviceName);
  VideoCaptureCapabilities deviceCapabilities;
  if (it != _capabilitiesMap.end()) {
    _captureCapabilities = it->second;
    return 0;
  }

  return -1;
}
