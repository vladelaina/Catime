/**
 * @file timer_render_cache.c
 * @brief Successfully-painted timer text cache implementation.
 */

#include "timer/timer_render_cache.h"

#include <wchar.h>

BOOL TimerRenderCache_NeedsRepaint(const wchar_t* lastPaintedText,
                                   BOOL hasLastPaintedText,
                                   const wchar_t* currentText) {
    if (!currentText) return FALSE;
    if (!hasLastPaintedText || !lastPaintedText) return TRUE;
    return wcscmp(lastPaintedText, currentText) != 0;
}

void TimerRenderCache_CommitPaint(wchar_t* lastPaintedText,
                                  size_t lastPaintedTextSize,
                                  BOOL* hasLastPaintedText,
                                  const wchar_t* paintedText) {
    if (!lastPaintedText || lastPaintedTextSize == 0 ||
        !hasLastPaintedText || !paintedText) {
        return;
    }

    wcsncpy(lastPaintedText, paintedText, lastPaintedTextSize - 1);
    lastPaintedText[lastPaintedTextSize - 1] = L'\0';
    *hasLastPaintedText = TRUE;
}
