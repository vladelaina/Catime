/**
 * @file memory_pool.c
 * @brief Memory pool implementation
 */

#include "../../include/utils/memory_pool.h"
#include <stdlib.h>

/**
 * @brief Create memory pool
 */
MemoryPool* MemoryPool_Create(SIZE_T size) {
    if (size == 0) return NULL;
    
    MemoryPool* pool = (MemoryPool*)malloc(sizeof(MemoryPool));
    if (!pool) return NULL;
    
    pool->buffer = (BYTE*)malloc(size);
    if (!pool->buffer) {
        free(pool);
        return NULL;
    }
    
    pool->size = size;
    pool->inUse = FALSE;
    
    return pool;
}

/**
 * @brief Allocate from pool with fallback to heap
 */
void* MemoryPool_Alloc(MemoryPool* pool, SIZE_T size) {
    if (!pool) return malloc(size);
    
    /* Try pool first if available and size fits */
    if (!pool->inUse && size <= pool->size) {
        pool->inUse = TRUE;
        return pool->buffer;
    }
    
    /* Fallback to malloc */
    return malloc(size);
}

/**
 * @brief Free memory (returns to pool if it's pool memory)
 */
void MemoryPool_Free(MemoryPool* pool, void* ptr) {
    if (!ptr) return;
    
    if (pool && ptr == pool->buffer) {
        pool->inUse = FALSE;
        return;
    }
    
    free(ptr);
}

/**
 * @brief Destroy pool and free buffer
 */
void MemoryPool_Destroy(MemoryPool* pool) {
    if (!pool) return;
    
    if (pool->buffer) {
        free(pool->buffer);
    }
    free(pool);
}

