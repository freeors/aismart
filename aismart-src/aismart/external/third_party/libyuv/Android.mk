SUB_PATH := external/third_party/libyuv

LOCAL_SRC_FILES += \
	$(SUB_PATH)/source/compare.cc \
    $(SUB_PATH)/source/compare_common.cc \
    $(SUB_PATH)/source/compare_gcc.cc \
    $(SUB_PATH)/source/compare_win.cc \
    $(SUB_PATH)/source/convert.cc \
    $(SUB_PATH)/source/convert_argb.cc \
    $(SUB_PATH)/source/convert_from.cc \
    $(SUB_PATH)/source/convert_from_argb.cc \
    $(SUB_PATH)/source/convert_jpeg.cc \
    $(SUB_PATH)/source/convert_to_argb.cc \
    $(SUB_PATH)/source/convert_to_i420.cc \
    $(SUB_PATH)/source/cpu_id.cc \
    $(SUB_PATH)/source/mjpeg_decoder.cc \
    $(SUB_PATH)/source/mjpeg_validate.cc \
    $(SUB_PATH)/source/planar_functions.cc \
    $(SUB_PATH)/source/rotate.cc \
    $(SUB_PATH)/source/rotate_any.cc \
    $(SUB_PATH)/source/rotate_argb.cc \
    $(SUB_PATH)/source/rotate_common.cc \
    $(SUB_PATH)/source/rotate_mips.cc \
    $(SUB_PATH)/source/rotate_gcc.cc \
    $(SUB_PATH)/source/rotate_win.cc \
    $(SUB_PATH)/source/row_any.cc \
    $(SUB_PATH)/source/row_common.cc \
    $(SUB_PATH)/source/row_mips.cc \
    $(SUB_PATH)/source/row_gcc.cc \
    $(SUB_PATH)/source/row_win.cc \
    $(SUB_PATH)/source/scale.cc \
    $(SUB_PATH)/source/scale_any.cc \
    $(SUB_PATH)/source/scale_argb.cc \
    $(SUB_PATH)/source/scale_common.cc \
    $(SUB_PATH)/source/scale_mips.cc \
    $(SUB_PATH)/source/scale_gcc.cc \
    $(SUB_PATH)/source/scale_win.cc \
    $(SUB_PATH)/source/video_common.cc \
	$(SUB_PATH)/source/compare_neon.cc \
	$(SUB_PATH)/source/compare_neon64.cc \
	$(SUB_PATH)/source/rotate_neon.cc \
	$(SUB_PATH)/source/rotate_neon64.cc \
	$(SUB_PATH)/source/row_neon.cc \
	$(SUB_PATH)/source/row_neon64.cc \
	$(SUB_PATH)/source/scale_neon.cc \
	$(SUB_PATH)/source/scale_neon64.cc