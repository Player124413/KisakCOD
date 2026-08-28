// Minimal EGL/GLES2 declarations.
//
// On Android the real platform headers are used; everywhere else (desktop
// syntax checks / CI) this header provides just the symbols the host uses so
// the port sources compile without the NDK.
#pragma once

#if defined(__ANDROID__)
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>
#else

#ifdef __cplusplus
extern "C" {
#endif

typedef int EGLBoolean;
typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLDisplay;
typedef void *EGLSurface;
typedef int EGLint;
typedef long EGLNativeWindowType; // ignored on desktop

#define EGL_DEFAULT_DISPLAY ((void *)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_TRUE 1
#define EGL_FALSE 0

#define EGL_COLOR_BUFFER_TYPE 0x303F
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES2_BIT 0x4
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_ALPHA_SIZE 0x3021
#define EGL_DEPTH_SIZE 0x3025
#define EGL_STENCIL_SIZE 0x3026
#define EGL_NONE 0x3038
#define EGL_SUCCESS 0x3000
#define EGL_BAD_CONFIG 0x300A
#define EGL_BAD_DISPLAY 0x3008
#define EGL_BAD_SURFACE 0x300D
#define EGL_BAD_CONTEXT 0x300E
#define EGL_BAD_ALLOC 0x3003

#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_TRIANGLES 0x0004
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_FLOAT 0x1406
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_NO_ERROR 0
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef int GLsizei;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLfixed;
typedef float GLfloat;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef char GLchar;
typedef long GLsizeiptr;

EGLDisplay eglGetDisplay(void *display_id);
EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
                           EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config,
                            EGLContext share_context, const EGLint *attrib_list);
EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                  EGLNativeWindowType win, const EGLint *attrib_list);
EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read,
                          EGLContext ctx);
EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);
EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface);
EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx);
EGLBoolean eglTerminate(EGLDisplay dpy);
EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
                              EGLint attribute, EGLint *value);
EGLint eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);
EGLint eglGetError(void);

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void glClear(GLbitfield mask);
void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void glGenBuffers(GLsizei n, GLuint *buffers);
void glBindBuffer(GLenum target, GLuint buffer);
void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
void glEnableVertexAttribArray(GLuint index);
void glDisableVertexAttribArray(GLuint index);
void glVertexAttribPointer(GLuint index, GLint size, GLenum type,
                           GLboolean normalized, GLsizei stride, const void *pointer);
GLint glGetAttribLocation(GLuint program, const GLchar *name);
void glDrawArrays(GLenum mode, GLint first, GLsizei count);
GLuint glCreateShader(GLenum type);
void glShaderSource(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
void glCompileShader(GLuint shader);
void glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
GLuint glCreateProgram(void);
void glAttachShader(GLuint program, GLuint shader);
void glLinkProgram(GLuint program);
void glGetProgramiv(GLuint program, GLenum pname, GLint *params);
void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
void glUseProgram(GLuint program);
GLint glGetUniformLocation(GLuint program, const GLchar *name);
void glUniform1f(GLint location, GLfloat v0);
void glUniform2f(GLint location, GLfloat v0, GLfloat v1);
void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                        const GLfloat *value);
void glGenTextures(GLsizei n, GLuint *textures);
void glBindTexture(GLenum target, GLuint texture);
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                  GLsizei height, GLint border, GLenum format, GLenum type,
                  const void *pixels);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glActiveTexture(GLenum texture);
void glUniform1i(GLint location, GLint v0);
GLenum glGetError(void);
void glDisable(GLenum cap);
void glEnable(GLenum cap);
void glBlendFunc(GLenum sfactor, GLenum dfactor);

#ifdef __cplusplus
}
#endif
#endif // __ANDROID__