#include "window/window_visual_effects_internal.h"
#include <wchar.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

BOOL WindowVisualEffects_IsValidWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return FALSE;
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) return FALSE;
    wchar_t className[64] = {0};
    if (!GetClassNameW(hwnd, className, _countof(className))) return FALSE;
    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}
