// EGL host: owns the rendering surface lifecycle for the game activity.
// Platform-neutral (uses gles_stub.h types); on Android the JNI layer feeds it
// an ANativeWindow.
#pragma once

#include <stddef.h>

struct ANativeWindowStub {}; // replaced by ANativeWindow on Android

class EglHost {
public:
    EglHost();
    ~EglHost();

    // window is an ANativeWindow* on Android (void* here for portability
    // of the declaration). Returns true on success.
    bool initSurface(void *nativeWindow, int width, int height);
    void resize(int width, int height);
    void destroySurface();
    bool isReady() const;
    int width() const { return mWidth; }
    int height() const { return mHeight; }

    bool beginFrame();   // make current + clear
    bool endFrame();     // swap buffers
    void invalidate();

private:
    void *mDisplay;
    void *mSurface;
    void *mContext;
    void *mConfig;
    int mWidth;
    int mHeight;
    bool mReady;
};