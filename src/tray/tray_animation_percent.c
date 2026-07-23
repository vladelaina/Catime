/**
 * @file tray_animation_percent.c
 * @brief Public color and cache lifecycle API for generated tray icons
 */

#include "tray_animation_percent_internal.h"

COLORREF g_percentTextColor = RGB(0, 0, 0);
COLORREF g_percentBgColor = TRANSPARENT_BG_AUTO;
PercentIconCacheEntry g_percentIconCache[
    GENERATED_PERCENT_ICON_CACHE_SIZE];
CapsIconCacheEntry g_capsIconCache[2];
COLORREF g_cachedThemeTextColor = CLR_INVALID;
DWORD g_lastThemeCheckTick = 0;
INIT_ONCE g_percentIconCacheLockOnce = INIT_ONCE_STATIC_INIT;
CRITICAL_SECTION g_percentIconCacheCS;

static BOOL CALLBACK InitPercentIconCacheLock(
    PINIT_ONCE initOnce, PVOID parameter, PVOID* context) {
    (void)initOnce;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&g_percentIconCacheCS);
    return TRUE;
}

BOOL BeginPercentIconCacheAccess(void) {
    if (!InitOnceExecuteOnce(
            &g_percentIconCacheLockOnce,
            InitPercentIconCacheLock, NULL, NULL)) {
        return FALSE;
    }
    EnterCriticalSection(&g_percentIconCacheCS);
    return TRUE;
}

void EndPercentIconCacheAccess(void) {
    LeaveCriticalSection(&g_percentIconCacheCS);
}

DWORD ColorRefToDibRgb(COLORREF color) {
    return (DWORD)GetBValue(color) |
           ((DWORD)GetGValue(color) << 8) |
           ((DWORD)GetRValue(color) << 16);
}

void GetGeneratedTrayIconSize(int* outCx, int* outCy) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    if (cy <= 0) cy = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    if (cx > GENERATED_TRAY_ICON_MAX_SIZE) cx = GENERATED_TRAY_ICON_MAX_SIZE;
    if (cy > GENERATED_TRAY_ICON_MAX_SIZE) cy = GENERATED_TRAY_ICON_MAX_SIZE;
    if (outCx) *outCx = cx;
    if (outCy) *outCy = cy;
}

void GetGeneratedTrayIconSizeSnapshot(int* outCx, int* outCy) {
    GetGeneratedTrayIconSize(outCx, outCy);
}

void ClearGeneratedIconCacheLocked(void) {
    for (int i = 0; i < (int)_countof(g_percentIconCache); ++i) {
        if (g_percentIconCache[i].icon) {
            DestroyIcon(g_percentIconCache[i].icon);
        }
        ZeroMemory(&g_percentIconCache[i],
                   sizeof(g_percentIconCache[i]));
    }
    for (int i = 0; i < (int)_countof(g_capsIconCache); ++i) {
        if (g_capsIconCache[i].icon) {
            DestroyIcon(g_capsIconCache[i].icon);
        }
        ZeroMemory(&g_capsIconCache[i], sizeof(g_capsIconCache[i]));
    }
}

void CleanupPercentIconCache(void) {
    if (!BeginPercentIconCacheAccess()) return;
    ClearGeneratedIconCacheLocked();
    g_cachedThemeTextColor = CLR_INVALID;
    g_lastThemeCheckTick = 0;
    EndPercentIconCacheAccess();
}

static BOOL IsSystemDarkTheme(void) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    HKEY key = NULL;
    if (RegOpenKeyExA(
            HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &key) == ERROR_SUCCESS) {
        if (RegQueryValueExA(
                key, "SystemUsesLightTheme", NULL, NULL,
                (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            RegCloseKey(key);
            return value == 0;
        }
        RegCloseKey(key);
    }
    return FALSE;
}

static COLORREF QueryThemeTextColor(void) {
    return IsSystemDarkTheme() ? RGB(255, 255, 255) : RGB(0, 0, 0);
}

static BOOL TryGetCachedThemeTextColorLocked(COLORREF* textColor) {
    if (!textColor) return FALSE;
    DWORD now = GetTickCount();
    if (g_cachedThemeTextColor != CLR_INVALID &&
        now - g_lastThemeCheckTick < 5000) {
        *textColor = g_cachedThemeTextColor;
        return TRUE;
    }
    return FALSE;
}

static void StoreThemeTextColorLocked(COLORREF textColor, DWORD tick) {
    g_cachedThemeTextColor = textColor;
    g_lastThemeCheckTick = tick;
}

BOOL SnapshotIconColorsLocked(COLORREF* textColor, COLORREF* bgColor) {
    if (!textColor || !bgColor) return FALSE;
    *bgColor = g_percentBgColor;
    if (*bgColor == TRANSPARENT_BG_AUTO) {
        return TryGetCachedThemeTextColorLocked(textColor);
    }
    *textColor = g_percentTextColor;
    return TRUE;
}

BOOL GetIconColorSnapshot(COLORREF* textColor, COLORREF* bgColor) {
    if (!textColor || !bgColor || !BeginPercentIconCacheAccess()) {
        return FALSE;
    }
    if (SnapshotIconColorsLocked(textColor, bgColor)) {
        EndPercentIconCacheAccess();
        return TRUE;
    }
    EndPercentIconCacheAccess();

    COLORREF themeTextColor = QueryThemeTextColor();
    DWORD themeTick = GetTickCount();
    if (!BeginPercentIconCacheAccess()) return FALSE;
    if (g_percentBgColor == TRANSPARENT_BG_AUTO) {
        StoreThemeTextColorLocked(themeTextColor, themeTick);
        *textColor = themeTextColor;
        *bgColor = TRANSPARENT_BG_AUTO;
    } else {
        *textColor = g_percentTextColor;
        *bgColor = g_percentBgColor;
    }
    EndPercentIconCacheAccess();
    return TRUE;
}

BOOL GetPercentIconColorSnapshot(
    COLORREF* textColor, COLORREF* bgColor) {
    return GetIconColorSnapshot(textColor, bgColor);
}

void SetPercentIconColors(COLORREF textColor, COLORREF bgColor) {
    if (!BeginPercentIconCacheAccess()) return;
    if (g_percentTextColor == textColor && g_percentBgColor == bgColor) {
        EndPercentIconCacheAccess();
        return;
    }
    ClearGeneratedIconCacheLocked();
    g_percentTextColor = textColor;
    g_percentBgColor = bgColor;
    g_cachedThemeTextColor = CLR_INVALID;
    g_lastThemeCheckTick = 0;
    EndPercentIconCacheAccess();
}

COLORREF GetPercentIconTextColor(void) {
    COLORREF textColor = RGB(0, 0, 0);
    if (!BeginPercentIconCacheAccess()) return textColor;
    textColor = g_percentTextColor;
    EndPercentIconCacheAccess();
    return textColor;
}

COLORREF GetPercentIconBgColor(void) {
    COLORREF bgColor = TRANSPARENT_BG_AUTO;
    if (!BeginPercentIconCacheAccess()) return bgColor;
    bgColor = g_percentBgColor;
    EndPercentIconCacheAccess();
    return bgColor;
}
