// Shell renderer: a tiny GLES2 demo scene that runs when the APK is built
// without the engine (KISAK_ENGINE_LINKED=OFF). It drives the same surface /
// frame loop the engine will use and visualizes the touch input, so the
// overlay + JNI wiring can be tested on any device before the engine is linked.
#pragma once

#include "gles_stub.h"
#include "gl_shader.h"

struct ShellInputState {
    // movement stick (normalized)
    float moveX = 0.f, moveY = 0.f;
    // look stick (normalized)
    float lookX = 0.f, lookY = 0.f;
    // button state (true = pressed)
    bool fire = false, ads = false, jump = false, sprint = false;
    bool crouch = false, prone = false, reload = false, melee = false;
    bool use = false, nade = false, score = false, pause = false;
    // cursors
    float cursorX = 0.f, cursorY = 0.f;
    bool touchEnabled = true;
};

struct ShellRender {
    Shader shader;
    GLuint vbo = 0;
    bool ready = false;
    int width = 0, height = 0;
    float anim = 0.f;

    bool init(int w, int h);
    void resize(int w, int h);
    void draw(float dtMs, const ShellInputState &state);
    void destroy();
};