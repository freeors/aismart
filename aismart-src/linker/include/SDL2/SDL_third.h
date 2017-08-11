/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef _SDL_third_h
#define _SDL_third_h

/**
 *  \file SDL_third.h
 *
 *  Header for the SDL third party management routines.
 */

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_video.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \name Third login app
 *
 *  Get the third app that support login.
 */
/* @{ */
typedef enum
{
	SDL_APP_NONE = 0,
    SDL_APP_WeChat,
    SDL_APP_QQ,
} SDL_ThirdApp;

/**
 * \brief Check third app installed or not.
 *
 * \return 1 installed, or 0 not installed.
 */
extern DECLSPEC int SDLCALL SDL_IsAppInstalled(int app);

/**
 *  Function prototype for the login callback function.
 *
 */
typedef void (SDLCALL * SDL_LoginCallback)(int app, const char* openid, const char* nick, const char* avatarurl, void *param);

/**
 * \brief Third login.
 *
 * \return 0, or -1 when an error occurs.
 */
extern DECLSPEC int SDLCALL SDL_ThirdLogin(int app, SDL_LoginCallback callback, const char* secretkey1, const char* secretkey2, void* param);

typedef enum {
    SDL_ThirdSceneSession,
    SDL_ThirdSceneTimeline,
    SDL_ThirdSceneFavorite
} SDL_ThirdScene;

typedef enum { 
	SDL_ThirdSendText,
	SDL_ThirdSendImage,
	SDL_ThirdSendLink,
	SDL_ThirdSendMusic,
	SDL_ThirdSendVideo,
	SDL_ThirdSendEmotion,
	SDL_ThirdSendFile
} SDL_ThirdSendType;

typedef void (SDLCALL * SDL_SendCallback)(int app, int result, void* param);

extern DECLSPEC int SDLCALL SDL_ThirdSend(int app, int scene, int type, const uint8_t* data, const int len, const char* name, const char* title, const char* desc, const uint8_t* thumb, const int thumb_len, SDL_SendCallback callback, void* param);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_timer_h */

/* vi: set ts=4 sw=4 expandtab: */
