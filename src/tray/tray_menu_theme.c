#include "tray/tray_menu_theme.h"

#include "log.h"

#include <roapi.h>
#include <string.h>
#include <windows.ui.viewmanagement.h>
#include <winstring.h>

#define WINDOWS_10_1809_BUILD 17763
#define WINDOWS_10_1903_BUILD 18362
#define UXTHEME_REFRESH_IMMERSIVE_COLOR_POLICY_ORDINAL 104
#define UXTHEME_SHOULD_APPS_USE_DARK_MODE_ORDINAL 132
#define UXTHEME_ALLOW_DARK_MODE_FOR_WINDOW_ORDINAL 133
#define UXTHEME_SET_PREFERRED_APP_MODE_ORDINAL 135
#define UXTHEME_FLUSH_MENU_THEMES_ORDINAL 136

typedef enum {
    PREFERRED_APP_MODE_DEFAULT = 0,
    PREFERRED_APP_MODE_ALLOW_DARK = 1,
    PREFERRED_APP_MODE_FORCE_DARK = 2,
    PREFERRED_APP_MODE_FORCE_LIGHT = 3
} PreferredAppMode;

typedef LONG (WINAPI* RtlGetVersionFn)(OSVERSIONINFOW* versionInfo);
typedef void (WINAPI* RefreshImmersiveColorPolicyStateFn)(void);
typedef BOOL (WINAPI* ShouldAppsUseDarkModeFn)(void);
typedef BOOL (WINAPI* AllowDarkModeForWindowFn)(HWND hwnd, BOOL allow);
typedef PreferredAppMode (WINAPI* SetPreferredAppModeFn)(PreferredAppMode mode);
typedef void (WINAPI* FlushMenuThemesFn)(void);
typedef HRESULT (WINAPI* RoInitializeFn)(RO_INIT_TYPE initType);
typedef void (WINAPI* RoUninitializeFn)(void);
typedef HRESULT (WINAPI* RoActivateInstanceFn)(HSTRING classId,
                                               IInspectable** instance);
typedef HRESULT (WINAPI* WindowsCreateStringFn)(PCNZWCH sourceString,
                                                UINT32 length,
                                                HSTRING* string);
typedef HRESULT (WINAPI* WindowsDeleteStringFn)(HSTRING string);

static HMODULE g_uxtheme = NULL;
static RefreshImmersiveColorPolicyStateFn
    g_refreshImmersiveColorPolicyState = NULL;
static ShouldAppsUseDarkModeFn g_shouldAppsUseDarkMode = NULL;
static AllowDarkModeForWindowFn g_allowDarkModeForWindow = NULL;
static SetPreferredAppModeFn g_setPreferredAppMode = NULL;
static FlushMenuThemesFn g_flushMenuThemes = NULL;
static BOOL g_initialized = FALSE;
static BOOL g_supported = FALSE;
static BOOL g_darkModeActive = FALSE;
static PreferredAppMode g_appliedMode = PREFERRED_APP_MODE_DEFAULT;
static DWORD g_windowsBuild = 0;
static HMODULE g_combase = NULL;
static BOOL g_combaseLoadAttempted = FALSE;
static RoInitializeFn g_roInitialize = NULL;
static RoUninitializeFn g_roUninitialize = NULL;
static RoActivateInstanceFn g_roActivateInstance = NULL;
static WindowsCreateStringFn g_windowsCreateString = NULL;
static WindowsDeleteStringFn g_windowsDeleteString = NULL;

static const GUID IID_UI_SETTINGS_3 = {
    0x03021be4, 0x5254, 0x4781,
    {0x81, 0x94, 0x51, 0x68, 0xf7, 0xd0, 0x6d, 0x7b}
};

static BOOL IsSupportedWindowsBuild(void) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn rtlGetVersion = NULL;
    OSVERSIONINFOW versionInfo = {0};
    FARPROC proc;

    if (!ntdll) return FALSE;
    proc = GetProcAddress(ntdll, "RtlGetVersion");
    memcpy(&rtlGetVersion, &proc, sizeof(rtlGetVersion));
    if (!rtlGetVersion) return FALSE;

    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    if (rtlGetVersion(&versionInfo) != 0) return FALSE;
    g_windowsBuild = versionInfo.dwBuildNumber;
    /* Windows 11 still reports major version 10. Do not assume ordinal-based
     * UxTheme entry points remain compatible on a future major version. */
    return versionInfo.dwMajorVersion == 10 &&
           versionInfo.dwBuildNumber >= WINDOWS_10_1809_BUILD;
}

