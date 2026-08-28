// Android port — stub r_drawsurf.h for dedicated server builds.
#pragma once

#include "r_gfx.h"

struct GfxDrawSurfList;
struct GfxDrawSurf;
struct GfxSurface;

// Surface types
struct srfTriangles_t {
    int vertexLayerData;
    int firstVertex;
    uint16_t vertexCount;
    uint16_t triCount;
    int baseIndex;
};