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

#define COLOR_KEY_BLACK RGB(0, 0, 0)
#define ALPHA_OPAQUE 255
#define BLUR_ALPHA_VALUE 180           /* 180/255 = 70% opacity balances blur visibility with background transparency */
#define BLUR_GRADIENT_COLOR 0x00202020  /* Dark gray (RGB 32,32,32) provides subtle background tint for blur effect */
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

void SetClickThrough(HWND hwnd, BOOL enable) {
    extern int CLOCK_WINDOW_OPACITY;
    BYTE alphaValue = (BYTE)((CLOCK_WINDOW_OPACITY * 255) / 100);

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~WS_EX_TRANSPARENT;

    if (enable) {
        exStyle |= WS_EX_TRANSPARENT;
        if (exStyle & WS_EX_LAYERED) {
            SetLayeredWindowAttributes(hwnd, COLOR_KEY_BLACK, alphaValue, LWA_COLORKEY | LWA_ALPHA);
        }
        LOG_INFO("Click-through enabled");
    } else {
        if (exStyle & WS_EX_LAYERED) {
            SetLayeredWindowAttributes(hwnd, 0, alphaValue, LWA_ALPHA);
        }
        LOG_INFO("Click-through disabled");
    }

    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

BOOL InitDWMFunctions(void) {
    HMODULE hDwmapi = LoadLibraryW(DWMAPI_DLL);
    if (hDwmapi) {
        _DwmEnableBlurBehindWindow = (pfnDwmEnableBlurBehindWindow)GetProcAddress(hDwmapi, "DwmEnableBlurBehindWindow");
        if (_DwmEnableBlurBehindWindow) {
            LOG_INFO("DWM blur functions loaded successfully");
            return TRUE;
        }
    }
    LOG_WARNING("Failed to load DWM blur functions");
    return FALSE;
}

static void ApplyAccentPolicy(HWND hwnd, ACCENT_STATE accentState) {
    ACCENT_POLICY policy = {0};
    policy.AccentState = accentState;
    policy.AccentFlags = 0;
    policy.GradientColor = (accentState == ACCENT_ENABLE_BLURBEHIND) ? 
                          ((BLUR_ALPHA_VALUE << 24) | BLUR_GRADIENT_COLOR) : 0;
    
    WINDOWCOMPOSITIONATTRIBDATA data = {0};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &policy;
    data.cbData = sizeof(policy);
    
    if (SetWindowCompositionAttribute) {
        SetWindowCompositionAttribute(hwnd, &data);
    } else if (_DwmEnableBlurBehindWindow) {
        DWM_BLURBEHIND bb = {0};
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = (accentState != ACCENT_DISABLED);
        bb.hRgnBlur = NULL;
        _DwmEnableBlurBehindWindow(hwnd, &bb);
    }
}

void SetBlurBehind(HWND hwnd, BOOL enable) {
    ApplyAccentPolicy(hwnd, enable ? ACCENT_ENABLE_BLURBEHIND : ACCENT_DISABLED);
    LOG_INFO("Blur effect %s", enable ? "enabled" : "disabled");
}