static BOOL IsHighContrastActive(void) {
    HIGHCONTRASTW highContrast = {0};
    highContrast.cbSize = sizeof(highContrast);
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast),
                                 &highContrast, 0) &&
           (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

static BOOL IsAppsDarkThemeRegistryFallback(void) {
    HKEY key = NULL;
    DWORD value = 1;
    DWORD valueSize = sizeof(value);
    DWORD type = 0;
    LSTATUS status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_QUERY_VALUE, &key);
    if (status != ERROR_SUCCESS) return FALSE;

    status = RegQueryValueExW(key, L"AppsUseLightTheme", NULL, &type,
                              (BYTE*)&value, &valueSize);
    RegCloseKey(key);
    return status == ERROR_SUCCESS && type == REG_DWORD && value == 0;
}

static BOOL IsAppsDarkTheme(void) {
    if (g_shouldAppsUseDarkMode) {
        return g_shouldAppsUseDarkMode();
    }
    return IsAppsDarkThemeRegistryFallback();
}

static BOOL IsSystemDarkThemeRegistryFallback(void) {
    HKEY key = NULL;
    DWORD value = 1;
    DWORD valueSize = sizeof(value);
    DWORD type = 0;
    LSTATUS status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_QUERY_VALUE, &key);
    if (status != ERROR_SUCCESS) return FALSE;

    status = RegQueryValueExW(key, L"SystemUsesLightTheme", NULL, &type,
                              (BYTE*)&value, &valueSize);
    RegCloseKey(key);
    return status == ERROR_SUCCESS && type == REG_DWORD && value == 0;
}

static BOOL IsSystemDarkTheme(void) {
    return IsSystemDarkThemeRegistryFallback();
}

static void LoadNamedFunction(HMODULE module, const char* name,
                              void* target, size_t targetSize) {
    FARPROC proc = module ? GetProcAddress(module, name) : NULL;
    if (target && targetSize == sizeof(proc)) {
        memcpy(target, &proc, targetSize);
    }
}

static BOOL EnsureWinRtThemeFunctions(void) {
    if (g_combaseLoadAttempted) {
        return g_roInitialize && g_roUninitialize &&
               g_roActivateInstance && g_windowsCreateString &&
               g_windowsDeleteString;
    }
    g_combaseLoadAttempted = TRUE;
    g_combase = LoadLibraryW(L"combase.dll");
    if (!g_combase) return FALSE;

    LoadNamedFunction(g_combase, "RoInitialize", &g_roInitialize,
                      sizeof(g_roInitialize));
    LoadNamedFunction(g_combase, "RoUninitialize", &g_roUninitialize,
                      sizeof(g_roUninitialize));
    LoadNamedFunction(g_combase, "RoActivateInstance", &g_roActivateInstance,
                      sizeof(g_roActivateInstance));
    LoadNamedFunction(g_combase, "WindowsCreateString", &g_windowsCreateString,
                      sizeof(g_windowsCreateString));
    LoadNamedFunction(g_combase, "WindowsDeleteString", &g_windowsDeleteString,
                      sizeof(g_windowsDeleteString));
    return g_roInitialize && g_roUninitialize &&
           g_roActivateInstance && g_windowsCreateString &&
           g_windowsDeleteString;
}

static BOOL QueryUiSettingsDarkTheme(BOOL* darkTheme) {
    HSTRING className = NULL;
    IInspectable* instance = NULL;
    __x_ABI_CWindows_CUI_CViewManagement_CIUISettings3* settings = NULL;
    __x_ABI_CWindows_CUI_CColor background = {0};
    HRESULT initializeResult;
    HRESULT hr;
    BOOL shouldUninitialize = FALSE;
    BOOL success = FALSE;

    if (!darkTheme || !EnsureWinRtThemeFunctions()) return FALSE;
    *darkTheme = FALSE;

    initializeResult = g_roInitialize(RO_INIT_SINGLETHREADED);
    if (SUCCEEDED(initializeResult)) {
        shouldUninitialize = TRUE;
    } else if (initializeResult != RPC_E_CHANGED_MODE) {
        return FALSE;
    }

    hr = g_windowsCreateString(
        RuntimeClass_Windows_UI_ViewManagement_UISettings,
        (UINT32)wcslen(RuntimeClass_Windows_UI_ViewManagement_UISettings),
        &className);
    if (SUCCEEDED(hr)) {
        hr = g_roActivateInstance(className, &instance);
    }
    if (SUCCEEDED(hr) && instance) {
        hr = instance->lpVtbl->QueryInterface(
            instance, &IID_UI_SETTINGS_3, (void**)&settings);
    }
    if (SUCCEEDED(hr) && settings) {
        hr = settings->lpVtbl->GetColorValue(
            settings, UIColorType_Background, &background);
    }
    if (SUCCEEDED(hr)) {
        unsigned int luminance = 299u * background.R +
                                 587u * background.G +
                                 114u * background.B;
        *darkTheme = luminance < 128000u;
        success = TRUE;
    }

    if (settings) settings->lpVtbl->Release(settings);
    if (instance) instance->lpVtbl->Release(instance);
    if (className) (void)g_windowsDeleteString(className);
    if (shouldUninitialize) g_roUninitialize();
    return success;
}

