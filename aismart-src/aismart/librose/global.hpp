/* $Id: global.hpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_GLOBAL_HPP_INCLUDED
#define LIBROSE_GLOBAL_HPP_INCLUDED

#ifdef _MSC_VER

// #undef snprintf
// #define snprintf _snprintf

// Disable warnig about source encoding not in current code page.
#pragma warning(disable: 4819)

// Disable warning about deprecated functions.
#pragma warning(disable: 4996)

//disable some MSVC warnings which are useless according to mordante
#pragma warning(disable: 4244)
#pragma warning(disable: 4345)
#pragma warning(disable: 4250)
#pragma warning(disable: 4355)
#pragma warning(disable: 4800)
#pragma warning(disable: 4351)

#endif
/*
#ifdef _WIN32
// for memory lead detect begin
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
// for memory lead detect end
#endif
*/
// npos macro
#define nposm					-1

#define MAXLEN_APP				31
#define MAXLEN_TEXTDOMAIN		63	// MAXLEN_TEXTDOMAIN must >= MAXLEN_APP + 4. default textdomain is <app>-lib

void SDL_SimplerMB(const char* fmt, ...);

// 1.mask must be power's 2, and not 0.
// 2.if val is 0, it will return 0. even if mask is any value.
// 3.if mask is power's 2, use posix_align_ceil, or use posix_align_ceil2.
#define posix_align_ceil(val, mask)     (((val) + (mask) - 1) & ~((mask) - 1))

int posix_align_ceil2(int dividend, int divisor);
#define posix_align_floor(dividend, divisor)	((dividend) - ((dividend) % (divisor)))

// assistant macro of byte,word,dword,qword opration. windef.h has this definition.
// for make macro, first parameter is low part, second parameter is high part.
#ifndef posix_mku64
	#define posix_mku64(l, h)		((uint64_t)(((uint32_t)(l)) | ((uint64_t)((uint32_t)(h))) << 32))    
#endif

#ifndef posix_mku32
	#define posix_mku32(l, h)		((uint32_t)(((uint16_t)(l)) | ((uint32_t)((uint16_t)(h))) << 16))
#endif

#ifndef posix_mku16
	#define posix_mku16(l, h)		((uint16_t)(((uint8_t)(l)) | ((uint16_t)((uint8_t)(h))) << 8))
#endif

#ifndef posix_mki64
	#define posix_mki64(l, h)		((int64_t)(((uint32_t)(l)) | ((int64_t)((uint32_t)(h))) << 32))    
#endif

#ifndef posix_mki32
	#define posix_mki32(l, h)		((int32_t)(((uint16_t)(l)) | ((int32_t)((uint16_t)(h))) << 16))
#endif

#ifndef posix_mki16
	#define posix_mki16(l, h)		((int16_t)(((uint8_t)(l)) | ((int16_t)((uint8_t)(h))) << 8))
#endif

#ifndef posix_lo32
	#define posix_lo32(v64)			((uint32_t)(v64))
#endif

#ifndef posix_hi32
	#define posix_hi32(v64)			((uint32_t)(((uint64_t)(v64) >> 32) & 0xFFFFFFFF))
#endif

#ifndef posix_lo16
	#define posix_lo16(v32)			((uint16_t)(v32))
#endif

#ifndef posix_hi16
	#define posix_hi16(v32)			((uint16_t)(((uint32_t)(v32) >> 16) & 0xFFFF))
#endif

#ifndef posix_lo8
	#define posix_lo8(v16)			((uint8_t)(v16))
#endif

#ifndef posix_hi8
	#define posix_hi8(v16)			((uint8_t)(((uint16_t)(v16) >> 8) & 0xFF))
#endif

#ifndef posix_max
#define posix_max(a,b)            (((a) > (b))? (a) : (b))
#endif

#ifndef posix_min
#define posix_min(a,b)            (((a) < (b))? (a) : (b))
#endif

#ifndef posix_clip
#define	posix_clip(x, min, max)	  posix_max(posix_min((x), (max)), (min))
#endif

#ifndef posix_abs
#define posix_abs(a)            (((a) >= 0)? (a) : (-(a)))
#endif

#endif
