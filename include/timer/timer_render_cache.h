/**
 * @file timer_render_cache.h
 * @brief Helpers for tracking text that was successfully painted.
 */

#ifndef TIMER_RENDER_CACHE_H
#define TIMER_RENDER_CACHE_H

#include <windows.h>
#include <stddef.h>

/**
 * Return TRUE when currentText differs from the last successfully painted text.
 * This check intentionally does not mutate the cache so skipped or failed paints
 * remain eligible for retry on the next timer tick.
 */
BOOL TimerRenderCache_NeedsRepaint(const wchar_t* lastPaintedText,
                                   BOOL hasLastPaintedText,
                                   const wchar_t* currentText);

/** Record text only after the frame containing it was successfully presented. */
void TimerRenderCache_CommitPaint(wchar_t* lastPaintedText,
                                  size_t lastPaintedTextSize,
                                  BOOL* hasLastPaintedText,
                                  const wchar_t* paintedText);

#endif /* TIMER_RENDER_CACHE_H */