static BOOL QueryEffectiveDarkTheme(void) {
    BOOL darkTheme = FALSE;
    if (QueryUiSettingsDarkTheme(&darkTheme)) {
        return darkTheme;
    }
    return IsAppsDarkTheme() || IsSystemDarkTheme();
}

static void LoadOrdinalFunction(HMODULE module, WORD ordinal,
                                void* target, size_t targetSize) {
    FARPROC proc = module
        ? GetProcAddress(module, MAKEINTRESOURCEA(ordinal))
        : NULL;
    if (target && targetSize == sizeof(proc)) {
        memcpy(target, &proc, targetSize);
    }
}

BOOL InitializeNativeMenuTheme(void) {
    if (g_initialized) return g_supported;
    g_initialized = TRUE;

    if (!IsSupportedWindowsBuild()) return FALSE;

    g_uxtheme = LoadLibraryW(L"uxtheme.dll");
    if (!g_uxtheme) return FALSE;

    LoadOrdinalFunction(g_uxtheme,
                        UXTHEME_REFRESH_IMMERSIVE_COLOR_POLICY_ORDINAL,
                        &g_refreshImmersiveColorPolicyState,
                        sizeof(g_refreshImmersiveColorPolicyState));
    LoadOrdinalFunction(g_uxtheme,
                        UXTHEME_SHOULD_APPS_USE_DARK_MODE_ORDINAL,
                        &g_shouldAppsUseDarkMode,
                        sizeof(g_shouldAppsUseDarkMode));
    LoadOrdinalFunction(g_uxtheme,
                        UXTHEME_ALLOW_DARK_MODE_FOR_WINDOW_ORDINAL,
                        &g_allowDarkModeForWindow,
                        sizeof(g_allowDarkModeForWindow));
    LoadOrdinalFunction(g_uxtheme,
                        UXTHEME_SET_PREFERRED_APP_MODE_ORDINAL,
                        &g_setPreferredAppMode,
                        sizeof(g_setPreferredAppMode));
    LoadOrdinalFunction(g_uxtheme, UXTHEME_FLUSH_MENU_THEMES_ORDINAL,
                        &g_flushMenuThemes, sizeof(g_flushMenuThemes));
    if (!g_setPreferredAppMode || !g_flushMenuThemes) {
        LOG_INFO("Native dark menu APIs are unavailable; using standard menus");
        return FALSE;
    }

    g_supported = TRUE;
    RefreshNativeMenuTheme();
    return TRUE;
}

void ApplyNativeMenuThemeToWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    if (!g_initialized) {
        (void)InitializeNativeMenuTheme();
    } else if (GetAncestor(hwnd, GA_ROOT) == hwnd) {
        RefreshNativeMenuTheme();
    }
    if (g_supported && g_allowDarkModeForWindow) {
        (void)g_allowDarkModeForWindow(hwnd, !IsHighContrastActive());
    }
}

void RefreshNativeMenuTheme(void) {
    PreferredAppMode desiredMode;
    BOOL darkModeActive;
    BOOL highContrastActive;
    BOOL modeChanged;

    if (!g_initialized) {
        (void)InitializeNativeMenuTheme();
    }
    if (!g_supported) return;

    if (g_refreshImmersiveColorPolicyState) {
        g_refreshImmersiveColorPolicyState();
    }
    highContrastActive = IsHighContrastActive();
    darkModeActive = !highContrastActive && QueryEffectiveDarkTheme();
    if (highContrastActive) {
        desiredMode = PREFERRED_APP_MODE_DEFAULT;
    } else if (g_windowsBuild >= WINDOWS_10_1903_BUILD) {
        desiredMode = darkModeActive
            ? PREFERRED_APP_MODE_FORCE_DARK
            : PREFERRED_APP_MODE_FORCE_LIGHT;
    } else {
        /* Build 17763 exposes AllowDarkModeForApp at the same ordinal with a
         * BOOL-compatible ABI, so use TRUE/FALSE values only. */
        desiredMode = darkModeActive
            ? PREFERRED_APP_MODE_ALLOW_DARK
            : PREFERRED_APP_MODE_DEFAULT;
    }

    modeChanged = desiredMode != g_appliedMode;
    if (modeChanged) {
        (void)g_setPreferredAppMode(desiredMode);
        g_appliedMode = desiredMode;
    }
    if (darkModeActive != g_darkModeActive || modeChanged) {
        g_darkModeActive = darkModeActive;
        g_flushMenuThemes();
    }
}

BOOL IsNativeMenuDarkModeActive(void) {
    if (!g_initialized) {
        (void)InitializeNativeMenuTheme();
    }
    return g_supported && g_darkModeActive;
}

BOOL IsApplicationDarkModeActive(void) {
    if (!g_initialized) {
        (void)InitializeNativeMenuTheme();
    }
    if (g_supported) {
        RefreshNativeMenuTheme();
        return g_darkModeActive;
    }
    return !IsHighContrastActive() && QueryEffectiveDarkTheme();
}
