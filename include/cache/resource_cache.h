/**
 * @file resource_cache.h
 * @brief Resource cache coordinator
 * 
 * This module coordinates font and animation cache systems,
 * providing unified initialization, background scanning, and statistics.
 */

#ifndef RESOURCE_CACHE_H
#define RESOURCE_CACHE_H

#include <windows.h>
#include <stdbool.h>
#include <time.h>

// Include sub-modules
#include "cache/font_cache.h"
#include "cache/animation_cache.h"

/* ============================================================================
 * Public API - System Management
 * ============================================================================ */

/**
 * @brief Initialize the resource cache system
 * @param startBackgroundScan If TRUE, starts background scanning thread
 * @return TRUE on success, FALSE on failure
 */
BOOL ResourceCache_Initialize(BOOL startBackgroundScan);

/**
 * @brief Shutdown the resource cache system
 */
void ResourceCache_Shutdown(void);

/**
 * @brief Check if cache system is ready
 */
BOOL ResourceCache_IsReady(void);

/**
 * @brief Check if background scan is complete
 */
BOOL ResourceCache_IsBackgroundScanComplete(void);

/**
 * @brief Request async refresh of all caches
 */
void ResourceCache_RequestRefresh(void);

/**
 * @brief Invalidate all caches
 */
void ResourceCache_InvalidateAll(void);

/**
 * @brief Get cache statistics
 */
void ResourceCache_GetStatistics(int* outFontCount, int* outAnimCount,
                                  time_t* outFontScanTime, time_t* outAnimScanTime);

#endif /* RESOURCE_CACHE_H */
