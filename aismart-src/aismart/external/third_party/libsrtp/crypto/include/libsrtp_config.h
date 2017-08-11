// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a stub config.h for libSRTP. It doesn't define anything besides
// version number strings because the build is configured by libsrtp.gyp.
#ifndef _LIBSRTP_CONFIG_H
#define _LIBSRTP_CONFIG_H

#define PACKAGE_STRING "libsrtp2 2.1.0-pre"
#define PACKAGE_VERSION "2.1.0-pre"

#define HAVE_CONFIG_H
#define OPENSSL

#define HAVE_STDLIB_H
#define HAVE_STRING_H
#define HAVE_STDINT_H
#define HAVE_INTTYPES_H
#define HAVE_INT16_T
#define HAVE_INT32_T
#define HAVE_INT8_T
#define HAVE_UINT16_T
#define HAVE_UINT32_T
#define HAVE_UINT64_T
#define HAVE_UINT8_T

#ifdef _WIN32
	#define HAVE_WINSOCK2_H
#else
	#define HAVE_ARPA_INET_H
	#define HAVE_NETINET_IN_H
	#define HAVE_SYS_TYPES_H
	#define HAVE_UNISTD_H
#endif

#endif
