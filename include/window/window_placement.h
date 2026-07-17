#ifndef WINDOW_PLACEMENT_H
#define WINDOW_PLACEMENT_H

#include <windows.h>

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
