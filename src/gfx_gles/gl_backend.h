// GLES backend — public API
#pragma once

#include "gles_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void GLES_BeginFrame(int width, int height);
void GLES_EndFrame(void);
void GLES_ExecuteCommands(GlesCmdArray *cmds);
void GLES_Shutdown(void);

#ifdef __cplusplus
}
#endif