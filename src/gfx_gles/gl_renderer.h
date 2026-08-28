// ============================================================================
// gfx_gles — GLES renderer backend for KisakCOD (replaces gfx_d3d)
//
// Provides the R_Init / R_BeginFrame / R_EndFrame / R_RenderScene entry points
// and associated renderer API that the engine's client/cgame/ui code expects.
// ============================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

// Forward declaration of the engine's refdef_s
struct refdef_s;

#ifdef __cplusplus
extern "C" {
#endif

void  R_Init(void);
void  R_Shutdown(int destroyWindow);
void  R_BeginFrame(void);
void  R_EndFrame(void);
void  R_RenderScene(const refdef_s *refdef);
void  R_InitThreads(void);
void  R_InitGraphicsApi(void);
void  R_RegisterDvars(void);
void  R_SyncRenderThread(void);
int   R_GetRendererType(void);
int   R_GetMaxTextureSize(void);

// Image loading
struct GfxImage;
int   R_LoadImageBytes(const char *name, const uint8_t *data, size_t len,
                       struct GfxImage *image);

#ifdef __cplusplus
}
#endif