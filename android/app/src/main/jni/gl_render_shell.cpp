#include "gl_render_shell.h"

#include "and_sys.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *kVs =
    "attribute vec4 aPos;"
    "attribute vec4 aCol;"
    "varying vec4 vCol;"
    "void main(){ gl_Position = aPos; vCol = aCol; }";

static const char *kFs =
    "precision mediump float;"
    "varying vec4 vCol;"
    "void main(){ gl_FragColor = vCol; }";

struct Vert { float x, y, r, g, b, a; };

static void pushQuad(Vert *vb, int &n, float x0, float y0, float x1, float y1,
                     float r, float g, float b, float a) {
    Vert v[6] = {
        { x0, y0, r, g, b, a },
        { x1, y0, r, g, b, a },
        { x1, y1, r, g, b, a },
        { x0, y0, r, g, b, a },
        { x1, y1, r, g, b, a },
        { x0, y1, r, g, b, a },
    };
    memcpy(vb + n, v, sizeof(v));
    n += 6;
}

bool ShellRender::init(int w, int h) {
    width = w > 0 ? w : 1;
    height = h > 0 ? h : 1;
    if (!shader.init(kVs, kFs)) {
        AndroidSys_Log("kisakcod", "shell shader init failed");
        return false;
    }
    glGenBuffers(1, &vbo);
    ready = true;
    return true;
}

void ShellRender::resize(int w, int h) {
    width = w > 0 ? w : 1;
    height = h > 0 ? h : 1;
}

void ShellRender::draw(float dtMs, const ShellInputState &st) {
    if (!ready) return;
    anim += dtMs / 1000.f;

    glViewport(0, 0, width, height);
    glClearColor(0.02f, 0.03f, 0.025f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // ---- build geometry in NDC ----
    Vert vb[512];
    int n = 0;
    const float w = 2.f / width;
    const float h = 2.f / height;

    // background gradient (dark green top -> black bottom)
    pushQuad(vb, n, -1, -1, 1, 1,
             0.05f, 0.09f, 0.07f, 1.0f);

    // left stick base + knob
    const float sBase = 0.16f;
    const float sx = w * (width * 0.12f);
    const float sy = h * (height * 0.72f);
    pushQuad(vb, n, -sx - sBase, -sy - sBase, -sx + sBase, -sy + sBase,
             0.20f, 0.25f, 0.22f, 0.85f);
    const float kx = st.moveX * sBase * 0.7f;
    const float ky = st.moveY * sBase * 0.7f;
    pushQuad(vb, n, -sx + kx - sBase * 0.45f, -sy + ky - sBase * 0.45f,
             -sx + kx + sBase * 0.45f, -sy + ky + sBase * 0.45f,
             0.75f, 0.78f, 0.60f, 0.95f);

    // right stick base + knob
    const float ox = w * (width * 0.88f);
    const float oy = h * (height * 0.72f);
    pushQuad(vb, n, ox - sBase, oy - sBase, ox + sBase, oy + sBase,
             0.16f, 0.18f, 0.20f, 0.85f);
    pushQuad(vb, n, ox + st.lookX * sBase * 0.7f - sBase * 0.4f,
             oy + st.lookY * sBase * 0.7f - sBase * 0.4f,
             ox + st.lookX * sBase * 0.7f + sBase * 0.4f,
             oy + st.lookY * sBase * 0.7f + sBase * 0.4f,
             0.60f, 0.75f, 0.65f, 0.95f);

    // button indicators along the top
    const bool btns[11] = {
        st.fire, st.ads, st.jump, st.sprint, st.crouch, st.prone,
        st.reload, st.melee, st.use, st.nade, st.score,
    };
    const float bw = 0.10f;
    for (int i = 0; i < 11; i++) {
        const float bx = -1.f + (float)i * (2.f / 11.f);
        if (btns[i]) {
            pushQuad(vb, n, bx, 0.90f, bx + bw, 0.98f,
                     0.90f, 0.55f, 0.20f, 1.0f);
        } else {
            pushQuad(vb, n, bx, 0.90f, bx + bw, 0.98f,
                     0.10f, 0.12f, 0.11f, 0.8f);
        }
    }

    // crosshair (moves with the look stick); state is in 0..1 UI space
    float cx = st.cursorX * 2.f - 1.f;
    float cy = 1.f - st.cursorY * 2.f;
    const float cw = 0.012f;
    pushQuad(vb, n, cx - cw, cy - cw * 3.f, cx + cw, cy + cw * 3.f,
             1.0f, 0.9f, 0.7f, 0.9f);
    pushQuad(vb, n, cx - cw * 3.f, cy - cw, cx + cw * 3.f, cy + cw,
             1.0f, 0.9f, 0.7f, 0.9f);

    // touch-off banner
    if (!st.touchEnabled) {
        pushQuad(vb, n, -0.5f, 0.82f, 0.5f, 0.88f,
                 0.85f, 0.4f, 0.1f, 0.9f);
    }

    // ---- draw ----
    shader.use();
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(n * sizeof(Vert)), vb, GL_STATIC_DRAW);

    GLint aPos = glGetAttribLocation(shader.program, "aPos");
    GLint aCol = glGetAttribLocation(shader.program, "aCol");
    glEnableVertexAttribArray((GLuint)aPos);
    glVertexAttribPointer((GLuint)aPos, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)0);
    glEnableVertexAttribArray((GLuint)aCol);
    glVertexAttribPointer((GLuint)aCol, 4, GL_FLOAT, GL_FALSE, sizeof(Vert),
                          (void *)(2 * sizeof(float)));

    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLES, 0, n);

    glDisableVertexAttribArray((GLuint)aPos);
    glDisableVertexAttribArray((GLuint)aCol);
}

void ShellRender::destroy() {
    ready = false;
    shader.destroy();
}