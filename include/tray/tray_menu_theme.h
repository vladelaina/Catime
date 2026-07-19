#ifndef TRAY_MENU_THEME_H
#define TRAY_MENU_THEME_H

#include <windows.h>

/** Enable native dark popup-menu support when the OS provides it. */
BOOL InitializeNativeMenuTheme(void);

/** Allow popup menus owned by this window to use the native dark theme. */
void ApplyNativeMenuThemeToWindow(HWND hwnd);

/** Refresh the process menu theme after a system theme change. */
void RefreshNativeMenuTheme(void);

/** Return TRUE when Catime's native menus should currently use dark colors. */
BOOL IsNativeMenuDarkModeActive(void);

/** Return the effective Catime appearance shared by menus and dialogs. */
BOOL IsApplicationDarkModeActive(void);

#endif /* TRAY_MENU_THEME_H */
