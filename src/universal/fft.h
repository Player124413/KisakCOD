#pragma once

// Define complex_s locally to avoid pulling in gfx_d3d/r_material.h (D3D9)
struct complex_s {
    float real;
    float imag;
};

void __cdecl FFT_Init(int *fftBitswap, complex_s *fftTrigTable);
void __cdecl FFT(complex_s *data_inout, uint32_t log2_count, int *bitSwap, complex_s *trigTable);