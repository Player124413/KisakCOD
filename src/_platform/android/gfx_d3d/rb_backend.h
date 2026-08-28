// Android port — stub rb_backend.h
// Provides types that non-renderer code references.
#pragma once

// Thread context type
enum ThreadContext_t : int {
    THREAD_CONTEXT_MAIN = 0,
    THREAD_CONTEXT_BACKEND = 1,
    THREAD_CONTEXT_COUNT = 2
};
