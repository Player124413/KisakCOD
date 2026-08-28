// KisakCOD Android port — DDS decoder tests (r_image_dds).
// Builds synthetic DXT1/DXT5 blocks and verifies the decode output.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "r_image_dds.h"

static int failures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            printf("  ok   %s\n", msg);                                        \
        } else {                                                               \
            printf("  FAIL %s  (%s:%d)\n", msg, __FILE__, __LINE__);           \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static void buildHeader(unsigned char *out, uint32_t w, uint32_t h,
                        uint32_t fourCC, uint32_t mips) {
    memset(out, 0, 128);
    memcpy(out, "DDS ", 4);
    out[4] = 124; // dwSize
    out[12] = h & 0xFF; out[13] = (h >> 8) & 0xFF;
    out[14] = (h >> 16) & 0xFF; out[15] = (h >> 24) & 0xFF;
    out[16] = w & 0xFF; out[17] = (w >> 8) & 0xFF;
    out[18] = (w >> 16) & 0xFF; out[19] = (w >> 24) & 0xFF;
    // pixel format at offset 76
    unsigned char *pf = out + 76;
    pf[0] = 32;           // dwSize
    pf[4] = 0x04;         // DDPF_FOURCC
    pf[8] = fourCC & 0xFF;
    pf[9] = (fourCC >> 8) & 0xFF;
    pf[10] = (fourCC >> 16) & 0xFF;
    pf[11] = (fourCC >> 24) & 0xFF;
    out[124 - 32 + 32] = 0;
    // caps (offset 108): texture flag
    out[108] = 0x1000;
    if (mips > 1) {
        out[28] = mips; // dwMipMapCount
        out[108] |= 0x08; // MIPMAP
    }
}

int main(void) {
    printf("== KisakCOD DDS decoder tests ==\n");

    // ---- DXT1: 4x4, red block with a 4-color ramp ----
    {
        unsigned char file[128 + 8];
        buildHeader(file, 4, 4, 0x31545844 /* DXT1 */, 1);
        // color0 = 0xF800 (red), color1 = 0x001F (blue)
        file[128 + 0] = 0x00; file[128 + 1] = 0xF8;
        file[128 + 2] = 0x1F; file[128 + 3] = 0x00;
        file[128 + 4] = 0xE4; // row0: idx 0,1,2,3
        file[128 + 5] = 0x00;
        file[128 + 6] = 0x00;
        file[128 + 7] = 0x00;

        DdsImage img;
        CHECK(DdsImage_Decode(file, sizeof(file), &img) == 1, "DXT1 decodes");
        if (img.data) {
            CHECK(img.width == 4 && img.height == 4, "size ok");
            const unsigned char *px00 = img.data + 0;              // idx0 = red
            CHECK(px00[0] == 255 && px00[1] == 0 && px00[2] == 0 && px00[3] == 255,
                  "pixel(0,0) is red");
            const unsigned char *px30 = img.data + 3 * 4;          // row0 idx3 = lerp
            CHECK(px30[0] == 85 && px30[2] == 170, "pixel(3,0) is the lerp color");
            const unsigned char *px33 = img.data + (3 + 12) * 4;   // row3 idx0
            CHECK(px33[0] == 255 && px33[3] == 255, "pixel(3,3) is red opaque");
            DdsImage_Free(&img);
        }
    }

    // ---- DXT5: 4x4, alpha ramp + color ----
    {
        unsigned char file[128 + 16];
        buildHeader(file, 4, 4, 0x35545844 /* DXT5 */, 1);
        file[128 + 0] = 255; // alpha0
        file[128 + 1] = 0;   // alpha1
        file[128 + 2] = 0x08; // pixel0=idx0, pixel1=idx1
        file[128 + 3] = 0x00;
        file[128 + 4] = 0x00;
        file[128 + 5] = 0x00;
        file[128 + 6] = 0x00;
        file[128 + 7] = 0x00;
        // color block: red/blue 4-color ramp, row0 indices 0,1,2,3
        file[128 + 8 + 0] = 0x00; file[128 + 8 + 1] = 0xF8;
        file[128 + 8 + 2] = 0x1F; file[128 + 8 + 3] = 0x00;
        file[128 + 8 + 4] = 0xE4;
        file[128 + 8 + 5] = 0x00;
        file[128 + 8 + 6] = 0x00;
        file[128 + 8 + 7] = 0x00;

        DdsImage img;
        CHECK(DdsImage_Decode(file, sizeof(file), &img) == 1, "DXT5 decodes");
        if (img.data) {
            const unsigned char *pa = img.data + 1 * 4; // pixel(1,0): idx1=alpha1=0
            CHECK(pa[3] == 0, "alpha ramp: pixel(1,0) transparent");
            const unsigned char *pb = img.data + 0;     // pixel(0,0): alpha 255, red
            CHECK(pb[0] == 255 && pb[3] == 255, "alpha ramp: pixel(0,0) opaque red");
            DdsImage_Free(&img);
        }
    }

    // ---- error handling ----
    {
        unsigned char junk[64];
        memset(junk, 0xAB, sizeof(junk));
        DdsImage img;
        CHECK(DdsImage_Decode(junk, sizeof(junk), &img) == 0, "garbage rejected");
        CHECK(DdsImage_Decode(junk, 10, &img) == 0, "short buffer rejected");
    }

    printf("----\n");
    if (failures == 0) {
        printf("DDS TESTS PASSED\n");
        return 0;
    }
    printf("%d DDS TEST(S) FAILED\n", failures);
    return 1;
}