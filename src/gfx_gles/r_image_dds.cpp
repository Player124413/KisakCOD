// ============================================================================
// gfx_gles — DDS image decoder (see r_image_dds.h)
// ============================================================================

#include "r_image_dds.h"

#include <stdlib.h>
#include <string.h>

static const uint32_t FOURCC_DXT1 = 0x31545844; // "DXT1"
static const uint32_t FOURCC_DXT3 = 0x33545844; // "DXT3"
static const uint32_t FOURCC_DXT5 = 0x35545844; // "DXT5"

// DDPF flags
#define DDPF_ALPHAPIXELS 0x00000001
#define DDPF_FOURCC      0x00000004
#define DDPF_RGB         0x00000040
#define DDPF_LUMINANCE   0x00020000

#pragma pack(push, 1)
typedef struct DdsPixelFormat {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
} DdsPixelFormat;

typedef struct DdsHeader {
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DdsPixelFormat format;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
} DdsHeader;
#pragma pack(pop)

// ------------------------------------------------------------------ helpers

static uint16_t rdU16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static void rgb565To888(uint16_t c, uint8_t out[3]) {
    uint32_t r = (c >> 11) & 0x1F;
    uint32_t g = (c >> 5) & 0x3F;
    uint32_t b = c & 0x1F;
    out[0] = (uint8_t)((r * 255 + 15) / 31);
    out[1] = (uint8_t)((g * 255 + 31) / 63);
    out[2] = (uint8_t)((b * 255 + 15) / 31);
}

// decode one DXT1 color block into colors[4][4] (rgba; may be transparent)
static void decodeColorBlock(const uint8_t *src, uint8_t colors[4][4]) {
    uint16_t c0 = rdU16(src + 0);
    uint16_t c1 = rdU16(src + 2);
    uint8_t rgb0[3], rgb1[3];
    rgb565To888(c0, rgb0);
    rgb565To888(c1, rgb1);

    if (c0 > c1) {
        // 4-color mode
        for (int i = 0; i < 3; i++) {
            colors[0][i] = rgb0[i];
            colors[1][i] = rgb1[i];
            colors[2][i] = (uint8_t)((2 * rgb0[i] + rgb1[i]) / 3);
            colors[3][i] = (uint8_t)((rgb0[i] + 2 * rgb1[i]) / 3);
        }
        colors[0][3] = colors[1][3] = colors[2][3] = colors[3][3] = 255;
    } else {
        // 3-color + transparent
        for (int i = 0; i < 3; i++) {
            colors[0][i] = rgb0[i];
            colors[1][i] = rgb1[i];
            colors[2][i] = (uint8_t)((rgb0[i] + rgb1[i]) / 2);
        }
        colors[0][3] = colors[1][3] = colors[2][3] = 255;
        colors[3][0] = colors[3][1] = colors[3][2] = colors[3][3] = 0;
    }
}

static void decodeDxt1(const uint8_t *src, int w, int h, uint8_t *dst) {
    for (int by = 0; by < h; by += 4) {
        for (int bx = 0; bx < w; bx += 4) {
            uint8_t colors[4][4];
            decodeColorBlock(src, colors);
            const uint8_t *idx = src + 4;
            src += 8;
            for (int py = 0; py < 4; py++) {
                for (int px = 0; px < 4; px++) {
                    const int xi = bx + px;
                    const int yi = by + py;
                    if (xi >= w || yi >= h) continue;
                    const int ci = (idx[py] >> (px * 2)) & 3;
                    uint8_t *o = dst + ((size_t)yi * w + xi) * 4;
                    o[0] = colors[ci][0];
                    o[1] = colors[ci][1];
                    o[2] = colors[ci][2];
                    o[3] = colors[ci][3];
                }
            }
        }
    }
}

