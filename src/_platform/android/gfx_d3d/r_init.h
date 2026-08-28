// Android port — stub r_init.h for dedicated server builds.
#pragma once

#include "r_gfx.h"

// Forward declaration needed by r_init.h extern declarations
struct refdef_s;

struct GfxWindowParms {
    int width;
    int height;
    void* hwnd;
    void* hdc;
    int isFullscreen;
};

struct GfxConfiguration {
    uint32_t maxClientViews;
    uint32_t entCount;
    uint32_t entnumNone;
    uint32_t entnumOrdinaryEnd;
    int32_t threadContextCount;
    int32_t critSectCount;
    const char *codeFastFileName;
    const char *uiFastFileName;
};

struct vidConfig_t {
    uint32_t sceneWidth;
    uint32_t sceneHeight;
    uint32_t displayWidth;
    uint32_t displayHeight;
    uint32_t displayFrequency;
    int32_t  isFullscreen;
    float    aspectRatioWindow;
    float    aspectRatioScenePixel;
    float    aspectRatioDisplayPixel;
    uint32_t maxTextureSize;
    uint32_t maxTextureMaps;
    bool     deviceSupportsGamma;
};

// d3d9 device ptr
extern IDirect3D9* d3d9;
extern IDirect3DDevice9* dx_device;

// Extern declarations
void R_Init(void);
void R_Shutdown(int destroyWindow);
void R_BeginFrame(void);
void R_EndFrame(void);
void R_RenderScene(const refdef_s *refdef);
void R_InitGraphicsApi(void);
void R_InitThreads(void);
void R_RegisterDvars(void);