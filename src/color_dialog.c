/**
 * @file color_dialog.c
 * @brief Windows color picker with live preview and eyedropper
 */
#include <stdio.h>
#include <windows.h>
#include <commdlg.h>
#include "../include/color_dialog.h"
#include "../include/color_parser.h"
#include "../include/color_state.h"
#include "../include/menu_preview.h"
#include "../include/config.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_CUSTOM_COLORS 16
#define DIALOG_BG_COLOR RGB(240, 240, 240)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline void RefreshWindow(HWND hwnd) {
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

/**
 * @brief Sample color at cursor position
 * @param hdlg Dialog handle
 * @param outColor Output color
 * @return TRUE if valid color sampled
 * 
 * @details Filters out dialog background to avoid sampling gray
 */
static BOOL SampleColorAtCursor(HWND hdlg, COLORREF* outColor) {
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hdlg, &pt);
    
    HDC hdc = GetDC(hdlg);
    COLORREF color = GetPixel(hdc, pt.x, pt.y);
    ReleaseDC(hdlg, hdc);
    
    if (color != CLR_INVALID && color != DIALOG_BG_COLOR) {
        *outColor = color;
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Apply color preview to parent window
 * @param hwndParent Parent window handle
 * @param color Color to preview
 */
static void ApplyColorPreview(HWND hwndParent, COLORREF color) {
    char colorStr[COLOR_HEX_BUFFER];
    ColorRefToHex(color, colorStr, sizeof(colorStr));
    
    char finalColor[COLOR_HEX_BUFFER];
    ReplaceBlackColor(colorStr, finalColor, sizeof(finalColor));
    
    StartPreview(PREVIEW_TYPE_COLOR, finalColor, hwndParent);
}

/**
 * @brief Populate custom colors from saved palette
 * @param acrCustClr Custom color array
 * @param maxColors Array size
 */
static void PopulateCustomColors(COLORREF* acrCustClr, size_t maxColors) {
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT && i < maxColors; i++) {
        const char* hexColor = COLOR_OPTIONS[i].hexColor;
        if (hexColor[0] == '#') {
            int r, g, b;
            if (sscanf(hexColor + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
                acrCustClr[i] = RGB(r, g, b);
            }
        }
    }
}

/**
 * @brief Save custom colors to palette
 * @param lpCustColors Custom colors array
 * 
 * @details Automatically saves non-zero colors to configuration
 */
static void SaveCustomColorsToPalette(COLORREF* lpCustColors) {
    if (!lpCustColors) return;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    ClearColorOptions();
    
    for (int i = 0; i < MAX_CUSTOM_COLORS; i++) {
        if (lpCustColors[i] != 0) {
            char hexColor[COLOR_HEX_BUFFER];
            ColorRefToHex(lpCustColors[i], hexColor, sizeof(hexColor));
            AddColorOption(hexColor);
        }
    }
    
    void WriteConfig(const char*);
    WriteConfig(config_path);
}

/* ============================================================================
 * Hook Procedure
 * ============================================================================ */

UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hwndParent = NULL;
    static CHOOSECOLOR* pcc = NULL;
    static BOOL isColorLocked = FALSE;
    static COLORREF lastCustomColors[MAX_CUSTOM_COLORS] = {0};

    switch (msg) {
        case WM_INITDIALOG:
            pcc = (CHOOSECOLOR*)lParam;
            if (pcc) {
                hwndParent = pcc->hwndOwner;
                for (int i = 0; i < MAX_CUSTOM_COLORS; i++) {
                    lastCustomColors[i] = pcc->lpCustColors[i];
                }
            }
            isColorLocked = FALSE;
            return TRUE;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            isColorLocked = !isColorLocked;
            if (!isColorLocked) {
                COLORREF color;
                if (SampleColorAtCursor(hdlg, &color)) {
                    if (pcc) pcc->rgbResult = color;
                    ApplyColorPreview(hwndParent, color);
                }
            }
            break;

        case WM_MOUSEMOVE:
            if (!isColorLocked) {
                COLORREF color;
                if (SampleColorAtCursor(hdlg, &color)) {
                    if (pcc) pcc->rgbResult = color;
                    ApplyColorPreview(hwndParent, color);
                }
            }
            break;

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch (LOWORD(wParam)) {
                    case IDOK:
                        /* Preview is already set, will be applied by caller */
                        break;
                    
                    case IDCANCEL:
                        CancelPreview(hwndParent);
                        break;
                }
            }
            break;

        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
            if (pcc) {
                BOOL colorsChanged = FALSE;
                for (int i = 0; i < MAX_CUSTOM_COLORS; i++) {
                    if (lastCustomColors[i] != pcc->lpCustColors[i]) {
                        colorsChanged = TRUE;
                        lastCustomColors[i] = pcc->lpCustColors[i];
                    }
                }
                
                if (colorsChanged) {
                    SaveCustomColorsToPalette(pcc->lpCustColors);
                }
            }
            break;
    }
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

COLORREF ShowColorDialog(HWND hwnd) {
    CHOOSECOLOR cc = {0};
    static COLORREF acrCustClr[MAX_CUSTOM_COLORS] = {0};
    
    int r, g, b;
    if (CLOCK_TEXT_COLOR[0] == '#') {
        sscanf(CLOCK_TEXT_COLOR + 1, "%02x%02x%02x", &r, &g, &b);
    } else {
        sscanf(CLOCK_TEXT_COLOR, "%d,%d,%d", &r, &g, &b);
    }
    
    cc.lStructSize = sizeof(CHOOSECOLOR);
    cc.hwndOwner = hwnd;
    cc.lpCustColors = (LPDWORD)acrCustClr;
    cc.rgbResult = RGB(r, g, b);
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ENABLEHOOK;
    cc.lpfnHook = (LPCCHOOKPROC)ColorDialogHookProc;
    
    PopulateCustomColors(acrCustClr, MAX_CUSTOM_COLORS);
    
    if (ChooseColor(&cc)) {
        /* Apply preview (saves to config automatically) */
        if (ApplyPreview(hwnd)) {
            /* Success - preview was applied */
            return cc.rgbResult;
        } else {
            /* Fallback: no preview was active, save directly */
            char tempColor[COLOR_HEX_BUFFER];
            ColorRefToHex(cc.rgbResult, tempColor, sizeof(tempColor));
            
            char finalColorStr[COLOR_HEX_BUFFER];
            ReplaceBlackColor(tempColor, finalColorStr, sizeof(finalColorStr));
            
            strncpy(CLOCK_TEXT_COLOR, finalColorStr, sizeof(CLOCK_TEXT_COLOR) - 1);
            CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
            
            WriteConfigColor(CLOCK_TEXT_COLOR);
            RefreshWindow(hwnd);
            return cc.rgbResult;
        }
    }
    
    /* User cancelled */
    CancelPreview(hwnd);
    return (COLORREF)-1;
}

