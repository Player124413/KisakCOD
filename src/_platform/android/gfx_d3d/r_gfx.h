// Android port — stub r_gfx.h for dedicated server builds.
// Provides the type declarations that non-renderer code (EffectsCore, DynEntity,
// cgame, etc.) needs without pulling in <d3d9.h>.
#pragma once

#include <cstdint>

// D3D9 types that the engine's Gfx structures reference
struct IDirect3DVertexBuffer9 {};
struct IDirect3DIndexBuffer9 {};
struct IDirect3DTexture9 {};
struct IDirect3DBaseTexture9 {};
struct IDirect3DQuery9 {};
struct IDirect3DSurface9 {};
struct IDirect3DDevice9 {};
struct IDirect3D9 {};
struct D3DMATRIX {};

// GfxColor
union GfxColor {
    operator uint32_t() { return packed; }
    GfxColor() : packed(0) {}
    GfxColor(int i) : packed((uint32_t)i) {}
    GfxColor(uint32_t i) : packed(i) {}
    uint32_t packed;
    uint8_t array[4];
};

// PackedTexCoords, PackedUnitVec (from com_pack.h)
struct PackedTexCoords { uint32_t v; };
struct PackedUnitVec { uint32_t v; };

// GfxPackedVertex
struct GfxPackedVertex {
    float xyz[3];
    float binormalSign;
    GfxColor color;
    PackedTexCoords texCoord;
    PackedUnitVec normal;
    PackedUnitVec tangent;
};

// GfxPackedVertexNormal
struct GfxPackedVertexNormal {
    PackedUnitVec normal;
    PackedUnitVec tangent;
};

// GfxDynamicIndices
struct GfxDynamicIndices {
    volatile int used;
    int total;
    uint16_t* indices;
};

// GfxVertexBufferState
struct GfxVertexBufferState {
    volatile uint32_t used;
    int total;
    IDirect3DVertexBuffer9* buffer;
    uint8_t* verts;
};

// GfxMeshData
struct GfxMeshData {
    uint32_t indexCount;
    uint32_t totalIndexCount;
    uint16_t* indices;
    GfxVertexBufferState vb;
    uint32_t vertSize;
};

// GfxMatrix
struct GfxMatrix {
    float m[4][4];
};

// GfxPlacement
struct GfxPlacement {
    float quat[4];
    float origin[3];
};

// GfxScaledPlacement
struct GfxScaledPlacement {
    GfxPlacement base;
    float scale;
};

// Other types the engine references
struct Material;
struct XModel;
struct GfxImage;
struct GfxLight;
struct GfxWorld;
struct GfxScene;
struct GfxDrawSurf;

// Vertex buffer constants
#define NULL_VERTEX_BUFFER 0

// D3D enums (stubs - never used in dedicated server code paths)
enum D3DPRIMITIVETYPE {};
enum D3DFORMAT {};
enum _D3DCULL {};