// BC2/BC3 color block: always the 4-color ramp (index 3 is never transparent).
static void decodeColorMode4(const uint8_t *src, uint8_t colors[4][4],
                             uint8_t idx[16]) {
    uint16_t c0 = rdU16(src + 0);
    uint16_t c1 = rdU16(src + 2);
    uint8_t rgb0[3], rgb1[3];
    rgb565To888(c0, rgb0);
    rgb565To888(c1, rgb1);
    for (int i = 0; i < 3; i++) {
        colors[0][i] = rgb0[i];
        colors[1][i] = rgb1[i];
        colors[2][i] = (uint8_t)((2 * rgb0[i] + rgb1[i]) / 3);
        colors[3][i] = (uint8_t)((rgb0[i] + 2 * rgb1[i]) / 3);
    }
    colors[0][3] = colors[1][3] = colors[2][3] = colors[3][3] = 255;
    for (int y = 0; y < 4; y++) {
        const uint8_t byte = src[4 + y];
        for (int x = 0; x < 4; x++) {
            idx[y * 4 + x] = (byte >> (x * 2)) & 3;
        }
    }
}

static void decodeDxt3(const uint8_t *src, int w, int h, uint8_t *dst) {
    for (int by = 0; by < h; by += 4) {
        for (int bx = 0; bx < w; bx += 4) {
            const uint8_t *alpha = src;   // 8 bytes: 4-bit per pixel
            const uint8_t *color = src + 8;
            src += 16;
            uint8_t colors[4][4];
            uint8_t idx[16];
            decodeColorMode4(color, colors, idx);
            for (int py = 0; py < 4; py++) {
                for (int px = 0; px < 4; px++) {
                    const int xi = bx + px;
                    const int yi = by + py;
                    if (xi >= w || yi >= h) continue;
                    const int pi = py * 4 + px;
                    const uint8_t a = (uint8_t)((alpha[pi / 2] >> ((pi & 1) * 4)) & 0xF);
                    const int ci = idx[pi];
                    uint8_t *o = dst + ((size_t)yi * w + xi) * 4;
                    o[0] = colors[ci][0];
                    o[1] = colors[ci][1];
                    o[2] = colors[ci][2];
                    o[3] = (uint8_t)(a * 17); // 4-bit -> 8-bit
                }
            }
        }
    }
}

static void decodeDxt5(const uint8_t *src, int w, int h, uint8_t *dst) {
    for (int by = 0; by < h; by += 4) {
        for (int bx = 0; bx < w; bx += 4) {
            const uint8_t a0 = src[0];
            const uint8_t a1 = src[1];
            // 48 bits of 3-bit indices, LSB first
            uint64_t bits = 0;
            for (int i = 0; i < 6; i++) {
                bits |= (uint64_t)src[2 + i] << (8 * i);
            }
            const uint8_t *color = src + 8;
            src += 16;

            uint8_t alphas[8];
            if (a0 > a1) {
                alphas[0] = a0;
                alphas[1] = a1;
                for (int i = 1; i < 7; i++) {
                    alphas[i + 1] = (uint8_t)(((7 - i) * a0 + i * a1) / 7);
                }
            } else {
                alphas[0] = a0;
                alphas[1] = a1;
                for (int i = 1; i < 5; i++) {
                    alphas[i + 1] = (uint8_t)(((5 - i) * a0 + i * a1) / 5);
                }
                alphas[6] = 0;
                alphas[7] = 255;
            }

            uint8_t colors[4][4];
            uint8_t idx[16];
            decodeColorMode4(color, colors, idx);
            for (int py = 0; py < 4; py++) {
                for (int px = 0; px < 4; px++) {
                    const int xi = bx + px;
                    const int yi = by + py;
                    if (xi >= w || yi >= h) continue;
                    const int pi = py * 4 + px;
                    const int ai = (int)((bits >> (pi * 3)) & 7);
                    uint8_t *o = dst + ((size_t)yi * w + xi) * 4;
                    o[0] = colors[idx[pi]][0];
                    o[1] = colors[idx[pi]][1];
                    o[2] = colors[idx[pi]][2];
                    o[3] = alphas[ai];
                }
            }
        }
    }
}

