// Android port stub — io.h
#pragma once
// MSVC io.h functions map to POSIX equivalents
// These are already provided via win32_compat.h (_findfirst, etc.)
// and unistd.h (read, write, close, lseek, isatty).
#include <unistd.h>

