#ifndef CATIME_TRAY_THEME_STATE_INTERNAL_H
#define CATIME_TRAY_THEME_STATE_INTERNAL_H

#include <windows.h>
#include "tray/tray_theme_state.h"

BOOL TrayThemeState_IsHighContrastActive(void);
BOOL TrayThemeState_IsAppsDark(void);
BOOL TrayThemeState_IsSystemDark(void);

#endif /* CATIME_TRAY_THEME_STATE_INTERNAL_H */
