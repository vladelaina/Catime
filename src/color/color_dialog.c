/**
 * @file color_dialog.c
 * @brief Windows color picker with live preview and eyedropper
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <commdlg.h>
#include "color/color_dialog.h"
#include "color/color_parser.h"
#include "color/color_state.h"
#include "menu_preview.h"
#include "config.h"
#include "log.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_CUSTOM_COLORS 16
#define DIALOG_BG_COLOR RGB(240, 240, 240)
#define COLOR_OPTIONS_CONFIG_BUFFER 2048
#define COLOR_DIALOG_PARENT_PROP L"Catime.ColorDialog.Parent"
#define COLOR_DIALOG_CHOOSE_PROP L"Catime.ColorDialog.ChooseColor"
#define COLOR_DIALOG_LOCKED_PROP L"Catime.ColorDialog.Locked"

/* Track the actual number of colors loaded */
static size_t g_loadedColorCount = 0;

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
    if (!hdc) return FALSE;
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

static BOOL AppendColorOption(char* outValue, size_t outSize, const char* color, BOOL* firstColor) {
    if (!outValue || outSize == 0 || !color || !firstColor) return FALSE;

    size_t used = strlen(outValue);
    size_t colorLen = strlen(color);
    size_t sepLen = *firstColor ? 0 : 1;
    if (used + sepLen + colorLen >= outSize) {
        return FALSE;
    }

    if (!*firstColor) {
        outValue[used++] = ',';
        outValue[used] = '\0';
    }

    memcpy(outValue + used, color, colorLen + 1);
    *firstColor = FALSE;
    return TRUE;
}

