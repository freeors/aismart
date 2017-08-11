// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     org/chromium/base/JavaExceptionReporter

#ifndef org_chromium_base_JavaExceptionReporter_JNI
#define org_chromium_base_JavaExceptionReporter_JNI

#include <jni.h>

#include "base/android/jni_generator/jni_generator_helper.h"

// Step 1: forward declarations.
JNI_REGISTRATION_EXPORT extern const char
    kClassPath_org_chromium_base_JavaExceptionReporter[];
const char kClassPath_org_chromium_base_JavaExceptionReporter[] =
    "org/chromium/base/JavaExceptionReporter";

// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT base::subtle::AtomicWord
    g_org_chromium_base_JavaExceptionReporter_clazz = 0;
#ifndef org_chromium_base_JavaExceptionReporter_clazz_defined
#define org_chromium_base_JavaExceptionReporter_clazz_defined
inline jclass org_chromium_base_JavaExceptionReporter_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env,
      kClassPath_org_chromium_base_JavaExceptionReporter,
      &g_org_chromium_base_JavaExceptionReporter_clazz);
}
#endif

namespace base {
namespace android {

// Step 2: method stubs.

static void JNI_JavaExceptionReporter_ReportJavaException(JNIEnv* env, const
    base::android::JavaParamRef<jclass>& jcaller,
    jboolean crashAfterReport,
    const base::android::JavaParamRef<jthrowable>& e);

JNI_GENERATOR_EXPORT void
    Java_org_chromium_base_JavaExceptionReporter_nativeReportJavaException(JNIEnv*
    env, jclass jcaller,
    jboolean crashAfterReport,
    jthrowable e) {
  TRACE_NATIVE_EXECUTION_SCOPED("ReportJavaException");
  return JNI_JavaExceptionReporter_ReportJavaException(env,
      base::android::JavaParamRef<jclass>(env, jcaller), crashAfterReport,
      base::android::JavaParamRef<jthrowable>(env, e));
}

static void JNI_JavaExceptionReporter_ReportJavaStackTrace(JNIEnv* env, const
    base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jstring>& stackTrace);

JNI_GENERATOR_EXPORT void
    Java_org_chromium_base_JavaExceptionReporter_nativeReportJavaStackTrace(JNIEnv*
    env, jclass jcaller,
    jstring stackTrace) {
  TRACE_NATIVE_EXECUTION_SCOPED("ReportJavaStackTrace");
  return JNI_JavaExceptionReporter_ReportJavaStackTrace(env,
      base::android::JavaParamRef<jclass>(env, jcaller),
      base::android::JavaParamRef<jstring>(env, stackTrace));
}

static base::subtle::AtomicWord
    g_org_chromium_base_JavaExceptionReporter_installHandler = 0;
static void Java_JavaExceptionReporter_installHandler(JNIEnv* env, jboolean
    crashAfterReport) {
  CHECK_CLAZZ(env, org_chromium_base_JavaExceptionReporter_clazz(env),
      org_chromium_base_JavaExceptionReporter_clazz(env));
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, org_chromium_base_JavaExceptionReporter_clazz(env),
      "installHandler",
"("
"Z"
")"
"V",
      &g_org_chromium_base_JavaExceptionReporter_installHandler);

env->CallStaticVoidMethod(org_chromium_base_JavaExceptionReporter_clazz(env),
          method_id, crashAfterReport);
  jni_generator::CheckException(env);
}

}  // namespace android
}  // namespace base

#endif  // org_chromium_base_JavaExceptionReporter_JNI
