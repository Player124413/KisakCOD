// ============================================================================
// gfx_gles — GLES3 backend command executor (see gl_backend.h)
//
// Processes the command buffer produced by the engine's R_AddCmd* frontend.
// Each command (identified by GfxRenderCommand enum) is dispatched through the
// s_cmdTable function table. GLES3 state management and draw calls here.
// ============================================================================

#include "gl_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#else
#include "gles_stub.h"
// Stub engine types for desktop compilation
struct Material {}; // gfx_d3d/r_material.h – opaque
struct Font_s {
    const char *fontName;
    int pixelHeight;
    int glyphCount;
    Material *material;
    Material *glowMaterial;
    void *glyphs;
};
#endif

// ---- forward declarations ---------------------------------------------------
#define GLES_CMD_DECL(name) static void name(GlesCmdExecState *es)

GLES_CMD_DECL(gles_SetMaterialColor);
GLES_CMD_DECL(gles_SaveScreen);
GLES_CMD_DECL(gles_SaveScreenSection);
GLES_CMD_DECL(gles_ClearScreen);
GLES_CMD_DECL(gles_SetViewport);
GLES_CMD_DECL(gles_StretchPic);
GLES_CMD_DECL(gles_StretchPicFlipST);
GLES_CMD_DECL(gles_StretchPicRotateXY);
GLES_CMD_DECL(gles_StretchPicRotateST);
GLES_CMD_DECL(gles_StretchRaw);
GLES_CMD_DECL(gles_DrawQuadPic);
GLES_CMD_DECL(gles_FullScreenColoredQuad);
GLES_CMD_DECL(gles_DrawText2D);
GLES_CMD_DECL(gles_DrawText3D);
GLES_CMD_DECL(gles_BlendSavedBlurred);
GLES_CMD_DECL(gles_BlendSavedFlashed);
GLES_CMD_DECL(gles_DrawPoints);
GLES_CMD_DECL(gles_DrawLines);
GLES_CMD_DECL(gles_DrawTriangles);
GLES_CMD_DECL(gles_DrawProfile);
GLES_CMD_DECL(gles_ProjectionSet);

// ---- command table -----------------------------------------------------------
static void (*s_cmdTable[RC_COUNT])(GlesCmdExecState *) = {
    NULL,                          // RC_END_OF_LIST
    gles_SetMaterialColor,         // RC_SET_MATERIAL_COLOR
    gles_SaveScreen,               // RC_SAVE_SCREEN
    gles_SaveScreenSection,        // RC_SAVE_SCREEN_SECTION
    gles_ClearScreen,              // RC_CLEAR_SCREEN
    gles_SetViewport,              // RC_SET_VIEWPORT
    gles_StretchPic,               // RC_STRETCH_PIC
    gles_StretchPicFlipST,         // RC_STRETCH_PIC_FLIP_ST
    gles_StretchPicRotateXY,       // RC_STRETCH_PIC_ROTATE_XY
    gles_StretchPicRotateST,       // RC_STRETCH_PIC_ROTATE_ST
    gles_StretchRaw,               // RC_STRETCH_RAW
    gles_DrawQuadPic,              // RC_DRAW_QUAD_PIC
    gles_FullScreenColoredQuad,    // RC_DRAW_FULL_SCREEN_COLORED_QUAD
    gles_DrawText2D,               // RC_DRAW_TEXT_2D
    gles_DrawText3D,               // RC_DRAW_TEXT_3D
    gles_BlendSavedBlurred,        // RC_BLEND_SAVED_SCREEN_BLURRED
    gles_BlendSavedFlashed,        // RC_BLEND_SAVED_SCREEN_FLASHED
    gles_DrawPoints,               // RC_DRAW_POINTS
    gles_DrawLines,                // RC_DRAW_LINES
    gles_DrawTriangles,            // RC_DRAW_TRIANGLES
    gles_DrawProfile,              // RC_DRAW_PROFILE
    gles_ProjectionSet,            // RC_PROJECTION_SET
};

// ---- GLES state --------------------------------------------------------------
static struct {
    int w, h;
    float curColor[4];
    int projection; // 0=2D, 1=3D
} s_state;

