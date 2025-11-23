/**
 * @file animation_cache.h
 * @brief Animation resource cache system
 */

#ifndef ANIMATION_CACHE_H
#define ANIMATION_CACHE_H

#include <windows.h>
#include <stdbool.h>
#include <time.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_ANIM_NAME_LENGTH 260
#define MAX_ANIM_CACHE_ENTRIES 200

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Single animation file cache entry
 */
typedef struct {
    char fileName[MAX_ANIM_NAME_LENGTH];
    char relativePath[MAX_PATH];
    wchar_t fullPath[MAX_PATH];
    BOOL isSpecial;
    BOOL isCurrent;
    int depth;
} AnimationCacheEntry;

/**
 * @brief Animation cache status
 */
typedef enum {
    ANIM_CACHE_OK = 0,
    ANIM_CACHE_INVALID,
    ANIM_CACHE_EXPIRED,
    ANIM_CACHE_EMPTY,
    ANIM_CACHE_ERROR
} AnimationCacheStatus;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize animation cache system
 */
BOOL AnimationCache_Initialize(void);

/**
 * @brief Shutdown animation cache system
 */
void AnimationCache_Shutdown(void);

/**
 * @brief Scan animations and populate cache (thread-safe)
 */
BOOL AnimationCache_Scan(void);

/**
 * @brief Get animation entries from cache
 * @param outEntries Pointer to receive array (Caller must free this memory using free())
 * @param outCount Pointer to receive entry count
 * @return Cache status
 */
AnimationCacheStatus AnimationCache_GetEntries(AnimationCacheEntry** outEntries, int* outCount);

/**
 * @brief Invalidate animation cache
 */
void AnimationCache_Invalidate(void);

/**
 * @brief Update current animation marker
 */
void AnimationCache_UpdateCurrent(const char* animationName);

/**
 * @brief Get animation cache statistics
 */
void AnimationCache_GetStatistics(int* outCount, time_t* outScanTime);

/**
 * @brief Check if cache is valid
 */
BOOL AnimationCache_IsValid(void);

#endif /* ANIMATION_CACHE_H */
