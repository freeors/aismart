// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base\android\jni_generator\jni_generator.py
// For
//     org/webrtc/WrappedNativeI420Buffer

#ifndef org_webrtc_WrappedNativeI420Buffer_JNI
#define org_webrtc_WrappedNativeI420Buffer_JNI

#include "sdk/android/src/jni/jni_generator_helper.h"

// Step 1: forward declarations.
JNI_REGISTRATION_EXPORT extern const char
    kClassPath_org_webrtc_WrappedNativeI420Buffer[];
const char kClassPath_org_webrtc_WrappedNativeI420Buffer[] =
    "org/webrtc/WrappedNativeI420Buffer";

// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT base::subtle::AtomicWord
    g_org_webrtc_WrappedNativeI420Buffer_clazz = 0;
#ifndef org_webrtc_WrappedNativeI420Buffer_clazz_defined
#define org_webrtc_WrappedNativeI420Buffer_clazz_defined
inline jclass org_webrtc_WrappedNativeI420Buffer_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env,
      kClassPath_org_webrtc_WrappedNativeI420Buffer,
      &g_org_webrtc_WrappedNativeI420Buffer_clazz);
}
#endif

// Step 2: method stubs.

static base::subtle::AtomicWord g_org_webrtc_WrappedNativeI420Buffer_Constructor
    = 0;
static base::android::ScopedJavaLocalRef<jobject>
    Java_WrappedNativeI420Buffer_Constructor(JNIEnv* env, JniIntWrapper width,
    JniIntWrapper height,
    const base::android::JavaRef<jobject>& dataY,
    JniIntWrapper strideY,
    const base::android::JavaRef<jobject>& dataU,
    JniIntWrapper strideU,
    const base::android::JavaRef<jobject>& dataV,
    JniIntWrapper strideV,
    jlong nativeBuffer) {
  CHECK_CLAZZ(env, org_webrtc_WrappedNativeI420Buffer_clazz(env),
      org_webrtc_WrappedNativeI420Buffer_clazz(env), NULL);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, org_webrtc_WrappedNativeI420Buffer_clazz(env),
      "<init>",
"("
"I"
"I"
"Ljava/nio/ByteBuffer;"
"I"
"Ljava/nio/ByteBuffer;"
"I"
"Ljava/nio/ByteBuffer;"
"I"
"J"
")"
"V",
      &g_org_webrtc_WrappedNativeI420Buffer_Constructor);

  jobject ret =
      env->NewObject(org_webrtc_WrappedNativeI420Buffer_clazz(env),
          method_id, as_jint(width), as_jint(height), dataY.obj(),
              as_jint(strideY), dataU.obj(), as_jint(strideU), dataV.obj(),
              as_jint(strideV), nativeBuffer);
  jni_generator::CheckException(env);
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

#endif  // org_webrtc_WrappedNativeI420Buffer_JNI