// ------------------------------------------------------------------ entry point

int DdsImage_Decode(const uint8_t *dds, size_t length, DdsImage *img) {
    if (!dds || !img || length < sizeof(DdsHeader)) return 0;
    memset(img, 0, sizeof(*img));

    const DdsHeader *h = (const DdsHeader *)dds;
    if (h->magic != 0x20534444u) return 0; // "DDS "
    if (h->size != 124) return 0;
    if (h->width == 0 || h->height == 0) return 0;
    if (h->width > 16384 || h->height > 16384) return 0;

    const uint32_t w = h->width;
    const uint32_t ht = h->height;
    const uint32_t mips = h->mipMapCount ? h->mipMapCount : 1;

    img->width = (int)w;
    img->height = (int)ht;
    img->mipCount = (int)mips;

    const size_t pixelBytes = (size_t)w * ht * 4;
    uint8_t *out = (uint8_t *)malloc(pixelBytes);
    if (!out) return 0;

    const DdsPixelFormat *pf = &h->format;
    const bool ok = [&]() -> bool {
        if (pf->flags & DDPF_FOURCC) {
            switch (pf->fourCC) {
                case FOURCC_DXT1: decodeDxt1(dds + 128, (int)w, (int)ht, out); break;
                case FOURCC_DXT3: decodeDxt3(dds + 128, (int)w, (int)ht, out); break;
                case FOURCC_DXT5: decodeDxt5(dds + 128, (int)w, (int)ht, out); break;
                default: return false;
            }
            return true;
        }
        if (pf->flags & DDPF_RGB) {
            const uint32_t bits = pf->rgbBitCount;
            const uint8_t *src = dds + 128;
            if (bits == 32 && pf->aBitMask) {
                for (size_t i = 0; i < pixelBytes; i += 4) {
                    out[i + 0] = src[i + 2]; // BGRA->RGBA (common DDS layout)
                    out[i + 1] = src[i + 1];
                    out[i + 2] = src[i + 0];
                    out[i + 3] = src[i + 3];
                }
                return true;
            }
            if (bits == 32) { // BGRA with alpha flag
                for (size_t i = 0; i < pixelBytes; i += 4) {
                    out[i + 0] = src[i + 2];
                    out[i + 1] = src[i + 1];
                    out[i + 2] = src[i + 0];
                    out[i + 3] = 255;
                }
                return true;
            }
            if (bits == 24) { // BGR
                for (size_t i = 0, s = 0; i < pixelBytes; i += 4, s += 3) {
                    out[i + 0] = src[s + 2];
                    out[i + 1] = src[s + 1];
                    out[i + 2] = src[s + 0];
                    out[i + 3] = 255;
                }
                return true;
            }
            if (bits == 8 && (pf->flags & DDPF_LUMINANCE)) {
                for (size_t i = 0; i < pixelBytes; i += 4) {
                    out[i + 0] = out[i + 1] = out[i + 2] = src[i / 4];
                    out[i + 3] = 255;
                }
                return true;
            }
            return false;
        }
        // alpha-only (A8)
        if ((pf->flags & DDPF_ALPHAPIXELS) && pf->rgbBitCount == 8 && !(pf->flags & DDPF_RGB)) {
            const uint8_t *src = dds + 128;
            for (size_t i = 0; i < pixelBytes; i += 4) {
                out[i + 0] = 255;
                out[i + 1] = 255;
                out[i + 2] = 255;
                out[i + 3] = src[i / 4];
            }
            return true;
        }
        return false;
    }();

    if (!ok) {
        free(out);
        return 0;
    }

    img->data = out;
    img->dataSize = pixelBytes;
    return 1;
}

void DdsImage_Free(DdsImage *img) {
    if (!img) return;
    free(img->data);
    img->data = NULL;
    img->dataSize = 0;
}