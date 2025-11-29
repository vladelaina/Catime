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

/* ============================================================================
 * Implementation
 * ============================================================================ */

BOOL InitDWMFunctions(void) {
    HMODULE hDwmapi = LoadLibraryW(DWMAPI_DLL);
    if (hDwmapi) {
        _DwmEnableBlurBehindWindow = (pfnDwmEnableBlurBehindWindow)GetProcAddress(hDwmapi, "DwmEnableBlurBehindWindow");
        
        if (_DwmEnableBlurBehindWindow) {
            LOG_INFO("DWM functions loaded successfully");
        }
    }
    
    /* Load SetWindowCompositionAttribute from user32.dll (undocumented API for acrylic blur) */
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        _SetWindowCompositionAttribute = (pfnSetWindowCompositionAttribute)GetProcAddress(hUser32, "SetWindowCompositionAttribute");
        if (_SetWindowCompositionAttribute) {
            LOG_INFO("SetWindowCompositionAttribute loaded successfully");
        }
    }
    
    if (_DwmEnableBlurBehindWindow || _SetWindowCompositionAttribute) {
        return TRUE;
    }
    
    LOG_WARNING("Failed to load DWM/composition functions");
    return FALSE;
}

/* Click-through state */
static BOOL g_clickThroughEnabled = FALSE;
static BOOL g_currentlyTransparent = FALSE;

/* Timer ID for mouse position checking */
#define TIMER_ID_CLICK_THROUGH 201
#define CLICK_THROUGH_CHECK_INTERVAL 50  /* ms */

BOOL IsSoftClickThroughEnabled(void) {
    return g_clickThroughEnabled;
}

/* Update WS_EX_TRANSPARENT based on mouse position over clickable regions */
void UpdateClickThroughState(HWND hwnd) {
    if (!g_clickThroughEnabled || !hwnd) return;
    
    extern BOOL CLOCK_EDIT_MODE;
    if (CLOCK_EDIT_MODE) return;
    
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
    extern void UpdateRegionPositions(int windowX, int windowY);
    extern const void* GetClickableRegionAt(POINT pt);
    
    UpdateRegionPositions(rcWindow.left, rcWindow.top);
    const void* region = GetClickableRegionAt(pt);
    
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    if (region) {
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

void SetClickThrough(HWND hwnd, BOOL enable) {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    if (enable) {
        /* Enable click-through with dynamic switching for clickable regions */
        exStyle |= WS_EX_TRANSPARENT;
        g_clickThroughEnabled = TRUE;
        g_currentlyTransparent = TRUE;
        
        /* Start timer to check mouse position */
        SetTimer(hwnd, TIMER_ID_CLICK_THROUGH, CLICK_THROUGH_CHECK_INTERVAL, NULL);
        LOG_INFO("Click-through enabled (dynamic mode)");
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
        g_clickThroughEnabled = FALSE;
        g_currentlyTransparent = FALSE;
        
        /* Stop timer */
        KillTimer(hwnd, TIMER_ID_CLICK_THROUGH);
        LOG_INFO("Click-through disabled");
    }

    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

/* Get timer ID for external handling */
UINT GetClickThroughTimerId(void) {
    return TIMER_ID_CLICK_THROUGH;
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
    
    if (_SetWindowCompositionAttribute) {
        _SetWindowCompositionAttribute(hwnd, &data);
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

