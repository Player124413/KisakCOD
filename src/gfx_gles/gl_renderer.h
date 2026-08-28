// ============================================================================
// gfx_gles — GLES renderer backend for KisakCOD (replaces gfx_d3d)
//
// Maintains API parity with the renderer surface the client/cgame/ui code
// expects (the R_RenderScene / R_BeginFrame / R_EndFrame entry points and the
// R_AddCmd* draw-command registration the UI uses). The D3D9 dev paths
// (postfx, sun shadows, DPVS surface passes) are re-implemented pass by pass;
// see ENGINE_PORT.md for the status map.
//
// Compiles standalone on desktop for syntax checks; on Android it uses the
// EGL context created by the JNI host (see android/app/src/main/jni/egl_host.*)
// ============================================================================
#pragma once

#include <stdint.h>

// Type mirror of the refdef the engine passes to R_RenderScene (subset used
// by the menu/HUD path; full mirror lives in the M3 header contract).
typedef struct GlRefdef {
    float x, y, width, height;
    float fovX, fovY;
    float tanHalfFovX, tanHalfFovY;
    float vieworg[3];
    float viewaxis[3][3];
    float viewport[4];        // x, y, w, h (px)
    int   drawScene;
    int   rendererType;
    float time;
    float blurRadius;
} GlRefdef;

#ifdef __cplusplus
extern "C" {
#endif

void  GL_R_Init(void);
void  GL_R_Shutdown(int destroyWindow);
void  GL_R_BeginFrame(void);
void  GL_R_EndFrame(void);
void  GL_R_RenderScene(const GlRefdef *refdef);

// Image/material loading entry points used by asset loading (see r_image_dds.h)
void  GL_R_RegisterDvars(void);

// Status helpers (undefined/implemented) — printed at boot for diagnostics.
int   GL_R_Status(void);

#ifdef __cplusplus
}
#endif