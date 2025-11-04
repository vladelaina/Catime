/**
 * @file memory_pool.h
 * @brief Simple memory pool for reducing malloc overhead
 * 
 * Single fixed-size buffer that can be reused for temporary allocations.
 * Reduces heap fragmentation and improves performance in frame decoding loops.
 */

#ifndef UTILS_MEMORY_POOL_H
#define UTILS_MEMORY_POOL_H

#include <windows.h>

/**
 * @brief Memory pool structure
 */
typedef struct {
    BYTE* buffer;       /**< Pre-allocated buffer */
    SIZE_T size;        /**< Buffer size in bytes */
    BOOL inUse;         /**< Single-use flag */
} MemoryPool;

/**
 * @brief Create memory pool with specified size
 * @param size Buffer size in bytes
 * @return Pool instance or NULL on failure
 */
MemoryPool* MemoryPool_Create(SIZE_T size);

/**
 * @brief Allocate from pool or fallback to malloc
 * @param pool Pool instance
 * @param size Requested size
 * @return Memory pointer or NULL on failure
 * 
 * @details
 * If pool is available and size fits, returns pool buffer.
 * Otherwise, falls back to malloc().
 * Only one allocation active at a time.
 */
void* MemoryPool_Alloc(MemoryPool* pool, SIZE_T size);

/**
 * @brief Free memory back to pool or free it
 * @param pool Pool instance
 * @param ptr Pointer to free
 */
void MemoryPool_Free(MemoryPool* pool, void* ptr);

/**
 * @brief Destroy pool and free buffer
 * @param pool Pool instance to destroy
 */
void MemoryPool_Destroy(MemoryPool* pool);

#endif /* UTILS_MEMORY_POOL_H */

