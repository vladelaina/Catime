#include "window/window_visual_effects.h"
#include "window/window_visual_effects_internal.h"
#include "log.h"
#include "markdown/markdown_interactive.h"
#include "utils/win32_dynamic_loader.h"
#include <dwmapi.h>
#include <wchar.h>
#define ALPHA_OPAQUE 255
#define BLUR_ALPHA_VALUE 180
#define BLUR_GRADIENT_COLOR 0x00FFFFFF
#define DWMAPI_DLL L"dwmapi.dll"
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
typedef HRESULT (WINAPI *pfnDwmEnableBlurBehindWindow)(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind);
static pfnDwmEnableBlurBehindWindow _DwmEnableBlurBehindWindow = NULL;
static HMODULE g_hDwmapi = NULL;
typedef enum _WINDOWCOMPOSITIONATTRIB {
    WCA_UNDEFINED = 0,
    WCA_ACCENT_POLICY = 19,
    WCA_LAST = 27
} WINDOWCOMPOSITIONATTRIB;
typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;
typedef enum _ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
} ACCENT_STATE;
typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
} ACCENT_POLICY;
typedef BOOL (WINAPI *pfnSetWindowCompositionAttribute)(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA* pData);
static pfnSetWindowCompositionAttribute _SetWindowCompositionAttribute = NULL;
static BOOL g_dwmFunctionsInitialized = FALSE;
BOOL InitDWMFunctions(void) {
    if (g_dwmFunctionsInitialized) {
        return _DwmEnableBlurBehindWindow || _SetWindowCompositionAttribute;
    }
    if (!g_hDwmapi) {
        g_hDwmapi = LoadLibraryW(DWMAPI_DLL);
    }
    if (g_hDwmapi) {
        CATIME_LOAD_PROC_ADDRESS(g_hDwmapi, "DwmEnableBlurBehindWindow", _DwmEnableBlurBehindWindow);
    }
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        CATIME_LOAD_PROC_ADDRESS(hUser32, "SetWindowCompositionAttribute", _SetWindowCompositionAttribute);
    }
    g_dwmFunctionsInitialized = TRUE;
    if (_DwmEnableBlurBehindWindow || _SetWindowCompositionAttribute) {
        return TRUE;
    }
    LOG_WARNING("Failed to load DWM/composition functions");
    return FALSE;
}
static BOOL g_clickThroughEnabled = FALSE;
static BOOL g_currentlyTransparent = FALSE;
static BOOL g_clickThroughTimerActive = FALSE;
static HWND g_clickThroughTimerHwnd = NULL;
static BOOL g_blurStateValid = FALSE;
static HWND g_blurStateHwnd = NULL;
static ACCENT_STATE g_blurAccentState = ACCENT_DISABLED;
static DWORD g_blurAlpha = 0;
#define TIMER_ID_CLICK_THROUGH 201
#define CLICK_THROUGH_CHECK_INTERVAL 50  /* ms */
BOOL IsSoftClickThroughEnabled(void) {
    return g_clickThroughEnabled;
}
static void StopClickThroughTimer(HWND fallbackHwnd) {
    HWND timerHwnd = g_clickThroughTimerHwnd ? g_clickThroughTimerHwnd : fallbackHwnd;
    if (g_clickThroughTimerActive && WindowVisualEffects_IsValidWindow(timerHwnd)) {
        KillTimer(timerHwnd, TIMER_ID_CLICK_THROUGH);
    }
    g_clickThroughTimerActive = FALSE;
    g_clickThroughTimerHwnd = NULL;
}
static BOOL StartClickThroughTimer(HWND hwnd) {
    if (!WindowVisualEffects_IsValidWindow(hwnd)) {
        return FALSE;
    }
    if (g_clickThroughTimerActive && g_clickThroughTimerHwnd == hwnd && WindowVisualEffects_IsValidWindow(g_clickThroughTimerHwnd)) {
        return TRUE;
    }
    if (!SetTimer(hwnd, TIMER_ID_CLICK_THROUGH, CLICK_THROUGH_CHECK_INTERVAL, NULL)) {
        LOG_WARNING("Failed to start click-through timer (error=%lu)", GetLastError());
        return FALSE;
    }
    HWND previousHwnd = g_clickThroughTimerHwnd;
    if (g_clickThroughTimerActive && previousHwnd != hwnd && WindowVisualEffects_IsValidWindow(previousHwnd)) {
        KillTimer(previousHwnd, TIMER_ID_CLICK_THROUGH);
    }
    g_clickThroughTimerActive = TRUE;
    g_clickThroughTimerHwnd = hwnd;
    return TRUE;
}
static void RestoreInteractiveClickThroughStyle(HWND hwnd) {
    if (!WindowVisualEffects_IsValidWindow(hwnd)) {
        return;
    }
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TRANSPARENT) {
        exStyle &= ~WS_EX_TRANSPARENT;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    g_currentlyTransparent = FALSE;
}
static void DisableSoftClickThroughFallback(HWND hwnd) {
    StopClickThroughTimer(hwnd);
    g_clickThroughEnabled = FALSE;
    RestoreInteractiveClickThroughStyle(hwnd);
}
void UpdateClickThroughState(HWND hwnd) {
    if (!g_clickThroughEnabled || hwnd != g_clickThroughTimerHwnd || !WindowVisualEffects_IsValidWindow(hwnd)) {
        return;
    }
    extern BOOL CLOCK_EDIT_MODE;
    if (CLOCK_EDIT_MODE) return;
    if (!HasClickableRegions()) {
        StopClickThroughTimer(hwnd);
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (!g_currentlyTransparent || !(exStyle & WS_EX_TRANSPARENT)) {
            exStyle |= WS_EX_TRANSPARENT;
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
            g_currentlyTransparent = TRUE;
        }
        return;
    }
    POINT pt;
    GetCursorPos(&pt);
    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);
    if (!PtInRect(&rcWindow, pt)) {
        if (!g_currentlyTransparent) {
            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            exStyle |= WS_EX_TRANSPARENT;
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
            g_currentlyTransparent = TRUE;
        }
        return;
    }
    UpdateRegionPositions(rcWindow.left, rcWindow.top);
    BOOL overClickableRegion = IsClickableRegionAt(pt);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (overClickableRegion) {
        if (g_currentlyTransparent) {
            exStyle &= ~WS_EX_TRANSPARENT;
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
            g_currentlyTransparent = FALSE;
        }
    } else {
        if (!g_currentlyTransparent) {
            exStyle |= WS_EX_TRANSPARENT;
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
            g_currentlyTransparent = TRUE;
        }
    }
}
void RefreshClickThroughState(HWND hwnd) {
    if (!g_clickThroughEnabled || !WindowVisualEffects_IsValidWindow(hwnd)) {
        return;
    }
    extern BOOL CLOCK_EDIT_MODE;
    if (CLOCK_EDIT_MODE) {
        StopClickThroughTimer(hwnd);
        RestoreInteractiveClickThroughStyle(hwnd);
        return;
    }
    if (!HasClickableRegions()) {
        StopClickThroughTimer(hwnd);
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (!g_currentlyTransparent || !(exStyle & WS_EX_TRANSPARENT)) {
            exStyle |= WS_EX_TRANSPARENT;
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
            g_currentlyTransparent = TRUE;
        }
        return;
    }
    if (!StartClickThroughTimer(hwnd)) {
        DisableSoftClickThroughFallback(hwnd);
        return;
    }
    UpdateClickThroughState(hwnd);
}
void SetClickThrough(HWND hwnd, BOOL enable) {
    if (!WindowVisualEffects_IsValidWindow(hwnd)) return;
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    LONG originalExStyle = exStyle;
    BOOL styleTransparent = (exStyle & WS_EX_TRANSPARENT) != 0;
    if (g_clickThroughEnabled == enable && g_currentlyTransparent == enable && styleTransparent == enable && ((enable && g_clickThroughTimerActive) || (!enable && !g_clickThroughTimerActive))) {
        if (!enable || (g_clickThroughTimerHwnd == hwnd && WindowVisualEffects_IsValidWindow(g_clickThroughTimerHwnd))) {
            return;
        }
    }
    if (enable && g_clickThroughTimerActive && g_clickThroughTimerHwnd != hwnd) {
        StopClickThroughTimer(NULL);
    }
    if (enable) {
        BOOL hasClickableRegions = HasClickableRegions();
        g_clickThroughEnabled = TRUE;
        exStyle |= WS_EX_TRANSPARENT;
        g_currentlyTransparent = TRUE;
        if (hasClickableRegions && !StartClickThroughTimer(hwnd)) {
            g_clickThroughEnabled = FALSE;
            exStyle &= ~WS_EX_TRANSPARENT;
            g_currentlyTransparent = FALSE;
        } else if (!hasClickableRegions) {
            StopClickThroughTimer(hwnd);
        }
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
        g_clickThroughEnabled = FALSE;
        g_currentlyTransparent = FALSE;
        StopClickThroughTimer(hwnd);
    }
    if (exStyle != originalExStyle) {
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    if (enable && g_clickThroughTimerActive) {
        UpdateClickThroughState(hwnd);
    }
}
UINT GetClickThroughTimerId(void) {
    return TIMER_ID_CLICK_THROUGH;
}
void CleanupWindowVisualEffects(HWND hwnd) {
    if (!hwnd || hwnd == g_clickThroughTimerHwnd) {
        StopClickThroughTimer(hwnd);
    }
    g_clickThroughEnabled = FALSE;
    g_currentlyTransparent = FALSE;
    if (!hwnd || g_blurStateHwnd == hwnd) {
        g_blurStateValid = FALSE;
        g_blurStateHwnd = NULL;
        g_blurAccentState = ACCENT_DISABLED;
        g_blurAlpha = 0;
    }
}
void ShutdownWindowVisualEffects(void) {
    CleanupWindowVisualEffects(NULL);
    _DwmEnableBlurBehindWindow = NULL;
    _SetWindowCompositionAttribute = NULL;
    g_dwmFunctionsInitialized = FALSE;
    if (g_hDwmapi) {
        FreeLibrary(g_hDwmapi);
        g_hDwmapi = NULL;
    }
}
static BOOL ApplyAccentPolicy(HWND hwnd, ACCENT_STATE accentState) {
    if (!WindowVisualEffects_IsValidWindow(hwnd)) {
        if (g_blurStateHwnd == hwnd) {
            g_blurStateValid = FALSE;
            g_blurStateHwnd = NULL;
            g_blurAccentState = ACCENT_DISABLED;
            g_blurAlpha = 0;
        }
        return FALSE;
    }
    extern int CLOCK_WINDOW_OPACITY;
    DWORD alpha = (DWORD)((CLOCK_WINDOW_OPACITY * 30) / 100);
    if (g_blurStateValid && g_blurStateHwnd == hwnd && g_blurAccentState == accentState && g_blurAlpha == alpha) {
        return TRUE;
    }
    ACCENT_POLICY policy = {0};
    policy.AccentState = accentState;
    policy.AccentFlags = 0;
    policy.GradientColor = (accentState != ACCENT_DISABLED) ?
                          ((alpha << 24) | 0x00FFFFFF) : 0;
    WINDOWCOMPOSITIONATTRIBDATA data = {0};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &policy;
    data.cbData = sizeof(policy);
    BOOL applied = FALSE;
    if (_SetWindowCompositionAttribute) {
        applied = _SetWindowCompositionAttribute(hwnd, &data);
    } else if (_DwmEnableBlurBehindWindow) {
        DWM_BLURBEHIND bb = {0};
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = (accentState != ACCENT_DISABLED);
        bb.hRgnBlur = NULL;
        applied = SUCCEEDED(_DwmEnableBlurBehindWindow(hwnd, &bb));
    }
    if (applied) {
        g_blurStateValid = TRUE;
        g_blurStateHwnd = hwnd;
        g_blurAccentState = accentState;
        g_blurAlpha = alpha;
    }
    return applied;
}
void SetBlurBehind(HWND hwnd, BOOL enable) {
    ApplyAccentPolicy(hwnd, enable ? ACCENT_ENABLE_ACRYLICBLURBEHIND : ACCENT_DISABLED);
}
