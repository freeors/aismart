// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use std::tuple as tuple type. This file contains helper functions for
// working with std::tuples.
// The functions DispatchToMethod and DispatchToFunction take a function pointer
// or instance and method pointer, and unpack a tuple into arguments to the
// call.
//
// Example usage:
//   // These two methods of creating a Tuple are identical.
//   std::tuple<int, const char*> tuple_a(1, "wee");
//   std::tuple<int, const char*> tuple_b = std::make_tuple(1, "wee");
//
//   void SomeFunc(int a, const char* b) { }
//   DispatchToFunction(&SomeFunc, tuple_a);  // SomeFunc(1, "wee")
//   DispatchToFunction(
//       &SomeFunc, std::make_tuple(10, "foo"));    // SomeFunc(10, "foo")
//
//   struct { void SomeMeth(int a, int b, int c) { } } foo;
//   DispatchToMethod(&foo, &Foo::SomeMeth, std::make_tuple(1, 2, 3));
//   // foo->SomeMeth(1, 2, 3);

#ifndef BASE_GENERATED_BUILD_DATE_H_
#define BASE_GENERATED_BUILD_DATE_H_

#define BUILD_DATE "Jan 03 2018 01:02:03"

#endif  // BASE_GENERATED_BUILD_DATE_H_
