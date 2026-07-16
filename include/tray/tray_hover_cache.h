/**
 * @file tray_hover_cache.h
 * @brief Resilient cache for the Explorer notification-area icon rectangle
 */

#ifndef TRAY_HOVER_CACHE_H
#define TRAY_HOVER_CACHE_H

#include <windows.h>

typedef struct TrayHoverRectCache {
    RECT rect;
    DWORD lastQueryTime;
    DWORD lastSuccessTime;
    BOOL valid;
} TrayHoverRectCache;

void TrayHoverRectCache_Reset(TrayHoverRectCache* cache);
BOOL TrayHoverRectCache_NeedsRefresh(const TrayHoverRectCache* cache,
                                     DWORD now,
                                     DWORD refreshIntervalMs);
void TrayHoverRectCache_RecordQuery(TrayHoverRectCache* cache,
                                    BOOL succeeded,
                                    const RECT* rect,
                                    DWORD now,
                                    DWORD staleGraceMs);
BOOL TrayHoverRectCache_Contains(const TrayHoverRectCache* cache, POINT point);

#endif /* TRAY_HOVER_CACHE_H */
