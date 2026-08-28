// ============================================================================
// gfx_gles — GLES renderer backend for KisakCOD
//
// Full implementation of the public R_* renderer API. Dispatches to the GLES
// backend (gl_backend.cpp) for 2D command execution and handles 3D scene
// rendering setup.
// ============================================================================

#include "gl_renderer.h"
#include "gl_backend.h"
#include "gles_types.h"
#include "r_image_dds.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#include <EGL/egl.h>
// Platform shim provides GfxImage struct (gfx_d3d/r_image.h) and related types
// without pulling in real <d3d9.h>. The platform include path
// (src/_platform/android) is added BEFORE by platform.cmake, so
// "gfx_d3d/r_image.h" resolves to the platform shim.
#include "gfx_d3d/r_image.h"
#else
#include "gles_stub.h"
#endif

// Provide refdef_s definition mirroring engine's gfx_d3d/r_gfx.h.
// Not guarded by __ANDROID__ — this file doesn't include engine headers that
// define refdef_s, so no ODR conflict. On Android the engine defines it
// separately in platform shim headers; that's a different TU.
struct refdef_s {
    uint32_t x, y;
    uint32_t width, height;
    float tanHalfFovX;
    float tanHalfFovY;
    float vieworg[3];
    float viewaxis[3][3];
    float viewOffset[3];
    int time;
    float zNear;
    float blurRadius;
    char _pad[0x4098 - 64];
};

// GfxImage — use the platform shim on Android (r_image.h) for consistent layout
// with engine callers; define a local stub for desktop test compiles.
#if !defined(__ANDROID__)
struct GfxImage {
    char name[64];
    uint64_t glesTexture;
    int width, height;
};
#endif

// ---- Forward declarations of engine symbols the linker resolves --------------
// These are provided by the engine build (gfx_d3d sources on Windows,
// gfx_gles sources on Android). The engine's Com_Frame code calls them.
extern "C" {

// ---- State -------------------------------------------------------------------
static int s_inited = 0;
static int s_width = 640, s_height = 480;
static GlesCmdArray s_cmdBuf; // command buffer for rendering

// ---- R_Init / Shutdown -------------------------------------------------------

void R_Init(void) {
    if (s_inited) return;
    s_inited = 1;
    fprintf(stderr, "[gfx_gles] R_Init: GLES3 backend (M3)\n");
}

void R_Shutdown(int destroyWindow) {
    (void)destroyWindow;
    if (!s_inited) return;
    GLES_Shutdown();
    s_inited = 0;
}

void R_InitThreads(void) {}
void R_InitGraphicsApi(void) {}

// ---- Frame begin/end ---------------------------------------------------------

void R_BeginFrame(void) {
    if (!s_inited) return;
    GLES_BeginFrame(s_width, s_height);
}

void R_EndFrame(void) {
    if (!s_inited) return;
    GLES_EndFrame();
}

// ---- Scene rendering ---------------------------------------------------------

void R_RenderScene(const refdef_s *refdef) {
    static int called = 0;
    if (!s_inited || !refdef) return;
    if (called < 5) { fprintf(stderr, "[gfx_gles] R_RenderScene: M3 — 2D menu path active\n"); called++; }

    s_width = refdef->width;
    s_height = refdef->height;
    glViewport(refdef->x, refdef->y, refdef->width, refdef->height);

    // Execute 2D commands from the front-end command buffer
    if (s_cmdBuf.cmds && s_cmdBuf.usedTotal > 0) {
        GLES_ExecuteCommands(&s_cmdBuf);
    }

    // NOTE: 3D scene rendering (world surfaces, models, etc.) is not
    // implemented in this M3 milestone. The GLES backend handles 2D menu
    // rendering through the command buffer above.
}

// ---- Image loading -----------------------------------------------------------

int R_LoadImageBytes(const char *name, const uint8_t *data, size_t len,
                     GfxImage *image) {
    if (!data || !image || len < 128) return 0;

    DdsImage decoded;
    if (!DdsImage_Decode(data, len, &decoded)) return 0;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, decoded.width, decoded.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, decoded.data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);

    image->glesTexture = (uint64_t)tex;
    image->width = decoded.width;
    image->height = decoded.height;

    fprintf(stderr, "[gfx_gles] Loaded: %s (%dx%d)\n", name, decoded.width, decoded.height);
    DdsImage_Free(&decoded);
    return 1;
}

// ---- Command buffer access (called by engine's R_AddCmd*) ---------------------

GlesCmdArray *GLES_GetCommandBuffer(void) {
    return &s_cmdBuf;
}

void GLES_SetCommandBuffer(GlesCmdArray *buf) {
    if (buf) s_cmdBuf = *buf;
}

// ---- Misc renderer info ------------------------------------------------------

int R_GetMaxTextureSize(void) {
    GLint mx = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mx);
    return mx > 0 ? (int)mx : 4096;
}

void R_RegisterDvars(void) {}

} // extern "C"