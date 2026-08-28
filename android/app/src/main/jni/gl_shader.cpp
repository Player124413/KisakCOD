#include "gl_shader.h"

#include <stdio.h>
#include <string.h>

static GLuint compileShader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    if (!s) return 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        GLsizei len = 0;
        glGetShaderInfoLog(s, sizeof(log), &len, log);
        fprintf(stderr, "[gles] shader compile error: %.*s\n", (int)len, log);
        return 0;
    }
    return s;
}

bool Shader::init(const char *vsSrc, const char *fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return false;
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    return ok != 0;
}

void Shader::destroy() {
    // program lives for the whole host lifetime; nothing to release
}