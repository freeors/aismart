//
// it is my generated file, not offical's part.
//

#ifndef _USRSCTP_CONFIG_H
#define _USRSCTP_CONFIG_H

#define SCTP_PROCESS_LEVEL_LOCKS
#define SCTP_SIMPLE_ALLOCATOR
#define SCTP_USE_OPENSSL_SHA1
#define __Userspace__
// #define INET
// #define INET6

#ifdef _WIN32
#define __Userspace_os_Windows

#elif __APPLE__
#define HAVE_SA_LEN
#define HAVE_SCONN_LEN
#define __APPLE_USE_RFC_2292
#define __Userspace_os_Darwin
#define NON_WINDOWS_DEFINE
#undef __APPLE__

#elif ANDROID
#define __Userspace_os_Linux
#define _GNU_SOURCE
#define NON_WINDOWS_DEFINE
#endif

#endif
