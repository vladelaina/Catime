#ifndef CATIME_TRAY_ANIMATION_SELECTION_H
#define CATIME_TRAY_ANIMATION_SELECTION_H

#include <windows.h>

BOOL TrayAnimationSelection_CanReuseCurrent(
    const char* currentName, const char* requestedName,
    BOOL previewActive, BOOL previewPending);

#endif /* CATIME_TRAY_ANIMATION_SELECTION_H */
