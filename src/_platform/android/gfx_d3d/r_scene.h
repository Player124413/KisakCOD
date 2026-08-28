// Android port — stub r_scene.h for dedicated server builds.
#pragma once

#include "r_gfx.h"

struct refdef_s;
struct GfxViewParms;

struct refdef_s {
    int x, y;
    int width, height;
    float fov_x, fov_y;
    float tanHalfFovX, tanHalfFovY;
    float vieworg[3];
    float viewaxis[3][3];
};