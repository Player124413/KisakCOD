// JNI header shim: on Android the real <jni.h> is used; on desktop builds it
// provides the handful of types/functions host.cpp uses so the file can be
// syntax-checked and unit-tested without the NDK.
#pragma once

#if defined(__ANDROID__)
#include <jni.h>
#else

#include <cstddef>

struct FakeJString {
    const char *c;
};

typedef void *jobject;
typedef FakeJString *jstring;
typedef unsigned char jboolean;
typedef int jint;
typedef long jlong;
typedef float jfloat;
typedef double jdouble;
typedef void *jclass;

struct JNIEnv_ {
    const char *GetStringUTFChars(jstring s, jboolean * /*isCopy*/) {
        return s ? s->c : nullptr;
    }
    void ReleaseStringUTFChars(jstring s, const char *) { (void)s; }
};

typedef struct JNIEnv_ JNIEnv; // mirrors <jni.h>

#define JNIEXPORT
#define JNICALL

#define JNI_TRUE 1
#define JNI_FALSE 0

#endif // !__ANDROID__