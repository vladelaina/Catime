/**
 * @file taskbar_monitor_discovery.c
 * @brief Recursive taskbar child-window discovery.
 */

#include "taskbar_monitor_internal.h"

#include <wchar.h>

HWND TaskbarMonitor_FindDescendantByClass(
    HWND parent, const wchar_t* className) {
    HWND child = NULL;
    if (!parent || !className) return NULL;
    while ((child = FindWindowExW(parent, child, NULL, NULL)) != NULL) {
        wchar_t actualClass[64] = {0};
        if (GetClassNameW(child, actualClass, _countof(actualClass)) > 0 &&
            wcscmp(actualClass, className) == 0) {
            return child;
        }
        HWND nested = TaskbarMonitor_FindDescendantByClass(
            child, className);
        if (nested) return nested;
    }
    return NULL;
}
