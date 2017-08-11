@echo off
if '%1'=='' goto help
if '%1'=='help' goto help

if not exist "%SDL_sdl%\libs\armeabi-v7a\libSDL2.so" goto copy_from_linker

@echo on

set DST=%1\..\..\..\..\linker\android\lib\armeabi-v7a\.
copy %SDL_sdl%\libs\armeabi-v7a\libSDL2.so %DST%
copy %SDL_image%\libs\armeabi-v7a\libSDL2_image.so %DST%
copy %SDL_mixer%\libs\armeabi-v7a\libSDL2_mixer.so %DST%
copy %SDL_ttf%\libs\armeabi-v7a\libSDL2_ttf.so %DST%
copy %libvpx%\libs\armeabi-v7a\libvpx.so %DST%
copy %protobuf%\libs\armeabi-v7a\libprotobuf.so %DST%

copy %opencv%\libs\armeabi-v7a\libopencv.so %DST%


:copy_from_linker

@echo on

set SRC=%1\..\..\..\..\linker\android\lib\armeabi-v7a
set DST=%1\libs\armeabi-v7a\.
copy %SRC%\libSDL2.so %DST%
copy %SRC%\libSDL2_image.so %DST%
copy %SRC%\libSDL2_mixer.so %DST%
copy %SRC%\libSDL2_ttf.so %DST%
copy %SRC%\libvpx.so %DST%
copy %SRC%\libprotobuf.so %DST%

copy %SRC%\libopencv.so %DST%

@echo off
goto exit

:help
echo Missing parameter, you must set app. for example: android_2_app %%studio%%

:exit