#ifndef WINDOW_PLACEMENT_H
#define WINDOW_PLACEMENT_H

#include <windows.h>

/** Return the minimum usable portion that must remain visible. */
int WindowPlacement_GetMinimumVisibleLength(int windowLength,
                                            int minimumMargin);

/** Clamp a window origin so the window remains within the supplied bounds. */
BOOL WindowPlacement_ClampFullyVisible(const RECT* bounds,
                                       int windowWidth,
                                       int windowHeight,
                                       int* x,
                                       int* y);

/**
 * Return TRUE when a saved taskbar anchor should be preserved for this
 * window geometry. Incidental taskbar overlap is not treated as an anchor.
 */
BOOL WindowPlacement_ShouldPreserveTaskbarAnchor(
    BOOL configuredTaskbarAnchored,
    const RECT* windowRect,
    const RECT* taskbarRect);

/** Capture a taskbar-relative placement that remains meaningful after resize. */
BOOL WindowPlacement_CaptureTaskbarAnchor(const RECT* windowRect,
                                          const RECT* taskbarRect,
                                          const RECT* monitorRect,
                                          int* axisRatio,
                                          int* crossCenterOffset);

/** Resolve a captured placement against the taskbar's current edge and size. */
BOOL WindowPlacement_ResolveTaskbarAnchor(const RECT* taskbarRect,
                                          const RECT* monitorRect,
                                          int windowWidth,
                                          int windowHeight,
                                          int axisRatio,
                                          int crossCenterOffset,
                                          int* outX,
                                          int* outY);

/**
 * Keep a user-selected top-left position authoritative across a layout pass.
 * Returns TRUE only when the layout moved the top-left and it should be restored.
 */
BOOL WindowPlacement_GetManualTopLeftRestore(const RECT* manualRect,
                                             const RECT* layoutRect,
                                             POINT* outPosition);

#endif /* WINDOW_PLACEMENT_H */
