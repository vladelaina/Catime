/**
 * @file tray_theme_state.c
 * @brief Win7-safe Windows appearance queries shared by tray UI.
 */

#include "tray_theme_state_internal.h"

#define PERSONALIZE_KEY \
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"
#define THEME_RECHECK_MS 300u
#define THEME_NO_ANIMATION_RECHECK_MS 50u

static BOOL IsRegistryThemeDark(const wchar_t* valueName) {
    HKEY key = NULL;
    DWORD value = 1;
    DWORD valueSize = sizeof(value);
    DWORD type = 0;
    LSTATUS status;
    if (!valueName) return FALSE;
    status = RegOpenKeyExW(HKEY_CURRENT_USER, PERSONALIZE_KEY, 0,
                           KEY_QUERY_VALUE, &key);
    if (status != ERROR_SUCCESS) return FALSE;
    status = RegQueryValueExW(key, valueName, NULL, &type,
                              (BYTE*)&value, &valueSize);
    RegCloseKey(key);
    return status == ERROR_SUCCESS && type == REG_DWORD && value == 0;
}

BOOL TrayThemeState_IsHighContrastActive(void) {
    HIGHCONTRASTW highContrast = {0};
    highContrast.cbSize = sizeof(highContrast);
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast),
                                 &highContrast, 0) &&
           (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

BOOL TrayThemeState_IsAppsDark(void) {
    return IsRegistryThemeDark(L"AppsUseLightTheme");
}

BOOL TrayThemeState_IsSystemDark(void) {
    return IsRegistryThemeDark(L"SystemUsesLightTheme");
}

BOOL IsSystemDarkModeActive(void) {
    return !TrayThemeState_IsHighContrastActive() &&
           TrayThemeState_IsSystemDark();
}

COLORREF GetSystemMetricTextColor(void) {
    if (TrayThemeState_IsHighContrastActive()) {
        return GetSysColor(COLOR_WINDOWTEXT);
    }
    return TrayThemeState_IsSystemDark()
        ? RGB(255, 255, 255) : RGB(0, 0, 0);
}

UINT GetSystemThemeRecheckDelay(void) {
    BOOL animationsEnabled = TRUE;
    if (SystemParametersInfoW(
            SPI_GETCLIENTAREAANIMATION, 0, &animationsEnabled, 0) &&
        !animationsEnabled) {
        return THEME_NO_ANIMATION_RECHECK_MS;
    }
    return THEME_RECHECK_MS;
}
