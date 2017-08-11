# The ARMv7 is significanly faster due to the use of the hardware FPU
# APP_ABI := armeabi armeabi-v7a arm64-v8a
APP_ABI := armeabi-v7a
APP_PLATFORM := android-21 # reference webrtc/android/config.gni

# APP_STL := gnustl_shared
# APP_GNUSTL_FORCE_CPP_FEATURES := exceptions rtti

APP_STL := c++_shared
APP_CPPFLAGS += -fexceptions
APP_CPPFLAGS += -frtti
APP_SHORT_COMMANDS := true