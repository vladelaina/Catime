#include "dialog/dialog_modern.h"
#include "tray/tray_menu_theme.h"
#include "utils/win32_dynamic_loader.h"
#include <dwmapi.h>
#include <wchar.h>

#define MODERN_DWM_CORNER_ATTRIBUTE 33
#define MODERN_DWM_CORNER_ROUND 2
#define MODERN_DWM_NC_RENDERING_POLICY_ATTRIBUTE 2
#define MODERN_DWM_NC_RENDERING_DISABLED 1
#define MODERN_THEME_MODE_PROP L"Catime.DialogThemeMode"

typedef HRESULT (WINAPI *DialogModernSetWindowThemeFn)(HWND, LPCWSTR, LPCWSTR);
static DialogModernSetWindowThemeFn g_setWindowTheme = NULL;
static BOOL g_setWindowThemeResolved = FALSE;

static void DialogModernResolveWindowThemeFunction(void) {
    HMODULE uxtheme = GetModuleHandleW(L"uxtheme.dll");
    if (!uxtheme) uxtheme = LoadLibraryW(L"uxtheme.dll");
    if (uxtheme) CATIME_LOAD_PROC_ADDRESS(uxtheme, "SetWindowTheme", g_setWindowTheme);
    g_setWindowThemeResolved = TRUE;
}

typedef struct { BOOL darkMode; } DialogModernThemeChildrenContext;

static BOOL CALLBACK DialogModernApplyThemeToChild(HWND child, LPARAM data) {
    const DialogModernThemeChildrenContext* context =
        (const DialogModernThemeChildrenContext*)data;
    if (context) DialogModern_ApplyTheme(child, context->darkMode);
    return TRUE;
}

void DialogModern_ApplyTheme(HWND hwnd, BOOL darkMode) {
    INT_PTR desiredMode = darkMode ? 2 : 1;
    if (!hwnd) return;
    BOOL rootWindow = GetAncestor(hwnd, GA_ROOT) == hwnd;
    BOOL modeChanged = (INT_PTR)GetPropW(hwnd, MODERN_THEME_MODE_PROP) != desiredMode;
    if (modeChanged) {
        SetPropW(hwnd, MODERN_THEME_MODE_PROP, (HANDLE)desiredMode);
        ApplyNativeMenuThemeToWindow(hwnd);
        if (!g_setWindowThemeResolved) DialogModernResolveWindowThemeFunction();
        if (g_setWindowTheme) {
            const wchar_t* themeName = NULL;
            if (darkMode) {
                wchar_t className[64] = {0};
                GetClassNameW(hwnd, className, (int)_countof(className));
                themeName = (_wcsicmp(className, L"ComboBox") == 0 ||
                             _wcsicmp(className, L"msctls_updown32") == 0)
                    ? L"DarkMode_CFD" : L"DarkMode_Explorer";
            }
            (void)g_setWindowTheme(hwnd, themeName, NULL);
        }
    }
    if (rootWindow) {
        if (modeChanged) {
            BOOL enabled = darkMode;
            HRESULT result = DwmSetWindowAttribute(hwnd, 20, &enabled, sizeof(enabled));
            if (FAILED(result)) (void)DwmSetWindowAttribute(hwnd, 19, &enabled, sizeof(enabled));
        }
        DialogModernThemeChildrenContext context = {darkMode};
        EnumChildWindows(hwnd, DialogModernApplyThemeToChild, (LPARAM)&context);
    }
}

void DialogModern_DisablePopupShadow(HWND hwnd) {
    if (!hwnd) return;
    LONG_PTR classStyle = GetClassLongPtrW(hwnd, GCL_STYLE);
    if ((classStyle & CS_DROPSHADOW) != 0) {
        (void)SetClassLongPtrW(hwnd, GCL_STYLE, classStyle & ~CS_DROPSHADOW);
    }
    int policy = MODERN_DWM_NC_RENDERING_DISABLED;
    (void)DwmSetWindowAttribute(hwnd, MODERN_DWM_NC_RENDERING_POLICY_ATTRIBUTE,
                                &policy, sizeof(policy));
}

void DialogModern_ApplyWindowShape(HWND hwnd, UINT dpi, int cornerRadius) {
    if (!hwnd) return;
    int preference = MODERN_DWM_CORNER_ROUND;
    HRESULT rounded = DwmSetWindowAttribute(hwnd, MODERN_DWM_CORNER_ATTRIBUTE,
                                             &preference, sizeof(preference));
    if (SUCCEEDED(rounded)) return;
    RECT client = {0};
    GetClientRect(hwnd, &client);
    int radius = DialogModern_Scale(dpi, cornerRadius);
    HRGN region = CreateRoundRectRgn(client.left, client.top, client.right + 1,
                                     client.bottom + 1, radius * 2, radius * 2);
    if (region && !SetWindowRgn(hwnd, region, TRUE)) DeleteObject(region);
}
