// ============================================================================
// gfx_gles — image loader for the KisakCOD Android renderer backend
//
// Decodes the DDS textures the engine ships in its IWD files (the same files
// gfx_d3d/r_image_load_obj.cpp decodes on Windows) into uncompressed RGBA8,
// so they can be uploaded with glTexImage2D.
//
// Stages: DDS header -> pixel format pipe (DXT1/3/5, RGBA8, BGRA8, RGB8, L8/A8)
// -> RGBA8. Self-contained (no engine includes) so it can also be unit-tested
// on a desktop host (see android/tests).
// ============================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DdsImage {
    int width;        // base-level width
    int height;       // base-level height
    int mipCount;     // mip levels present in the file (may include 1)
    int channels;     // 4 (always RGBA after decode)
    uint8_t *data;    // width*height*4 bytes (base level only)
    size_t dataSize;
} DdsImage;

// Returns 1 on success, 0 on any malformed input. On success the caller owns
// img->data and must pass it to DdsImage_Free().
int DdsImage_Decode(const uint8_t *ddsBytes, size_t length, DdsImage *img);
void DdsImage_Free(DdsImage *img);

#ifdef __cplusplus
}
#endif