// Android port — stub r_material.h for dedicated server builds.
#pragma once

#include "r_gfx.h"

struct Material;
struct MaterialPass;
struct MaterialTechnique;
struct GfxShaderProgram;

// MaterialTechniqueType enum (stub)
enum MaterialTechniqueType : int {
    TECHNIQUE_UNDEFINED = 0,
    TECHNIQUE_COUNT = 1
};

// complex_s (used by fft.h)
struct complex_s {
    float real;
    float imag;
};

// TechType enum
enum TechType : int {
    TECH_UNLIT = 0,
    TECH_LIT = 1,
    TECH_COUNT = 2
};