// ---- shader programs ---------------------------------------------------------
static GLuint s_shader2D = 0;
static GLuint s_shaderFont = 0;
static GLuint s_vao = 0;
static GLuint s_vbo = 0;

static const char *kVs2D =
    "#version 300 es\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aUV;\n"
    "layout(location=2) in vec4 aCol;\n"
    "out vec2 vUV;\n"
    "out vec4 vCol;\n"
    "uniform vec4 uVP;\n"
    "void main() {\n"
    "  vec2 n = (aPos - uVP.xy) / uVP.zw * 2.0 - 1.0;\n"
    "  gl_Position = vec4(n.x, -n.y, 0.0, 1.0);\n"
    "  vUV = aUV; vCol = aCol;\n"
    "}\n";

static const char *kFs2D =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 vUV; in vec4 vCol;\n"
    "uniform sampler2D uTex;\n"
    "uniform bool uHasTex;\n"
    "out vec4 oCol;\n"
    "void main() {\n"
    "  vec4 t = uHasTex ? texture(uTex, vUV) : vec4(1.0);\n"
    "  oCol = t * vCol;\n"
    "}\n";

static const char *kFsFont =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 vUV; in vec4 vCol;\n"
    "uniform sampler2D uTex;\n"
    "out vec4 oCol;\n"
    "void main() {\n"
    "  float a = texture(uTex, vUV).r;\n"
    "  oCol = vec4(vCol.rgb, vCol.a * a);\n"
    "}\n";

struct GlesVert2D { float x, y, u, v; uint8_t r, g, b, a; };

static GLuint compileShader(GLenum t, const char *src) {
    GLuint s = glCreateShader(t);
    if (!s) return 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; GLsizei l=0; glGetShaderInfoLog(s,512,&l,log); fprintf(stderr,"[gles] shader: %.*s\n",l,log); glDeleteShader(s); return 0; }
    return s;
}

static GLuint makeProg(const char *vs, const char *fs) {
    GLuint v=compileShader(GL_VERTEX_SHADER,vs), f=compileShader(GL_FRAGMENT_SHADER,fs);
    if (!v||!f) return 0;
    GLuint p=glCreateProgram(); glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
    GLint ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if (!ok) { char l[512]; GLsizei n=0; glGetProgramInfoLog(p,512,&n,l); fprintf(stderr,"[gles] link: %.*s\n",n,l); glDeleteProgram(p); p=0; }
    glDeleteShader(v); glDeleteShader(f); return p;
}

static void ensureRes() {
    if (s_shader2D) return;
    s_shader2D = makeProg(kVs2D, kFs2D);
    s_shaderFont = makeProg(kVs2D, kFsFont);
    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);
    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GlesVert2D), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GlesVert2D), (void*)8);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GlesVert2D), (void*)16);
    glBindVertexArray(0);
}

static void drawQuad(float x, float y, float w, float h,
                     float s0, float t0, float s1, float t1,
                     const float *c, GLuint tex, bool useTex, bool flip) {
    ensureRes();
    if (!s_shader2D) return;
    uint8_t r=(uint8_t)(c[0]*255), g=(uint8_t)(c[1]*255),
            b=(uint8_t)(c[2]*255), a=(uint8_t)(c[3]*255);
    if (flip) { float tmp=s0; s0=s1; s1=tmp; }
    float x2=x+w, y2=y+h;
    GlesVert2D vs[6] = {
        {x,y, s0,t0, r,g,b,a}, {x2,y, s1,t0, r,g,b,a}, {x,y2, s0,t1, r,g,b,a},
        {x,y2, s0,t1, r,g,b,a}, {x2,y, s1,t0, r,g,b,a}, {x2,y2, s1,t1, r,g,b,a},
    };
    glUseProgram(s_shader2D);
    glUniform4f(glGetUniformLocation(s_shader2D, "uVP"), 0,0,(float)s_state.w,(float)s_state.h);
    glUniform1i(glGetUniformLocation(s_shader2D, "uHasTex"), useTex?1:0);
    if (useTex && tex) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(glGetUniformLocation(s_shader2D, "uTex"), 0); }
    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vs), vs, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ---- command handlers --------------------------------------------------------

