/**
 * @file thread_local_buffer.c
 * @brief Win32 FLS-backed storage used to avoid GNU emulated TLS on MinGW
 */

#include "utils/thread_local_buffer.h"

#ifdef CATIME_USE_WIN32_FLS

static VOID NTAPI DestroyThreadLocalBuffer(PVOID value) {
    if (value) {
        HeapFree(GetProcessHeap(), 0, value);
    }
}

static BOOL CALLBACK InitializeThreadLocalBuffer(PINIT_ONCE initOnce,
                                                 PVOID parameter,
                                                 PVOID* context) {
    (void)initOnce;
    (void)context;

    ThreadLocalBuffer* buffer = (ThreadLocalBuffer*)parameter;
    if (!buffer || buffer->bufferSize == 0) {
        return FALSE;
    }

    DWORD index = FlsAlloc(DestroyThreadLocalBuffer);
    if (index == FLS_OUT_OF_INDEXES) {
        return FALSE;
    }

    buffer->flsIndex = index;
    return TRUE;
}

void* ThreadLocalBuffer_Get(ThreadLocalBuffer* buffer) {
    if (!buffer || buffer->bufferSize == 0 ||
        !InitOnceExecuteOnce(&buffer->initOnce, InitializeThreadLocalBuffer,
                             buffer, NULL)) {
        return NULL;
    }

    void* value = FlsGetValue(buffer->flsIndex);
    if (value) {
        return value;
    }

    value = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buffer->bufferSize);
    if (!value) {
        return NULL;
    }

    if (!FlsSetValue(buffer->flsIndex, value)) {
        HeapFree(GetProcessHeap(), 0, value);
        return NULL;
    }

    return value;
}

#endif /* CATIME_USE_WIN32_FLS */
