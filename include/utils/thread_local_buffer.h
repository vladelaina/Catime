/**
 * @file thread_local_buffer.h
 * @brief Heap-backed Win32 fiber-local storage for large per-thread buffers
 */

#ifndef THREAD_LOCAL_BUFFER_H
#define THREAD_LOCAL_BUFFER_H

#include <windows.h>

typedef struct {
    INIT_ONCE initOnce;
    DWORD flsIndex;
    SIZE_T bufferSize;
} ThreadLocalBuffer;

#define THREAD_LOCAL_BUFFER_STATIC_INIT(size) \
    { INIT_ONCE_STATIC_INIT, FLS_OUT_OF_INDEXES, (SIZE_T)(size) }

/** Return a zero-initialized buffer unique to the current thread/fiber. */
void* ThreadLocalBuffer_Get(ThreadLocalBuffer* buffer);

#endif /* THREAD_LOCAL_BUFFER_H */