void gles_SetMaterialColor(GlesCmdExecState *es) {
    auto *c = (const GlesCmdSetMaterialColor *)es->cmd;
    for (int i=0; i<4; i++) s_state.curColor[i] = c->color[i];
}

void gles_SaveScreen(GlesCmdExecState*) {}
void gles_SaveScreenSection(GlesCmdExecState*) {}
void gles_StretchRaw(GlesCmdExecState*) {}
void gles_DrawText3D(GlesCmdExecState*) {}
void gles_BlendSavedBlurred(GlesCmdExecState*) {}
void gles_BlendSavedFlashed(GlesCmdExecState*) {}
void gles_DrawPoints(GlesCmdExecState*) {}
void gles_DrawTriangles(GlesCmdExecState*) {}
void gles_DrawProfile(GlesCmdExecState*) {}

void gles_ClearScreen(GlesCmdExecState *es) {
    auto *c = (const GlesCmdClearScreen *)es->cmd;
    GLbitfield b = 0;
    if (c->whichToClear & 1) b |= GL_COLOR_BUFFER_BIT;
    if (c->whichToClear & 2) b |= GL_DEPTH_BUFFER_BIT;
    if (b) { glClearColor(c->color[0],c->color[1],c->color[2],c->color[3]); glClear(b); }
}

void gles_SetViewport(GlesCmdExecState *es) {
    auto *c = (const GlesCmdSetViewport *)es->cmd;
    glViewport(c->x, c->y, c->width, c->height);
    s_state.w = c->width; s_state.h = c->height;
}

static GLuint texFromMat(const Material *m) {
    (void)m;
    // M3: resolve GfxImage -> GL texture handle via the image->glesTexture field
    return 0;
}

void gles_StretchPic(GlesCmdExecState *es) {
    auto *c = (const GlesCmdStretchPic *)es->cmd;
    GLuint t = texFromMat(c->material);
    drawQuad(c->x, c->y, c->w, c->h, c->s0, c->t0, c->s1, c->t1,
             (const float*)&c->color, t, t!=0, false);
}

void gles_StretchPicFlipST(GlesCmdExecState *es) {
    auto *c = (const GlesCmdStretchPic *)es->cmd;
    GLuint t = texFromMat(c->material);
    drawQuad(c->x, c->y, c->w, c->h, c->s0, c->t0, c->s1, c->t1,
             (const float*)&c->color, t, t!=0, true);
}

void gles_StretchPicRotateXY(GlesCmdExecState *es) {
    auto *c = (const GlesCmdStretchPicRotateXY *)es->cmd;
    GLuint t = texFromMat(c->material);
    drawQuad(c->x, c->y, c->w, c->h, c->s0, c->t0, c->s1, c->t1,
             (const float*)&c->color, t, t!=0, false);
}

void gles_StretchPicRotateST(GlesCmdExecState *es) {
    auto *c = (const GlesCmdStretchPicRotateST *)es->cmd;
    GLuint t = texFromMat(c->material);
    drawQuad(c->x, c->y, c->w, c->h, c->s0, c->t0, c->s1, c->t1,
             (const float*)&c->color, t, t!=0, false);
}

void gles_DrawQuadPic(GlesCmdExecState *es) {
    auto *c = (const GlesCmdDrawQuadPic *)es->cmd;
    GLuint t = texFromMat(c->material);
    float mx=c->verts[0][0], Mx=c->verts[0][0], my=c->verts[0][1], My=c->verts[0][1];
    for (int i=1; i<6; i++) {
        if (c->verts[i][0]<mx) mx=c->verts[i][0];
        if (c->verts[i][0]>Mx) Mx=c->verts[i][0];
        if (c->verts[i][1]<my) my=c->verts[i][1];
        if (c->verts[i][1]>My) My=c->verts[i][1];
    }
    drawQuad(mx, my, Mx-mx, My-my, 0,0,1,1, (const float*)&c->color, t, t!=0, false);
}

