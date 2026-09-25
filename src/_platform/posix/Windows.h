// KisakCOD -- capitalisation shim for <Windows.h>
//
// Several engine files include <Windows.h> with the Windows spelling
// (src/universal/timing.cpp, src/qcommon/threads.cpp, src/qcommon/mem_track.cpp,
// src/database/db_registry.cpp and others). That resolves fine on a
// case-insensitive filesystem but not on Linux or Android, so this file exists
// purely to catch that spelling and forward to the real compat layer.
//
// Everything it needs is declared in win32_posix.h next to this file.

#pragma once

#include "win32_posix.h"
