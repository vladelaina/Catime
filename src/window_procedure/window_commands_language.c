#include "window_commands_internal.h"

#include "taskbar_monitor.h"

static const struct {
    UINT menuId;
    AppLanguage language;
} LANGUAGE_MAP[] = {
#define X(Enum, Code, Native, Eng, ConfigKey, ResId, MenuId, ...) \
    {MenuId, Enum},
#include "language_def.h"
    LANGUAGE_LIST
#undef X
};

BOOL HandleLanguageSelection(HWND hwnd, UINT menuId) {
    for (size_t i = 0; i < _countof(LANGUAGE_MAP); i++) {
        if (menuId == LANGUAGE_MAP[i].menuId) {
            if (WriteConfigLanguage(LANGUAGE_MAP[i].language) &&
                SetLanguage(LANGUAGE_MAP[i].language)) {
                InvalidateRect(hwnd, NULL, TRUE);
                TaskbarMonitor_Refresh();
            }
            return TRUE;
        }
    }
    return FALSE;
}
