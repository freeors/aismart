@echo off
if not exist "%SDL_sdl%\libs\armeabi-v7a\libSDL2.so" goto copy_from_linker

@echo on

set DST=%SCRIPTS%\..\linker\android\lib\armeabi-v7a\.
copy %SDL_sdl%\libs\armeabi-v7a\libSDL2.so %DST%
copy %SDL_image%\libs\armeabi-v7a\libSDL2_image.so %DST%
copy %SDL_mixer%\libs\armeabi-v7a\libSDL2_mixer.so %DST%
copy %SDL_ttf%\libs\armeabi-v7a\libSDL2_ttf.so %DST%
copy %libvpx%\libs\armeabi-v7a\libvpx.so %DST%
copy %protobuf%\libs\armeabi-v7a\libprotobuf.so %DST%

copy %opencv%\libs\armeabi-v7a\libopencv.so %DST%

:copy_from_linker

@echo on

set SRC=%SCRIPTS%\..\linker\android\lib\armeabi-v7a
set DST_arm=%NDK%\platforms\android-21\arch-arm\usr\lib\.
copy %SRC%\libSDL2.so %DST_arm%
copy %SRC%\libSDL2_image.so %DST_arm%
copy %SRC%\libSDL2_mixer.so %DST_arm%
copy %SRC%\libSDL2_ttf.so %DST_arm%
copy %SRC%\libvpx.so %DST_arm%
copy %SRC%\libprotobuf.so %DST_arm%

copy %SRC%\libopencv.so %DST_arm%

@echo off

rem ABI_LEVEL: 21 hasn't arch-arm64