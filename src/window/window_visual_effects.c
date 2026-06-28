/**
 * @file window_visual_effects.c
 * @brief Window visual effects implementation
 */

#include "window/window_visual_effects.h"
#include "log.h"
#include "markdown/markdown_interactive.h"
#include "utils/win32_dynamic_loader.h"
#include <dwmapi.h>
#include <wchar.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ALPHA_OPAQUE 255
#define BLUR_ALPHA_VALUE 180
#define BLUR_GRADIENT_COLOR 0x00FFFFFF
#define DWMAPI_DLL L"dwmapi.dll"
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

/* ============================================================================
 * DWM function pointers
 * ============================================================================ */

typedef HRESULT (WINAPI *pfnDwmEnableBlurBehindWindow)(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind);
static pfnDwmEnableBlurBehindWindow _DwmEnableBlurBehindWindow = NULL;
static HMODULE g_hDwmapi = NULL;

/* ============================================================================
 * Windows composition structures (for blur effects)
 * ============================================================================ */

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

/* Function pointer for SetWindowCompositionAttribute (undocumented API) */
typedef BOOL (WINAPI *pfnSetWindowCompositionAttribute)(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA* pData);
static pfnSetWindowCompositionAttribute _SetWindowCompositionAttribute = NULL;
static BOOL g_dwmFunctionsInitialized = FALSE;

/* ============================================================================
 * Implementation
 * ============================================================================ */

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
    
    /* Load SetWindowCompositionAttribute from user32.dll (undocumented API for acrylic blur) */
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

/* Click-through state */
static BOOL g_clickThroughEnabled = FALSE;
static BOOL g_currentlyTransparent = FALSE;
static BOOL g_clickThroughTimerActive = FALSE;
static HWND g_clickThroughTimerHwnd = NULL;
static BOOL g_blurStateValid = FALSE;
static HWND g_blurStateHwnd = NULL;
static ACCENT_STATE g_blurAccentState = ACCENT_DISABLED;
static DWORD g_blurAlpha = 0;

/* Timer ID for mouse position checking */
#define TIMER_ID_CLICK_THROUGH 201
#define CLICK_THROUGH_CHECK_INTERVAL 50  /* ms */

static BOOL IsValidVisualEffectsWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

BOOL IsSoftClickThroughEnabled(void) {
    return g_clickThroughEnabled;
}

static void StopClickThroughTimer(HWND fallbackHwnd) {
    HWND timerHwnd = g_clickThroughTimerHwnd ? g_clickThroughTimerHwnd : fallbackHwnd;
    if (g_clickThroughTimerActive && IsValidVisualEffectsWindow(timerHwnd)) {
        KillTimer(timerHwnd, TIMER_ID_CLICK_THROUGH);
    }
    g_clickThroughTimerActive = FALSE;
    g_clickThroughTimerHwnd = NULL;
}

static BOOL StartClickThroughTimer(HWND hwnd) {
    if (!IsValidVisualEffectsWindow(hwnd)) {
        return FALSE;
    }

    if (g_clickThroughTimerActive &&
        g_clickThroughTimerHwnd == hwnd &&
        IsValidVisualEffectsWindow(g_clickThroughTimerHwnd)) {
        return TRUE;
    }

    if (!SetTimer(hwnd, TIMER_ID_CLICK_THROUGH,
                  CLICK_THROUGH_CHECK_INTERVAL, NULL)) {
        LOG_WARNING("Failed to start click-through timer (error=%lu)", GetLastError());
        return FALSE;
    }

    HWND previousHwnd = g_clickThroughTimerHwnd;
    if (g_clickThroughTimerActive &&
        previousHwnd != hwnd &&
        IsValidVisualEffectsWindow(previousHwnd)) {
        KillTimer(previousHwnd, TIMER_ID_CLICK_THROUGH);
    }

    g_clickThroughTimerActive = TRUE;
    g_clickThroughTimerHwnd = hwnd;
    return TRUE;
}

static void RestoreInteractiveClickThroughStyle(HWND hwnd) {
    if (!IsValidVisualEffectsWindow(hwnd)) {
        return;
    }

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TRANSPARENT) {
        exStyle &= ~WS_EX_TRANSPARENT;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    g_currentlyTransparent = FALSE;
}

