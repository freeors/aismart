# The ARMv7 is significanly faster due to the use of the hardware FPU
# APP_ABI := all
APP_ABI := armeabi-v7a
APP_PLATFORM := android-16

APP_STL := c++_shared
APP_CPPFLAGS += -fexceptions -frtti

APP_CFLAGS += -Wno-error=format-security