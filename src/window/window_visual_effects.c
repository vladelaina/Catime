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
typedef HRESULT (WINAPI *pfnDwmExtendFrameIntoClientArea)(HWND hWnd, const MARGINS* pMarInset);
static pfnDwmEnableBlurBehindWindow _DwmEnableBlurBehindWindow = NULL;
static pfnDwmExtendFrameIntoClientArea _DwmExtendFrameIntoClientArea = NULL;

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
        _DwmExtendFrameIntoClientArea = (pfnDwmExtendFrameIntoClientArea)GetProcAddress(hDwmapi, "DwmExtendFrameIntoClientArea");
        
        if (_DwmEnableBlurBehindWindow && _DwmExtendFrameIntoClientArea) {
            LOG_INFO("DWM functions loaded successfully");
            return TRUE;
        }
    }
    LOG_WARNING("Failed to load DWM functions");
    return FALSE;
}

static void SetGlassEffect(HWND hwnd, BOOL enable) {
    if (_DwmExtendFrameIntoClientArea) {
        MARGINS margins = {0};
        if (enable) {
            // Extend frame into entire client area (-1)
            margins.cxLeftWidth = -1;
            margins.cxRightWidth = -1;
            margins.cyTopHeight = -1;
            margins.cyBottomHeight = -1;
        } else {
            // Reset margins
            margins.cxLeftWidth = 0;
            margins.cxRightWidth = 0;
            margins.cyTopHeight = 0;
            margins.cyBottomHeight = 0;
        }
        _DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
}

void SetClickThrough(HWND hwnd, BOOL enable) {
    extern int CLOCK_WINDOW_OPACITY;
    // Allow text transparency to be controlled by the opacity setting
    BYTE alphaValue = (BYTE)((CLOCK_WINDOW_OPACITY * 255) / 100);

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~WS_EX_TRANSPARENT;

    if (enable) {
        exStyle |= WS_EX_TRANSPARENT;
        if (exStyle & WS_EX_LAYERED) {
            // Normal mode: Use ColorKey for click-through transparency
            // Disable Glass to avoid double-transparency issues
            SetGlassEffect(hwnd, FALSE);
            SetLayeredWindowAttributes(hwnd, COLOR_KEY_BLACK, alphaValue, LWA_COLORKEY | LWA_ALPHA);
        }
        LOG_INFO("Click-through enabled");
    } else {
        if (exStyle & WS_EX_LAYERED) {
            // Edit mode: Use Glass for clickable transparency
            // Remove ColorKey so pixels are clickable
            SetGlassEffect(hwnd, TRUE);
            SetLayeredWindowAttributes(hwnd, 0, alphaValue, LWA_ALPHA);
        }
        LOG_INFO("Click-through disabled");
    }

    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

static void ApplyAccentPolicy(HWND hwnd, ACCENT_STATE accentState) {
    extern int CLOCK_WINDOW_OPACITY;
    // Map 1-100 opacity to 0-255 alpha for the acrylic background
    // We cap it at 240 to ensure it never becomes fully opaque solid block which would look bad
    DWORD alpha = (DWORD)((CLOCK_WINDOW_OPACITY * 240) / 100);
    
    ACCENT_POLICY policy = {0};
    policy.AccentState = accentState;
    policy.AccentFlags = 0;
    policy.GradientColor = (accentState == ACCENT_ENABLE_BLURBEHIND) ? 
                          ((alpha << 24) | BLUR_GRADIENT_COLOR) : 0;
    
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

