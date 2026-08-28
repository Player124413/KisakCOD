// EGL host implementation (see egl_host.h).
#include "egl_host.h"
#include "gles_stub.h"

#include <stdio.h>
#include <string.h>

EglHost::EglHost()
    : mDisplay(EGL_NO_DISPLAY), mSurface(EGL_NO_SURFACE),
      mContext(EGL_NO_CONTEXT), mConfig(0), mWidth(0), mHeight(0),
      mReady(false) {}

EglHost::~EglHost() {
    destroySurface();
}

bool EglHost::initSurface(void *nativeWindow, int width, int height) {
    if (!nativeWindow) return false;

    if (mDisplay == EGL_NO_DISPLAY) {
        mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (mDisplay == EGL_NO_DISPLAY) {
            return false;
        }
        EGLint major = 0, minor = 0;
        if (!eglInitialize(mDisplay, &major, &minor)) {
            mDisplay = EGL_NO_DISPLAY;
            return false;
        }
    }

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLint numConfigs = 0;
    if (!eglChooseConfig(mDisplay, configAttribs, (EGLConfig *)&mConfig, 1, &numConfigs) ||
        numConfigs < 1) {
        return false;
    }

    const EGLint contextAttribs[] = { 0x3098, 2, EGL_NONE }; // EGL_CONTEXT_CLIENT_VERSION 2
    mContext = eglCreateContext(mDisplay, (EGLConfig)mConfig, EGL_NO_CONTEXT, contextAttribs);
    if (mContext == EGL_NO_CONTEXT) {
        return false;
    }

    mSurface = eglCreateWindowSurface(mDisplay, (EGLConfig)mConfig,
                                      (EGLNativeWindowType)nativeWindow, NULL);
    if (mSurface == EGL_NO_SURFACE) {
        eglDestroyContext(mDisplay, mContext);
        mContext = EGL_NO_CONTEXT;
        return false;
    }

    if (!eglMakeCurrent(mDisplay, mSurface, mSurface, mContext)) {
        eglDestroySurface(mDisplay, mSurface);
        mSurface = EGL_NO_SURFACE;
        eglDestroyContext(mDisplay, mContext);
        mContext = EGL_NO_CONTEXT;
        return false;
    }

    mWidth = width;
    mHeight = height;
    mReady = true;
    return true;
}

void EglHost::resize(int width, int height) {
    mWidth = width;
    mHeight = height;
    if (mReady) {
        glViewport(0, 0, mWidth, mHeight);
    }
}

void EglHost::destroySurface() {
    if (mDisplay != EGL_NO_DISPLAY) {
        if (mSurface != EGL_NO_SURFACE) {
            eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroySurface(mDisplay, mSurface);
            mSurface = EGL_NO_SURFACE;
        }
        if (mContext != EGL_NO_CONTEXT) {
            eglDestroyContext(mDisplay, mContext);
            mContext = EGL_NO_CONTEXT;
        }
        eglTerminate(mDisplay);
        mDisplay = EGL_NO_DISPLAY;
    }
    mReady = false;
}

bool EglHost::isReady() const {
    return mReady;
}

bool EglHost::beginFrame() {
    if (!mReady) return false;
    glViewport(0, 0, mWidth, mHeight);
    return true;
}

bool EglHost::endFrame() {
    if (!mReady) return false;
    EGLBoolean ok = eglSwapBuffers(mDisplay, mSurface);
    return ok == EGL_TRUE;
}

void EglHost::invalidate() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}