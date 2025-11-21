/**
 * @file font_cache.h
 * @brief Font resource cache system
 */

#ifndef FONT_CACHE_H
#define FONT_CACHE_H

#include <windows.h>
#include <stdbool.h>
#include <time.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_FONT_NAME_LENGTH 260
#define MAX_FONT_CACHE_ENTRIES 200

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Single font file cache entry
 */
typedef struct {
    wchar_t fileName[MAX_FONT_NAME_LENGTH];
    wchar_t fullPath[MAX_PATH];
    wchar_t displayName[MAX_FONT_NAME_LENGTH];
    wchar_t relativePath[MAX_PATH];
    BOOL isCurrentFont;
    int depth;
} FontCacheEntry;

/**
 * @brief Font cache status
 */
typedef enum {
    FONT_CACHE_OK = 0,
    FONT_CACHE_INVALID,
    FONT_CACHE_EXPIRED,
    FONT_CACHE_EMPTY,
    FONT_CACHE_ERROR
} FontCacheStatus;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize font cache system
 */
BOOL FontCache_Initialize(void);

/**
 * @brief Shutdown font cache system
 */
void FontCache_Shutdown(void);

/**
 * @brief Scan fonts and populate cache (thread-safe)
 */
BOOL FontCache_Scan(void);

/**
 * @brief Get font entries from cache
 * @param outEntries Pointer to receive array
 * @param outCount Pointer to receive entry count
 * @return Cache status
 */
FontCacheStatus FontCache_GetEntries(const FontCacheEntry** outEntries, int* outCount);

/**
 * @brief Invalidate font cache
 */
void FontCache_Invalidate(void);

/**
 * @brief Update current font marker
 */
void FontCache_UpdateCurrent(const char* fontRelativePath);

/**
 * @brief Get font cache statistics
 */
void FontCache_GetStatistics(int* outCount, time_t* outScanTime);

/**
 * @brief Check if cache is valid
 */
BOOL FontCache_IsValid(void);

#endif /* FONT_CACHE_H */
