// ============================================================================
// gfx_gles — GLES backend command types
//
// Defines the command structures that the frontend (gfx_d3d/r_rendercmds.cpp)
// writes into the command buffer and the backend (gl_backend.cpp) reads and
// executes. The layouts MUST match the engine's GfxCmd* structs byte-for-byte.
// Static_asserts in the engine build verify this.
//
// On the Android engine build, these are identical to the gfx_d3d definitions;
// we duplicate them here so the GLES backend compiles without pulling in
// gfx_d3d headers.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>

// ---- Forward declarations (mirroring engine types) ---------------------------
struct Material;
struct Font_s;
struct GfxImage;

// ---- Render command enum (from gfx_d3d/r_rendercmds.h) -----------------------
enum GfxRenderCommand : int32_t {
    RC_END_OF_LIST = 0,
    RC_SET_MATERIAL_COLOR = 1,
    RC_SAVE_SCREEN = 2,
    RC_SAVE_SCREEN_SECTION = 3,
    RC_CLEAR_SCREEN = 4,
    RC_SET_VIEWPORT = 5,
    RC_FIRST_NONCRITICAL = 6,
    RC_STRETCH_PIC = 6,
    RC_STRETCH_PIC_FLIP_ST = 7,
    RC_STRETCH_PIC_ROTATE_XY = 8,
    RC_STRETCH_PIC_ROTATE_ST = 9,
    RC_STRETCH_RAW = 10,
    RC_DRAW_QUAD_PIC = 11,
    RC_DRAW_FULL_SCREEN_COLORED_QUAD = 12,
    RC_DRAW_TEXT_2D = 13,
    RC_DRAW_TEXT_3D = 14,
    RC_BLEND_SAVED_SCREEN_BLURRED = 15,
    RC_BLEND_SAVED_SCREEN_FLASHED = 16,
    RC_DRAW_POINTS = 17,
    RC_DRAW_LINES = 18,
    RC_DRAW_TRIANGLES = 19,
    RC_DRAW_PROFILE = 20,
    RC_PROJECTION_SET = 21,
    RC_COUNT = 22,
};

// ---- GfxColor (matches union in r_gfx.h) -------------------------------------
union GfxColor {
    operator uint32_t() { return packed; }
    GfxColor() : packed(0) {}
    GfxColor(int i) : packed((uint32_t)i) {}
    GfxColor(uint32_t i) : packed(i) {}
    uint32_t packed;
    uint8_t array[4];
};

// ---- Command header (all commands start with this) ---------------------------
struct GlesCmdHeader {
    uint16_t id;
    uint16_t byteCount;
};

// ---- Individual command structures (layouts match gfx_d3d) -------------------
#pragma pack(push, 1)

struct GlesCmdStretchPic {
    GlesCmdHeader header;
    const Material *material;
    float x, y, w, h;
    float s0, t0, s1, t1;
    GfxColor color;
};

struct GlesCmdDrawQuadPic {
    GlesCmdHeader header;
    float verts[6][2];
    GfxColor color;
    const Material *material;
};

struct GlesCmdClearScreen {
    GlesCmdHeader header;
    uint8_t whichToClear;
    uint8_t stencil;
    float depth;
    float color[4];
};

struct GlesCmdSetViewport {
    GlesCmdHeader header;
    int x, y;
    int width, height;
};

struct GlesCmdDrawText2D {
    GlesCmdHeader header;
    float x, y, rotation;
    Font_s *font;
    float xScale, yScale;
    GfxColor color;
    int maxChars;
    int renderFlags;
    int cursorPos;
    char cursorLetter;
    GfxColor glowForceColor;
    int fxBirthTime;
    int fxLetterTime;
    int fxDecayStartTime;
    int fxDecayDuration;
    const Material *fxMaterial;
    const Material *fxMaterialGlow;
    float padding;
    char text[3];
};

struct GlesCmdSetMaterialColor {
    GlesCmdHeader header;
    float color[4];
};

struct GlesCmdProjectionSet {
    GlesCmdHeader header;
    int projection;
};

struct GlesCmdDrawLines {
    GlesCmdHeader header;
    float verts[256][2]; // simplified
    int pointCount;
    float width;
    float color[4];
};

struct GlesCmdFullScreenColoredQuad {
    GlesCmdHeader header;
    float s0, t0, s1, t1;
    float color[4];
    const Material *material;
};

struct GlesCmdStretchPicRotateXY {
    GlesCmdHeader header;
    const Material *material;
    float x, y, w, h;
    float s0, t0, s1, t1;
    float angle;
    GfxColor color;
};

struct GlesCmdStretchPicRotateST {
    GlesCmdHeader header;
    const Material *material;
    float x, y, w, h;
    float s0, t0, s1, t1;
    float centerS, centerT, radiusST;
    float scaleFinalS, scaleFinalT;
    float angle;
    GfxColor color;
};

#pragma pack(pop)

// ---- Command execution state (passed to each RB_* handler) --------------------
struct GlesCmdExecState {
    const void *cmd;
};

// ---- Command buffer array (mirrors GfxCmdArray) ------------------------------
struct GlesCmdArray {
    uint8_t *cmds;
    int usedTotal;
    int usedCritical;
    GlesCmdHeader *lastCmd;
};

// ---- Texture type for image struct extension ---------------------------------
// GfxImage stores the GL texture handle here
#define GFX_GLES_TEXTURE_HANDLE(image) ((uint64_t)(image)->glesTexture)