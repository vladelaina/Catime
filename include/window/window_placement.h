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

#endif /* WINDOW_PLACEMENT_H */
