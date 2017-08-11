// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a cross platform interface for helper functions related to
// debuggers.  You should use this to test if you're running under a debugger,
// and if you would like to yield (breakpoint) into the debugger.

#ifndef BASE_ANDROID_LIBRARY_LOADER_ANCHOR_FUNCTIONS_FLAGS_H_
#define BASE_ANDROID_LIBRARY_LOADER_ANCHOR_FUNCTIONS_FLAGS_H_

#include "build/buildflag.h"

#define BUILDFLAG_INTERNAL_USE_LLD() (0)
#define BUILDFLAG_INTERNAL_SUPPORTS_CODE_ORDERING() (0)

#endif  // BASE_ANDROID_LIBRARY_LOADER_ANCHOR_FUNCTIONS_FLAGS_H_
