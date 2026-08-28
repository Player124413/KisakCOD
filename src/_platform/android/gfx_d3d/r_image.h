// Android port — full GfxImage struct for GLES rendering backend
#pragma once

#include "r_gfx.h"
#include "r_material.h"

struct GfxImage {
    char name[64];
    uint64_t glesTexture; // GL texture handle (extension for GLES build)
    int width;
    int height;
    int depth;
    int flags;
    int category;
    int semantic;
    int imageTrack;
    void *pixelData;
};

struct GfxTexFormat {
    int format;
    int internalFormat;
    int type;
};

struct GfxTextureState {
    int minFilter;
    int magFilter;
    int wrapS;
    int wrapT;
    int anisoLevel;
};