/**
 * @file tray_hover_cache.c
 * @brief Resilient cache for the Explorer notification-area icon rectangle
 */

#include "tray/tray_hover_cache.h"

void TrayHoverRectCache_Reset(TrayHoverRectCache* cache) {
    if (!cache) return;
    SetRectEmpty(&cache->rect);
    cache->lastQueryTime = 0;
    cache->lastSuccessTime = 0;
    cache->valid = FALSE;
}

BOOL TrayHoverRectCache_NeedsRefresh(const TrayHoverRectCache* cache,
                                     DWORD now,
                                     DWORD refreshIntervalMs) {
    if (!cache) return FALSE;
    return cache->lastQueryTime == 0 ||
           (DWORD)(now - cache->lastQueryTime) > refreshIntervalMs;
}

void TrayHoverRectCache_RecordQuery(TrayHoverRectCache* cache,
                                    BOOL succeeded,
                                    const RECT* rect,
                                    DWORD now,
                                    DWORD staleGraceMs) {
    if (!cache) return;

    cache->lastQueryTime = now;
    if (succeeded && rect && !IsRectEmpty(rect)) {
        cache->rect = *rect;
        cache->lastSuccessTime = now;
        cache->valid = TRUE;
        return;
    }

    if (!cache->valid ||
        (DWORD)(now - cache->lastSuccessTime) > staleGraceMs) {
        SetRectEmpty(&cache->rect);
        cache->valid = FALSE;
    }
}

BOOL TrayHoverRectCache_Contains(const TrayHoverRectCache* cache, POINT point) {
    return cache && cache->valid && PtInRect(&cache->rect, point);
}
