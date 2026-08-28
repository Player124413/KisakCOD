// ============================================================================
// gfx_gles — GLES renderer backend for KisakCOD (see gl_renderer.h)
//
// M3 WIP. This translation unit is the anchor point where gfx_d3d is replaced
// in the engine build (scripts/platform/android/platform.cmake). The functions
// below are the *boot + menu* slice of the R_* API; every other function the
// engine references is listed against gfx_d3d sources in ENGINE_PORT.md.
// ============================================================================

#include "gl_renderer.h"
#include "r_image_dds.h"

#include <stdio.h>
#include <string.h>

// gles entry points; on desktop these come from android/app/src/main/jni/gles_stub.h
#include "gles_stub.h"

static int s_inited = 0;
static int s_frameWidth = 0;
static int s_frameHeight = 0;

// ------------------------------------------------------------------ boot

void GL_R_RegisterDvars(void) {
    // Dvar registration happens engine-side; the GLES backend adds its own
    // settings (r_sceneScale etc.) here once the dvar API is linked (M3).
}

void GL_R_Init(void) {
    if (s_inited) return;
    s_inited = 1;
    // The EGL surface/context is created by the JNI host (egl_host.cpp).
    // Everything below only needs the context to be current on this thread.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    fprintf(stderr, "[gfx_gles] GL_R_Init: backend active (M3 slice)\n");
}

void GL_R_Shutdown(int /*destroyWindow*/) {
    s_inited = 0;
}

// ------------------------------------------------------------------ frames

void GL_R_BeginFrame(void) {
    if (!s_inited) return;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GL_R_EndFrame(void) {
    // The host swaps buffers after Com_Frame returns (jni/host.cpp).
}

void GL_R_RenderScene(const GlRefdef *refdef) {
    if (!s_inited || !refdef) return;
    s_frameWidth = (int)refdef->viewport[2];
    s_frameHeight = (int)refdef->viewport[3];
    glViewport((GLint)refdef->viewport[0], (GLint)refdef->viewport[1],
               (GLsizei)refdef->viewport[2], (GLsizei)refdef->viewport[3]);

    // ---- M3: replace with the real scene passes ----
    // gfx_d3d splits this into: DPVS (r_dpvs.cpp) -> frontend surface
    // collection (r_add_*.cpp) -> backend draw (rb_backend.cpp) -> technique
    // pipeline (r_draw_*.cpp / rb_shade.cpp). The GLES port re-implements
    // that pipeline against the same GfxDrawSurfList structures, replacing
    // D3D9 state with GLES3 state.
    glClearColor(0.02f, 0.03f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    (void)refdef;
}

int GL_R_Status(void) {
    return s_inited ? 1 : 0;
}