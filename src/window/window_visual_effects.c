/**
 * @file window_visual_effects.c
 * @brief Window visual effects implementation
 */

#include "window/window_visual_effects.h"
#include "log.h"
#include <dwmapi.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ALPHA_OPAQUE 255
#define BLUR_ALPHA_VALUE 180
#define BLUR_GRADIENT_COLOR 0x00FFFFFF
#define DWMAPI_DLL L"dwmapi.dll"

/* ============================================================================
 * DWM function pointers
 * ============================================================================ */

typedef HRESULT (WINAPI *pfnDwmEnableBlurBehindWindow)(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind);
static pfnDwmEnableBlurBehindWindow _DwmEnableBlurBehindWindow = NULL;

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

WINUSERAPI BOOL WINAPI SetWindowCompositionAttribute(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA* pData);

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

/* ============================================================================
 * Implementation
 * ============================================================================ */

BOOL InitDWMFunctions(void) {
    HMODULE hDwmapi = LoadLibraryW(DWMAPI_DLL);
    if (hDwmapi) {
        _DwmEnableBlurBehindWindow = (pfnDwmEnableBlurBehindWindow)GetProcAddress(hDwmapi, "DwmEnableBlurBehindWindow");
        
        if (_DwmEnableBlurBehindWindow) {
            LOG_INFO("DWM functions loaded successfully");
            return TRUE;
        }
    }
    LOG_WARNING("Failed to load DWM functions");
    return FALSE;
}

/* Global flag for soft click-through (using WM_NCHITTEST instead of WS_EX_TRANSPARENT) */
static BOOL g_softClickThrough = FALSE;

BOOL IsSoftClickThroughEnabled(void) {
    return g_softClickThrough;
}

void SetClickThrough(HWND hwnd, BOOL enable) {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~WS_EX_TRANSPARENT;

    if (enable) {
        /* Use soft click-through (WM_NCHITTEST returns HTTRANSPARENT) 
         * instead of WS_EX_TRANSPARENT to allow selective clicking */
        g_softClickThrough = TRUE;
        LOG_INFO("Click-through enabled (soft mode)");
    } else {
        g_softClickThrough = FALSE;
        LOG_INFO("Click-through disabled");
    }

    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

static void ApplyAccentPolicy(HWND hwnd, ACCENT_STATE accentState) {
    extern int CLOCK_WINDOW_OPACITY;
    // Map 0-100 opacity to 0-255 alpha for the acrylic background
    // For "Liquid Glass", we want it very clear/transparent.
    // Lower the base alpha significantly (e.g., max 30 out of 255) to avoid "milky" look.
    DWORD alpha = (DWORD)((CLOCK_WINDOW_OPACITY * 30) / 100);
    
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
    
    if (SetWindowCompositionAttribute) {
        SetWindowCompositionAttribute(hwnd, &data);
    } else if (_DwmEnableBlurBehindWindow) {
        // Fallback for older Windows versions
        DWM_BLURBEHIND bb = {0};
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = (accentState != ACCENT_DISABLED);
        bb.hRgnBlur = NULL;
        _DwmEnableBlurBehindWindow(hwnd, &bb);
    }
}

void SetBlurBehind(HWND hwnd, BOOL enable) {
    ApplyAccentPolicy(hwnd, enable ? ACCENT_ENABLE_ACRYLICBLURBEHIND : ACCENT_DISABLED);
    LOG_INFO("Acrylic blur effect %s", enable ? "enabled" : "disabled");
}