static void DisableSoftClickThroughFallback(HWND hwnd) {
    StopClickThroughTimer(hwnd);
    g_clickThroughEnabled = FALSE;
    RestoreInteractiveClickThroughStyle(hwnd);
}

/* Update WS_EX_TRANSPARENT based on mouse position over clickable regions */
void UpdateClickThroughState(HWND hwnd) {
    if (!g_clickThroughEnabled ||
        hwnd != g_clickThroughTimerHwnd ||
        !IsValidVisualEffectsWindow(hwnd)) {
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
    
    /* Check if mouse is over window */
    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);
    if (!PtInRect(&rcWindow, pt)) {
        /* Mouse outside window - ensure transparent */
        if (!g_currentlyTransparent) {
            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            exStyle |= WS_EX_TRANSPARENT;
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
            g_currentlyTransparent = TRUE;
        }
        return;
    }
    
    /* Check if mouse is over a clickable region */
    
    UpdateRegionPositions(rcWindow.left, rcWindow.top);
    BOOL overClickableRegion = IsClickableRegionAt(pt);

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    if (overClickableRegion) {
        /* Mouse over clickable region - remove WS_EX_TRANSPARENT to allow clicks */
        if (g_currentlyTransparent) {
            exStyle &= ~WS_EX_TRANSPARENT;
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
            g_currentlyTransparent = FALSE;
        }
    } else {
        /* Mouse not over clickable region - add WS_EX_TRANSPARENT to pass through */
        if (!g_currentlyTransparent) {
            exStyle |= WS_EX_TRANSPARENT;
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
            g_currentlyTransparent = TRUE;
        }
    }
}

void RefreshClickThroughState(HWND hwnd) {
    if (!g_clickThroughEnabled || !IsValidVisualEffectsWindow(hwnd)) {
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
    if (!IsValidVisualEffectsWindow(hwnd)) return;

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    LONG originalExStyle = exStyle;

    BOOL styleTransparent = (exStyle & WS_EX_TRANSPARENT) != 0;
    if (g_clickThroughEnabled == enable &&
        g_currentlyTransparent == enable &&
        styleTransparent == enable &&
        ((enable && g_clickThroughTimerActive) ||
         (!enable && !g_clickThroughTimerActive))) {
        if (!enable ||
            (g_clickThroughTimerHwnd == hwnd &&
             IsValidVisualEffectsWindow(g_clickThroughTimerHwnd))) {
            return;
        }
    }

    if (enable && g_clickThroughTimerActive && g_clickThroughTimerHwnd != hwnd) {
        StopClickThroughTimer(NULL);
    }

    if (enable) {
        /* Enable soft click-through; polling is only needed while clickable regions exist. */
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

        /* Stop timer */
        StopClickThroughTimer(hwnd);
    }

    if (exStyle != originalExStyle) {
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    if (enable && g_clickThroughTimerActive) {
        UpdateClickThroughState(hwnd);
    }
}

/* Get timer ID for external handling */
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
    if (!IsValidVisualEffectsWindow(hwnd)) {
        if (g_blurStateHwnd == hwnd) {
            g_blurStateValid = FALSE;
            g_blurStateHwnd = NULL;
            g_blurAccentState = ACCENT_DISABLED;
            g_blurAlpha = 0;
        }
        return FALSE;
    }

    extern int CLOCK_WINDOW_OPACITY;
    // Map 0-100 opacity to 0-255 alpha for the acrylic background
    // For "Liquid Glass", we want it very clear/transparent.
    // Lower the base alpha significantly (e.g., max 30 out of 255) to avoid "milky" look.
    DWORD alpha = (DWORD)((CLOCK_WINDOW_OPACITY * 30) / 100);
    if (g_blurStateValid &&
        g_blurStateHwnd == hwnd &&
        g_blurAccentState == accentState &&
        g_blurAlpha == alpha) {
        return TRUE;
    }
    
    ACCENT_POLICY policy = {0};
    policy.AccentState = accentState;
    policy.AccentFlags = 0;
    
    // Acrylic requires the color format AABBGGRR
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
        // Fallback for older Windows versions
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

