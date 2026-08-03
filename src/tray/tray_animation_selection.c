#include "tray_animation_selection.h"

#include <string.h>

BOOL TrayAnimationSelection_CanReuseCurrent(
    const char* currentName, const char* requestedName,
    BOOL previewActive, BOOL previewPending) {
    return currentName && requestedName && currentName[0] && requestedName[0] &&
           !previewActive && !previewPending &&
           _stricmp(currentName, requestedName) == 0;
}