static BOOL ColorOptionEquals(const char* entry, size_t entryLen, const char* color) {
    if (!entry || !color || entryLen != strlen(color)) return FALSE;

    for (size_t i = 0; i < entryLen; i++) {
        if (toupper((unsigned char)entry[i]) != toupper((unsigned char)color[i])) {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL ColorOptionsConfigContains(const char* value, const char* color) {
    if (!value || !color) return FALSE;

    const char* entry = value;
    while (*entry) {
        const char* end = strchr(entry, ',');
        size_t entryLen = end ? (size_t)(end - entry) : strlen(entry);
        if (ColorOptionEquals(entry, entryLen, color)) {
            return TRUE;
        }
        if (!end) break;
        entry = end + 1;
    }

    return FALSE;
}

static BOOL AppendUniqueColorOption(char* outValue, size_t outSize,
                                    const char* color, BOOL* firstColor) {
    if (ColorOptionsConfigContains(outValue, color)) {
        return TRUE;
    }

    return AppendColorOption(outValue, outSize, color, firstColor);
}

static BOOL BuildCustomColorsConfigValue(COLORREF* lpCustColors,
                                         char* outValue, size_t outSize) {
    if (!lpCustColors || !outValue || outSize == 0) return FALSE;

    outValue[0] = '\0';
    BOOL firstColor = TRUE;

    for (size_t i = 0; i < g_loadedColorCount && i < MAX_CUSTOM_COLORS; i++) {
        char hexColor[COLOR_HEX_BUFFER];
        ColorRefToHex(lpCustColors[i], hexColor, sizeof(hexColor));
        if (!AppendUniqueColorOption(outValue, outSize, hexColor, &firstColor)) {
            return FALSE;
        }
    }

    for (size_t i = g_loadedColorCount; i < MAX_CUSTOM_COLORS; i++) {
        if (lpCustColors[i] != RGB(255, 255, 255)) {
            char hexColor[COLOR_HEX_BUFFER];
            ColorRefToHex(lpCustColors[i], hexColor, sizeof(hexColor));
            if (!AppendUniqueColorOption(outValue, outSize, hexColor, &firstColor)) {
                return FALSE;
            }
        }
    }

    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        const char* color = COLOR_OPTIONS[i].hexColor;
        if (color && strchr(color, '_') &&
            !AppendUniqueColorOption(outValue, outSize, color, &firstColor)) {
            return FALSE;
        }
    }

    return TRUE;
}

static int HexDigitValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static BOOL TryParseColorRef(const char* input, COLORREF* outColor) {
    if (!input || !outColor) return FALSE;

    char normalized[COLOR_HEX_BUFFER];
    normalizeColor(input, normalized, sizeof(normalized));
    if (normalized[0] != '#' || strlen(normalized) != 7) {
        return FALSE;
    }

    int channels[3] = {0, 0, 0};
    for (int i = 0; i < 3; i++) {
        int hi = HexDigitValue(normalized[1 + i * 2]);
        int lo = HexDigitValue(normalized[2 + i * 2]);
        if (hi < 0 || lo < 0) return FALSE;
        channels[i] = hi * 16 + lo;
    }

    *outColor = RGB(channels[0], channels[1], channels[2]);
    return TRUE;
}

/**
 * @brief Populate custom colors from saved palette
 * @param acrCustClr Custom color array
 * @param maxColors Array size
 */
static void PopulateCustomColors(COLORREF* acrCustClr, size_t maxColors) {
    /* Fill all slots with white (empty appearance) */
    for (size_t i = 0; i < maxColors; i++) {
        acrCustClr[i] = RGB(255, 255, 255);
    }
    
    size_t custIdx = 0;
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT && custIdx < maxColors; i++) {
        const char* hexColor = COLOR_OPTIONS[i].hexColor;
        /* Skip gradient colors (contain underscore) */
        if (strchr(hexColor, '_') != NULL) continue;
        
        COLORREF parsedColor;
        if (TryParseColorRef(hexColor, &parsedColor)) {
            acrCustClr[custIdx++] = parsedColor;
        }
    }
    g_loadedColorCount = custIdx;
}

/**
 * @brief Save custom colors to palette
 * @param lpCustColors Custom colors array
 * 
 * @details Saves modified colors and new non-white colors to configuration.
 *          Preserves gradient colors from the original palette.
 */
static BOOL SaveCustomColorsToPalette(COLORREF* lpCustColors) {
    if (!lpCustColors) return FALSE;

    char newOptions[COLOR_OPTIONS_CONFIG_BUFFER];
    if (!BuildCustomColorsConfigValue(lpCustColors, newOptions, sizeof(newOptions))) {
        return FALSE;
    }

    return WriteConfigColorOptions(newOptions);
}

/* ============================================================================
 * Hook Procedure
 * ============================================================================ */

UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            CHOOSECOLOR* pcc = (CHOOSECOLOR*)lParam;
            if (pcc) {
                SetPropW(hdlg, COLOR_DIALOG_PARENT_PROP, (HANDLE)pcc->hwndOwner);
                SetPropW(hdlg, COLOR_DIALOG_CHOOSE_PROP, (HANDLE)pcc);
            }
            RemovePropW(hdlg, COLOR_DIALOG_LOCKED_PROP);
            return TRUE;
        }

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: {
            HWND hwndParent = (HWND)GetPropW(hdlg, COLOR_DIALOG_PARENT_PROP);
            CHOOSECOLOR* pcc = (CHOOSECOLOR*)GetPropW(hdlg, COLOR_DIALOG_CHOOSE_PROP);
            BOOL isColorLocked = GetPropW(hdlg, COLOR_DIALOG_LOCKED_PROP) == NULL;
            if (isColorLocked) {
                SetPropW(hdlg, COLOR_DIALOG_LOCKED_PROP, (HANDLE)1);
            } else {
                RemovePropW(hdlg, COLOR_DIALOG_LOCKED_PROP);
            }

            if (!isColorLocked) {
                COLORREF color;
                if (SampleColorAtCursor(hdlg, &color)) {
                    if (pcc) pcc->rgbResult = color;
                    ApplyColorPreview(hwndParent, color);
                }
            }
            break;
        }

        case WM_MOUSEMOVE: {
            HWND hwndParent = (HWND)GetPropW(hdlg, COLOR_DIALOG_PARENT_PROP);
            CHOOSECOLOR* pcc = (CHOOSECOLOR*)GetPropW(hdlg, COLOR_DIALOG_CHOOSE_PROP);
            BOOL isColorLocked = GetPropW(hdlg, COLOR_DIALOG_LOCKED_PROP) != NULL;
            if (!isColorLocked) {
                COLORREF color;
                if (SampleColorAtCursor(hdlg, &color)) {
                    if (pcc) pcc->rgbResult = color;
                    ApplyColorPreview(hwndParent, color);
                }
            }
            break;
        }

        case WM_COMMAND: {
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDCANCEL) {
                HWND hwndParent = (HWND)GetPropW(hdlg, COLOR_DIALOG_PARENT_PROP);
                CancelPreview(hwndParent);
            }
            break;
        }

        case WM_DESTROY:
            RemovePropW(hdlg, COLOR_DIALOG_PARENT_PROP);
            RemovePropW(hdlg, COLOR_DIALOG_CHOOSE_PROP);
            RemovePropW(hdlg, COLOR_DIALOG_LOCKED_PROP);
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
    
    COLORREF initialColor = RGB(255, 255, 255);
    TryParseColorRef(CLOCK_TEXT_COLOR, &initialColor);
    
    cc.lStructSize = sizeof(CHOOSECOLOR);
    cc.hwndOwner = hwnd;
    cc.lpCustColors = (LPDWORD)acrCustClr;
    cc.rgbResult = initialColor;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ENABLEHOOK;
    cc.lpfnHook = (LPCCHOOKPROC)ColorDialogHookProc;
    
    PopulateCustomColors(acrCustClr, MAX_CUSTOM_COLORS);
    
    if (ChooseColor(&cc)) {
        if (!SaveCustomColorsToPalette(acrCustClr)) {
            LOG_WARNING("ColorDialog: failed to persist custom color palette");
        }
        
        if (IsPreviewActive()) {
            if (ApplyPreview(hwnd)) {
                return cc.rgbResult;
            }
            CancelPreview(hwnd);
            return (COLORREF)-1;
        }

        char tempColor[COLOR_HEX_BUFFER];
        ColorRefToHex(cc.rgbResult, tempColor, sizeof(tempColor));
        
        char finalColorStr[COLOR_HEX_BUFFER];
        ReplaceBlackColor(tempColor, finalColorStr, sizeof(finalColorStr));

        if (!WriteConfigColor(finalColorStr)) {
            CancelPreview(hwnd);
            return (COLORREF)-1;
        }

        RefreshWindow(hwnd);
        return cc.rgbResult;
    }
    
    /* User cancelled */
    CancelPreview(hwnd);
    return (COLORREF)-1;
}
