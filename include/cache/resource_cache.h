/**
 * @file resource_cache.h
 * @brief Resource cache system for fonts and animations
 * 
 * This module provides persistent caching for resource files (fonts, animations)
 * with background scanning and automatic refresh capabilities.
 * 
 * Features:
 * - Background thread scanning on startup
 * - Persistent in-memory cache with expiry
 * - Thread-safe access with read-write locks
 * - Automatic refresh on file system changes
 * - Graceful degradation on cache miss
 */

#ifndef RESOURCE_CACHE_H
#define RESOURCE_CACHE_H

#include <windows.h>
#include <stdbool.h>
#include <time.h>

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

#define RESOURCE_CACHE_EXPIRY_SECONDS 60  /**< Cache validity period */
#define MAX_CACHE_ENTRIES 200             /**< Maximum entries per cache */
#define MAX_FONT_NAME_LENGTH 260          /**< Maximum font name length */
#define MAX_ANIM_NAME_LENGTH 260          /**< Maximum animation name length */

/* ============================================================================
 * Font Cache Structures
 * ============================================================================ */

/**
 * @brief Single font file cache entry
 */
typedef struct {
    wchar_t fileName[MAX_FONT_NAME_LENGTH];      /**< Font file name */
    wchar_t fullPath[MAX_PATH];                   /**< Full file path */
    wchar_t displayName[MAX_FONT_NAME_LENGTH];   /**< Display name (no ext) */
    wchar_t relativePath[MAX_PATH];               /**< Path relative to fonts folder */
    BOOL isCurrentFont;                           /**< Is this the active font */
    int depth;                                    /**< Folder depth (0 = root) */
} FontCacheEntry;

/**
 * @brief Font cache container
 */
typedef struct {
    FontCacheEntry* entries;     /**< Array of font entries */
    int count;                   /**< Number of entries */
    int capacity;                /**< Allocated capacity */
    time_t scanTime;             /**< Last scan timestamp */
    BOOL isValid;                /**< Cache validity flag */
    SRWLOCK lock;                /**< Read-write lock */
} FontCache;

/* ============================================================================
 * Animation Cache Structures
 * ============================================================================ */

/**
 * @brief Single animation file cache entry
 */
typedef struct {
    char fileName[MAX_ANIM_NAME_LENGTH];         /**< Animation file name (UTF-8) */
    char relativePath[MAX_PATH];                 /**< Relative path for config */
    wchar_t fullPath[MAX_PATH];                  /**< Full file path */
    BOOL isSpecial;                              /**< Is __logo__/__cpu__/__mem__ */
    BOOL isCurrent;                              /**< Is this the active animation */
} AnimationCacheEntry;

/**
 * @brief Animation cache container
 */
typedef struct {
    AnimationCacheEntry* entries;  /**< Array of animation entries */
    int count;                     /**< Number of entries */
    int capacity;                  /**< Allocated capacity */
    time_t scanTime;               /**< Last scan timestamp */
    BOOL isValid;                  /**< Cache validity flag */
    SRWLOCK lock;                  /**< Read-write lock */
} AnimationCache;

/* ============================================================================
 * Cache State Enum
 * ============================================================================ */

/**
 * @brief Cache operation result codes
 */
typedef enum {
    CACHE_OK = 0,                /**< Operation successful */
    CACHE_INVALID,               /**< Cache is invalid */
    CACHE_EXPIRED,               /**< Cache has expired */
    CACHE_EMPTY,                 /**< Cache is empty */
    CACHE_ERROR,                 /**< General error */
    CACHE_SCANNING               /**< Background scan in progress */
} CacheStatus;

/* ============================================================================
 * Public API - Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize the resource cache system
 * @param startBackgroundScan If TRUE, starts background scanning thread
 * @return TRUE on success, FALSE on failure
 * 
 * @note Call this once during application startup.
 *       If startBackgroundScan is TRUE, scanning happens asynchronously.
 */