void gles_FullScreenColoredQuad(GlesCmdExecState *es) {
    auto *c = (const GlesCmdFullScreenColoredQuad *)es->cmd;
    GLuint t = texFromMat(c->material);
    drawQuad(0, 0, (float)s_state.w, (float)s_state.h,
             c->s0, c->t0, c->s1, c->t1, c->color, t, t!=0, false);
}

void gles_DrawText2D(GlesCmdExecState *es) {
    auto *c = (const GlesCmdDrawText2D *)es->cmd;
    if (!c->font || !c->font->material) return;
    // M3: Get font texture and render each character
    // For now, draw a placeholder rectangle so the menu is visible
    float charW = 16.0f * c->xScale;
    float charH = 24.0f * c->yScale;
    float x = c->x, y = c->y;
    const char *p = c->text;
    if (!p || !*p) return;

    ensureRes();
    if (!s_shaderFont) return;

    glUseProgram(s_shaderFont);
    glUniform4f(glGetUniformLocation(s_shaderFont, "uVP"), 0,0,(float)s_state.w,(float)s_state.h);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUniform1i(glGetUniformLocation(s_shaderFont, "uTex"), 0);

    uint8_t r=(uint8_t)(c->color.array[0]*255), g=(uint8_t)(c->color.array[1]*255),
            b=(uint8_t)(c->color.array[2]*255), a=(uint8_t)(c->color.array[3]*255);

    for (; *p && (p-c->text)<c->maxChars; p++) {
        if (*p=='\n') { x=c->x; y+=charH; continue; }
        if (*p==' ') { x+=charW*0.5f; continue; }

        float sx = (float)((unsigned char)*p % 16) / 16.0f;
        float sy = (float)((unsigned char)*p / 16) / 16.0f;
        float ex = sx+1.0f/16.0f, ey = sy+1.0f/16.0f;
        float x2=x+charW, y2=y+charH;

        GlesVert2D vs[6] = {
            {x,y, sx,sy, r,g,b,a}, {x2,y, ex,sy, r,g,b,a}, {x,y2, sx,ey, r,g,b,a},
            {x,y2, sx,ey, r,g,b,a}, {x2,y, ex,sy, r,g,b,a}, {x2,y2, ex,ey, r,g,b,a},
        };

        glBindVertexArray(s_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vs), vs, GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        x += charW;
    }
}

void gles_DrawLines(GlesCmdExecState *es) {
    // Simplified line rendering: engine debug lines, not used in menu
    (void)es;
}

void gles_ProjectionSet(GlesCmdExecState *es) {
    auto *c = (const GlesCmdProjectionSet *)es->cmd;
    s_state.projection = c->projection;
}

// ---- Public API --------------------------------------------------------------

void GLES_BeginFrame(int width, int height) {
    s_state.w = width ? width : 1;
    s_state.h = height ? height : 1;
    memset(s_state.curColor, 0, sizeof(s_state.curColor));
    s_state.projection = 0;
    glViewport(0, 0, s_state.w, s_state.h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    ensureRes();
}

void GLES_EndFrame() {}

void GLES_ExecuteCommands(GlesCmdArray *cmds) {
    if (!cmds || !cmds->cmds || cmds->usedTotal <= 0) return;
    uint8_t *ptr = cmds->cmds, *end = ptr + cmds->usedTotal;
    while (ptr < end) {
        auto *hdr = (const GlesCmdHeader *)ptr;
        if (hdr->id == 0 || hdr->id >= RC_COUNT) break;
        if (hdr->byteCount < sizeof(GlesCmdHeader)) break;
        GlesCmdExecState es; es.cmd = hdr;
        if (s_cmdTable[hdr->id]) s_cmdTable[hdr->id](&es);
        ptr += hdr->byteCount;
    }
}

void GLES_Shutdown() {
    if (s_shader2D) glDeleteProgram(s_shader2D);
    if (s_shaderFont) glDeleteProgram(s_shaderFont);
    if (s_vao) glDeleteVertexArrays(1, &s_vao);
    if (s_vbo) glDeleteBuffers(1, &s_vbo);
    memset(&s_state, 0, sizeof(s_state));
}