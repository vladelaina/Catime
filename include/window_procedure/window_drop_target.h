#ifndef WINDOW_DROP_TARGET_H
#define WINDOW_DROP_TARGET_H

#include <windows.h>

typedef enum {
    RESOURCE_TYPE_UNKNOWN = 0,
    RESOURCE_TYPE_FONT,
    RESOURCE_TYPE_ANIMATION
} ResourceType;

void HandleDropFiles(HWND hwnd, HDROP hDrop);

#endif // WINDOW_DROP_TARGET_H
