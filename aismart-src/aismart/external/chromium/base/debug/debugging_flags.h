// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a cross platform interface for helper functions related to
// debuggers.  You should use this to test if you're running under a debugger,
// and if you would like to yield (breakpoint) into the debugger.

#ifndef BASE_DEBUGING_FLAGS_H_
#define BASE_DEBUGING_FLAGS_H_

#include "build/buildflag.h"

#define BUILDFLAG_INTERNAL_ENABLE_LOCATION_SOURCE() (0)
#define BUILDFLAG_INTERNAL_ENABLE_PROFILING() (0)
#define BUILDFLAG_INTERNAL_CAN_UNWIND_WITH_FRAME_POINTERS() (0)
#define BUILDFLAG_INTERNAL_UNSAFE_DEVELOPER_BUILD() (0)

#endif  // BASE_DEBUGING_FLAGS_H_
