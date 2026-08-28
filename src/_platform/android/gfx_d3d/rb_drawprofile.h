// Android port — stub rb_drawprofile.h
#pragma once
// This header is included by qcommon/threads.cpp on Windows for debug
// profile drawing. On Android, the backend stubs provide the types.
// The original includes r_gfx.h which needs <d3d9.h>; here we just
// include the platform's own gfx types.
#include "r_gfx.h"