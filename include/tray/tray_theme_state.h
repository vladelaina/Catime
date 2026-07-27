#ifndef CATIME_TRAY_THEME_STATE_H
#define CATIME_TRAY_THEME_STATE_H

#include <windows.h>

/** Return the Windows system appearance used by the taskbar and tray. */
BOOL IsSystemDarkModeActive(void);

/** Return the accessible black/white text color shared by metric surfaces. */
COLORREF GetSystemMetricTextColor(void);

/** Return a short delay for confirming the final Windows theme state. */
UINT GetSystemThemeRecheckDelay(void);

#endif /* CATIME_TRAY_THEME_STATE_H */