BOOL ResourceCache_Initialize(BOOL startBackgroundScan);

/**
 * @brief Shutdown the resource cache system
 * 
 * Stops background threads, releases locks, and frees all memory.
 * Call this during application cleanup.
 */
void ResourceCache_Shutdown(void);

/**
 * @brief Check if cache system is ready
 * @return TRUE if initialized and ready to use
 */
BOOL ResourceCache_IsReady(void);

/* ============================================================================
 * Public API - Font Cache Access
 * ============================================================================ */

/**
 * @brief Get font cache status
 * @return Cache status code
 */
CacheStatus FontCache_GetStatus(void);

/**
 * @brief Get font entries from cache (thread-safe read)
 * @param outEntries Pointer to receive array (do not free, internal pointer)
 * @param outCount Pointer to receive entry count
 * @return Cache status code
 * 
 * @note The returned array is valid until next cache refresh.
 *       Caller must not modify or free the array.
 *       Use within a short time window or copy if needed.
 */
CacheStatus FontCache_GetEntries(const FontCacheEntry** outEntries, int* outCount);

/**
 * @brief Trigger asynchronous font cache refresh
 * 
 * Invalidates current cache and schedules background rescan.
 * Returns immediately without blocking.
 */
void FontCache_RequestRefresh(void);

/**
 * @brief Force synchronous font cache refresh
 * @return TRUE on success, FALSE on failure
 * 
 * @warning This blocks the calling thread during scan.
 *          Use only when absolutely necessary.
 */
BOOL FontCache_RefreshSync(void);

/**
 * @brief Update current font marker in cache
 * @param fontRelativePath Relative path of the new current font
 * 
 * Updates the isCurrentFont flag without rescanning filesystem.
 * Use this after user changes font.
 */
void FontCache_UpdateCurrentFont(const char* fontRelativePath);

/* ============================================================================
 * Public API - Animation Cache Access
 * ============================================================================ */

/**
 * @brief Get animation cache status
 * @return Cache status code
 */
CacheStatus AnimationCache_GetStatus(void);

/**
 * @brief Get animation entries from cache (thread-safe read)
 * @param outEntries Pointer to receive array (do not free, internal pointer)
 * @param outCount Pointer to receive entry count
 * @return Cache status code
 * 
 * @note Same usage notes as FontCache_GetEntries
 */
CacheStatus AnimationCache_GetEntries(const AnimationCacheEntry** outEntries, int* outCount);

/**
 * @brief Trigger asynchronous animation cache refresh
 */
void AnimationCache_RequestRefresh(void);

/**
 * @brief Force synchronous animation cache refresh
 * @return TRUE on success, FALSE on failure
 * 
 * @warning Blocks calling thread
 */
BOOL AnimationCache_RefreshSync(void);

/**
 * @brief Update current animation marker in cache
 * @param animationName Name of the new current animation
 */
void AnimationCache_UpdateCurrentAnimation(const char* animationName);

/* ============================================================================
 * Public API - Utilities
 * ============================================================================ */

/**
 * @brief Get cache statistics for debugging
 * @param outFontCount Font entries count
 * @param outAnimCount Animation entries count
 * @param outFontScanTime Last font scan timestamp
 * @param outAnimScanTime Last animation scan timestamp
 */
void ResourceCache_GetStatistics(int* outFontCount, int* outAnimCount,
                                  time_t* outFontScanTime, time_t* outAnimScanTime);

/**
 * @brief Invalidate all caches
 * 
 * Marks all caches as invalid without freeing memory.
 * Next access will trigger refresh.
 */
void ResourceCache_InvalidateAll(void);

/**
 * @brief Check if background scan is complete
 * @return TRUE if initial background scan has finished
 */
BOOL ResourceCache_IsBackgroundScanComplete(void);

#endif /* RESOURCE_CACHE_H */
