#pragma once
#include "gles_stub.h"

// Tiny GL shader helper (compile + link + uniform access).
struct Shader {
    GLuint program = 0;

    bool init(const char *vsSrc, const char *fsSrc);
    void use() const { if (program) glUseProgram(program); }
    GLint uniform(const char *name) const { return glGetUniformLocation(program, name); }
    void destroy();
};