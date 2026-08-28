// Android port stub — zlib.h
#pragma once
// This is a stub. The real zlib is available in the NDK.
// For host syntax checks, just provide the types the engine needs.
typedef void *z_streamp;
#define Z_OK 0
#define Z_STREAM_END 1
#define Z_STREAM_ERROR -2
#define Z_DATA_ERROR -3
#define Z_MEM_ERROR -4
#define Z_BUF_ERROR -5
#define Z_VERSION_ERROR -6
int inflateInit_(z_streamp, const char *, int);
int inflate(z_streamp, int);
int inflateEnd(z_streamp);
int deflateInit_(z_streamp, int, const char *, int);
int deflate(z_streamp, int);
int deflateEnd(z_streamp);
const char *zError(int);
uLong adler32(uLong, const Bytef *, uInt);

