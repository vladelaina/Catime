/**
 * @file color.c
 * @brief Color management with CSS colors, live preview, and custom color picker
 * @version 2.0 - Refactored for better maintainability and reduced code duplication
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/color.h"
#include "../include/language.h"
#include "../resource/resource.h"
#include "../include/dialog_procedure.h"
#include "../include/config.h"
#include <commdlg.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum number of custom colors in Windows color dialog */
#define MAX_CUSTOM_COLORS 16

/** @brief Buffer sizes for color strings */
#define COLOR_BUFFER_SIZE 32
#define COLOR_HEX_BUFFER 10

/** @brief Color string lengths */
#define HEX_COLOR_LENGTH 7      // "#RRGGBB"
#define HEX_SHORT_LENGTH 4      // "#RGB"
#define HEX_DIGITS_LENGTH 6

/** @brief Special color values */
#define DIALOG_BG_COLOR RGB(240, 240, 240)
#define NEAR_BLACK_COLOR "#000001"

/** @brief Hex digit validation string */
#define HEX_DIGITS "0123456789abcdefABCDEF"

/* ============================================================================
 * Global state
 * ============================================================================ */

/** @brief Dynamic array of user-defined color options */
PredefinedColor* COLOR_OPTIONS = NULL;
size_t COLOR_OPTIONS_COUNT = 0;

/** @brief Live preview color state */
char PREVIEW_COLOR[COLOR_HEX_BUFFER] = "";
BOOL IS_COLOR_PREVIEWING = FALSE;

/** @brief Current clock text color */
char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER] = "#FFFFFF";

/** @brief Subclass window procedure storage */
WNDPROC g_OldEditProc;

/* ============================================================================
 * Forward declarations
 * ============================================================================ */

void GetConfigPath(char* path, size_t size);
void CreateDefaultConfig(const char* config_path);
void ReadConfig(void);
void WriteConfig(const char* config_path);

/* ============================================================================
 * CSS color lookup table
 * ============================================================================ */

/** @brief CSS color name to hex mapping table */
static const CSSColor CSS_COLORS[] = {
    {"white", "#FFFFFF"},   {"black", "#000000"},   {"red", "#FF0000"},
    {"lime", "#00FF00"},    {"blue", "#0000FF"},    {"yellow", "#FFFF00"},
    {"cyan", "#00FFFF"},    {"magenta", "#FF00FF"}, {"silver", "#C0C0C0"},
    {"gray", "#808080"},    {"maroon", "#800000"},  {"olive", "#808000"},
    {"green", "#008000"},   {"purple", "#800080"},  {"teal", "#008080"},
    {"navy", "#000080"},    {"orange", "#FFA500"},  {"pink", "#FFC0CB"},
    {"brown", "#A52A2A"},   {"violet", "#EE82EE"},  {"indigo", "#4B0082"},
    {"gold", "#FFD700"},    {"coral", "#FF7F50"},   {"salmon", "#FA8072"},
    {"khaki", "#F0E68C"},   {"plum", "#DDA0DD"},    {"azure", "#F0FFFF"},
    {"ivory", "#FFFFF0"},   {"wheat", "#F5DEB3"},   {"snow", "#FFFAFA"}
};

#define CSS_COLORS_COUNT (sizeof(CSS_COLORS) / sizeof(CSS_COLORS[0]))

/** @brief Default color palette for initial configuration */
static const char* DEFAULT_COLOR_OPTIONS[] = {
    "#FFFFFF", "#F9DB91", "#F4CAE0", "#FFB6C1",
    "#A8E7DF", "#A3CFB3", "#92CBFC", "#BDA5E7",
    "#9370DB", "#8C92CF", "#72A9A5", "#EB99A7",
    "#EB96BD", "#FFAE8B", "#FF7F50", "#CA6174"
};

#define DEFAULT_COLOR_OPTIONS_COUNT (sizeof(DEFAULT_COLOR_OPTIONS) / sizeof(DEFAULT_COLOR_OPTIONS[0]))

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * @brief Check if string is empty or contains only whitespace
 */
static BOOL IsEmptyOrWhitespaceA(const char* str) {
    if (!str || str[0] == '\0') return TRUE;
    for (int i = 0; str[i]; i++) {
        if (!isspace((unsigned char)str[i])) return FALSE;
    }
    return TRUE;
}

/**
 * @brief Trim leading and trailing whitespace from string
 */
static void TrimString(char* str) {
    if (!str) return;
    
    // Trim leading
    char* start = str;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != str) memmove(str, start, strlen(start) + 1);
    
    // Trim trailing
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

/**
 * @brief Refresh window display
 */
static inline void RefreshWindow(HWND hwnd) {
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

/**
 * @brief Convert COLORREF to hex string
 */
static void ColorRefToHex(COLORREF color, char* output, size_t size) {
    snprintf(output, size, "#%02X%02X%02X",
             GetRValue(color), GetGValue(color), GetBValue(color));
}

/**
 * @brief Replace pure black with near-black to avoid visibility issues
 */
static void ReplaceBlackColor(const char* color, char* output, size_t output_size) {
    if (color && strcasecmp(color, "#000000") == 0) {
        strncpy(output, NEAR_BLACK_COLOR, output_size);
    } else {
        strncpy(output, color, output_size);
    }
    output[output_size - 1] = '\0';
}

/**
 * @brief Show error dialog and refocus to edit control
 */
static void ShowErrorAndRefocus(HWND hwndDlg, int editControlId) {
    ShowErrorDialog(hwndDlg);
    HWND hwndEdit = GetDlgItem(hwndDlg, editControlId);
    if (hwndEdit) {
        SetFocus(hwndEdit);
        SendMessage(hwndEdit, EM_SETSEL, 0, -1);
    }
}

/* ============================================================================
 * Color parsing functions
 * ============================================================================ */

/**
 * @brief Parse CSS color name to hex
 */
static BOOL ParseCSSColor(const char* name, char* output, size_t size) {
    for (size_t i = 0; i < CSS_COLORS_COUNT; i++) {
        if (strcmp(name, CSS_COLORS[i].name) == 0) {
            strncpy(output, CSS_COLORS[i].hex, size);
            output[size - 1] = '\0';
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief Parse hex color string to normalized format
 */
static BOOL ParseHexColor(const char* hex, char* output, size_t size) {
    const char* ptr = (hex[0] == '#') ? hex + 1 : hex;
    size_t len = strlen(ptr);
    
    // Handle 3-digit hex shorthand (#RGB -> #RRGGBB)
    if (len == 3 && strspn(ptr, HEX_DIGITS) == 3) {
        snprintf(output, size, "#%c%c%c%c%c%c",
                ptr[0], ptr[0], ptr[1], ptr[1], ptr[2], ptr[2]);
        return TRUE;
    }
    
    // Handle 6-digit hex
    if (len == HEX_DIGITS_LENGTH && strspn(ptr, HEX_DIGITS) == HEX_DIGITS_LENGTH) {
        snprintf(output, size, "#%s", ptr);
        return TRUE;
    }
    
    return FALSE;
}

/**
 * @brief Try parsing RGB values with given format
 */
static BOOL TryParseRGBWithSeparator(const char* str, const char* sep, int* r, int* g, int* b) {
    char format[32];
    snprintf(format, sizeof(format), "%%d%s%%d%s%%d", sep, sep);
    return (sscanf(str, format, r, g, b) == 3);
}

/**
 * @brief Parse RGB color string to hex
 */
static BOOL ParseRGBColor(const char* rgb_input, char* output, size_t size) {
    int r = -1, g = -1, b = -1;
    const char* rgb_str = rgb_input;
    
    // Skip "rgb(" prefix if present
    if (strncmp(rgb_str, "rgb", 3) == 0) {
        rgb_str += 3;
        while (*rgb_str && (*rgb_str == '(' || isspace(*rgb_str))) rgb_str++;
    }
    
    // Try various separators for international compatibility
    static const char* separators[] = {",", "，", ";", "；", " ", "|"};
    for (size_t i = 0; i < sizeof(separators) / sizeof(char*); i++) {
        if (TryParseRGBWithSeparator(rgb_str, separators[i], &r, &g, &b)) {
            if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                snprintf(output, size, "#%02X%02X%02X", r, g, b);
                return TRUE;
            }
        }
    }
    
    return FALSE;
}

/**
 * @brief Convert various color formats to normalized hex
 */
void normalizeColor(const char* input, char* output, size_t output_size) {
    if (!input || !output) return;
    
    // Skip leading whitespace
    while (isspace(*input)) input++;
    
    // Convert to lowercase for case-insensitive comparison
    char lower[COLOR_BUFFER_SIZE];
    strncpy(lower, input, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    for (char* p = lower; *p; p++) {
        *p = tolower(*p);
    }
    
    // Try CSS color name
    if (ParseCSSColor(lower, output, output_size)) return;
    
    // Clean non-essential characters for hex/RGB parsing
    char cleaned[COLOR_BUFFER_SIZE] = {0};
    int j = 0;
    for (int i = 0; lower[i] && j < (int)sizeof(cleaned) - 1; i++) {
        if (!isspace(lower[i]) && lower[i] != ',' && lower[i] != '(' && lower[i] != ')') {
            cleaned[j++] = lower[i];
        }
    }
    
    // Try hex color
    if (ParseHexColor(cleaned, output, output_size)) return;
    
    // Try RGB format
    if (ParseRGBColor(lower, output, output_size)) return;
    
    // Fallback: return input unchanged
    strncpy(output, input, output_size);
    output[output_size - 1] = '\0';
}

/**
 * @brief Validate color string format
 */
BOOL isValidColor(const char* input) {
    if (!input || !*input) return FALSE;
    
    char normalized[COLOR_BUFFER_SIZE];
    normalizeColor(input, normalized, sizeof(normalized));
    
    if (normalized[0] != '#' || strlen(normalized) != HEX_COLOR_LENGTH) {
        return FALSE;
    }
    
    for (int i = 1; i < HEX_COLOR_LENGTH; i++) {
        if (!isxdigit((unsigned char)normalized[i])) {
            return FALSE;
        }
    }
    
    return TRUE;
}

/* ============================================================================
 * Color dialog helper functions
 * ============================================================================ */

/**
 * @brief Sample color at cursor position in dialog
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
 * @brief Apply color preview to main window
 */
static void ApplyColorPreview(HWND hwndParent, COLORREF color) {
    char colorStr[COLOR_BUFFER_SIZE];
    ColorRefToHex(color, colorStr, sizeof(colorStr));
    
    char finalColor[COLOR_HEX_BUFFER];
    ReplaceBlackColor(colorStr, finalColor, sizeof(finalColor));
    
    strncpy(PREVIEW_COLOR, finalColor, sizeof(PREVIEW_COLOR) - 1);
    PREVIEW_COLOR[sizeof(PREVIEW_COLOR) - 1] = '\0';
    IS_COLOR_PREVIEWING = TRUE;
    
    RefreshWindow(hwndParent);
}

/**
 * @brief Populate custom colors array from COLOR_OPTIONS
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

/* ============================================================================
 * Color dialog procedures
 * ============================================================================ */

/**
 * @brief Color dialog hook procedure for live preview and mouse picking
 */
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
                        if (!IS_COLOR_PREVIEWING || PREVIEW_COLOR[0] != '#') {
                            ColorRefToHex(pcc->rgbResult, PREVIEW_COLOR, sizeof(PREVIEW_COLOR));
                        }
                        break;
                    
                    case IDCANCEL:
                        IS_COLOR_PREVIEWING = FALSE;
                        RefreshWindow(hwndParent);
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
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    
                    ClearColorOptions();
                    
                    for (int i = 0; i < MAX_CUSTOM_COLORS; i++) {
                        if (pcc->lpCustColors[i] != 0) {
                            char hexColor[COLOR_HEX_BUFFER];
                            ColorRefToHex(pcc->lpCustColors[i], hexColor, sizeof(hexColor));
                            AddColorOption(hexColor);
                        }
                    }
                    
                    WriteConfig(config_path);
                }
            }
            break;
    }
    return 0;
}

/**
 * @brief Show Windows color picker dialog with live preview support
 */
COLORREF ShowColorDialog(HWND hwnd) {
    CHOOSECOLOR cc = {0};
    static COLORREF acrCustClr[MAX_CUSTOM_COLORS] = {0};
    
    // Parse current color to RGB
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
        COLORREF finalColor;
        if (IS_COLOR_PREVIEWING && PREVIEW_COLOR[0] == '#') {
            sscanf(PREVIEW_COLOR + 1, "%02x%02x%02x", &r, &g, &b);
            finalColor = RGB(r, g, b);
        } else {
            finalColor = cc.rgbResult;
        }
        
        char tempColor[COLOR_HEX_BUFFER];
        ColorRefToHex(finalColor, tempColor, sizeof(tempColor));
        
        char finalColorStr[COLOR_HEX_BUFFER];
        ReplaceBlackColor(tempColor, finalColorStr, sizeof(finalColorStr));
        
        strncpy(CLOCK_TEXT_COLOR, finalColorStr, sizeof(CLOCK_TEXT_COLOR) - 1);
        CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
        
        WriteConfigColor(CLOCK_TEXT_COLOR);
        IS_COLOR_PREVIEWING = FALSE;
        
        RefreshWindow(hwnd);
        return finalColor;
    }
    
    IS_COLOR_PREVIEWING = FALSE;
    RefreshWindow(hwnd);
    return (COLORREF)-1;
}

/* ============================================================================
 * Edit control subclass for live color preview
 * ============================================================================ */

/**
 * @brief Update color preview from edit control text
 */
static void UpdateColorPreviewFromEdit(HWND hwndEdit) {
    char color[COLOR_BUFFER_SIZE];
    wchar_t wcolor[COLOR_BUFFER_SIZE];
    GetWindowTextW(hwndEdit, wcolor, sizeof(wcolor) / sizeof(wchar_t));
    WideCharToMultiByte(CP_UTF8, 0, wcolor, -1, color, sizeof(color), NULL, NULL);
    
    char normalized[COLOR_BUFFER_SIZE];
    normalizeColor(color, normalized, sizeof(normalized));
    
    HWND hwndMain = GetParent(GetParent(hwndEdit));
    
    if (normalized[0] == '#') {
        ReplaceBlackColor(normalized, PREVIEW_COLOR, sizeof(PREVIEW_COLOR));
        IS_COLOR_PREVIEWING = TRUE;
    } else {
        IS_COLOR_PREVIEWING = FALSE;
    }
    
    RefreshWindow(hwndMain);
}

/**
 * @brief Subclass procedure for color input edit control
 */
LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                SendMessage(hwnd, EM_SETSEL, 0, -1);
                return 0;
            }
            break;
        
        case WM_COMMAND:
            if (wParam == VK_RETURN) {
                HWND hwndDlg = GetParent(hwnd);
                if (hwndDlg) {
                    SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                    return 0;
                }
            }
            break;
        
        case WM_CHAR:
            if (GetKeyState(VK_CONTROL) < 0 && (wParam == 1 || wParam == 'a' || wParam == 'A')) {
                return 0;
            }
            {
                LRESULT result = CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
                UpdateColorPreviewFromEdit(hwnd);
                return result;
            }
        
        case WM_PASTE:
        case WM_CUT: {
            LRESULT result = CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
            UpdateColorPreviewFromEdit(hwnd);
            return result;
        }
    }
    
    return CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
}

/* ============================================================================
 * Color input dialog
 * ============================================================================ */

/**
 * @brief Dialog procedure for color input dialog
 */
INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            if (hwndEdit) {
                g_OldEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC,
                                                         (LONG_PTR)ColorEditSubclassProc);
                
                if (CLOCK_TEXT_COLOR[0] != '\0') {
                    wchar_t wcolor[COLOR_BUFFER_SIZE];
                    MultiByteToWideChar(CP_UTF8, 0, CLOCK_TEXT_COLOR, -1, 
                                      wcolor, sizeof(wcolor) / sizeof(wchar_t));
                    SetWindowTextW(hwndEdit, wcolor);
                }
            }
            
            MoveDialogToPrimaryScreen(hwndDlg);
            return TRUE;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                char color[COLOR_BUFFER_SIZE];
                wchar_t wcolor[COLOR_BUFFER_SIZE];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wcolor, 
                              sizeof(wcolor) / sizeof(wchar_t));
                WideCharToMultiByte(CP_UTF8, 0, wcolor, -1, color, sizeof(color), NULL, NULL);
                
                if (IsEmptyOrWhitespaceA(color)) {
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
                }
                
                if (isValidColor(color)) {
                    char normalized_color[COLOR_HEX_BUFFER];
                    normalizeColor(color, normalized_color, sizeof(normalized_color));
                    strncpy(CLOCK_TEXT_COLOR, normalized_color, sizeof(CLOCK_TEXT_COLOR) - 1);
                    CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
                    
                    WriteConfigColor(CLOCK_TEXT_COLOR);
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                } else {
                    ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

/* ============================================================================
 * Color options management
 * ============================================================================ */

/**
 * @brief Initialize color options from config file or defaults
 */
void InitializeDefaultLanguage(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    ClearColorOptions();

    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
    if (!file) {
        CreateDefaultConfig(config_path);
        file = _wfopen(wconfig_path, L"r");
    }

    if (file) {
        char line[1024];
        BOOL found_colors = FALSE;

        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "COLOR_OPTIONS=", 14) == 0) {
                ClearColorOptions();

                char* colors = line + 14;
                while (*colors == '=' || *colors == ' ') colors++;

                char* newline = strchr(colors, '\n');
                if (newline) *newline = '\0';

                char* token = strtok(colors, ",");
                while (token) {
                    TrimString(token);

                    if (*token) {
                        if (token[0] != '#') {
                            char colorWithHash[COLOR_HEX_BUFFER];
                            snprintf(colorWithHash, sizeof(colorWithHash), "#%s", token);
                            AddColorOption(colorWithHash);
                        } else {
                            AddColorOption(token);
                        }
                    }
                    token = strtok(NULL, ",");
                }
                found_colors = TRUE;
                break;
            }
        }
        fclose(file);

        if (!found_colors || COLOR_OPTIONS_COUNT == 0) {
            for (size_t i = 0; i < DEFAULT_COLOR_OPTIONS_COUNT; i++) {
                AddColorOption(DEFAULT_COLOR_OPTIONS[i]);
            }
        }
    }
}

/**
 * @brief Add color to user options list with validation and deduplication
 */
void AddColorOption(const char* hexColor) {
    if (!hexColor || !*hexColor) return;

    char normalizedColor[COLOR_HEX_BUFFER];
    const char* hex = (hexColor[0] == '#') ? hexColor + 1 : hexColor;

    // Validate hex format
    size_t len = strlen(hex);
    if (len != HEX_DIGITS_LENGTH) return;
    
    for (int i = 0; i < HEX_DIGITS_LENGTH; i++) {
        if (!isxdigit((unsigned char)hex[i])) return;
    }

    unsigned int color;
    if (sscanf(hex, "%x", &color) != 1) return;

    snprintf(normalizedColor, sizeof(normalizedColor), "#%06X", color);

    // Check for duplicates
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (strcasecmp(normalizedColor, COLOR_OPTIONS[i].hexColor) == 0) {
            return;
        }
    }

    // Expand array and add new color
    PredefinedColor* newArray = realloc(COLOR_OPTIONS,
                                      (COLOR_OPTIONS_COUNT + 1) * sizeof(PredefinedColor));
    if (newArray) {
        COLOR_OPTIONS = newArray;
        COLOR_OPTIONS[COLOR_OPTIONS_COUNT].hexColor = _strdup(normalizedColor);
        COLOR_OPTIONS_COUNT++;
    }
}

/**
 * @brief Free all color options memory
 */
void ClearColorOptions(void) {
    if (COLOR_OPTIONS) {
        for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
            free((void*)COLOR_OPTIONS[i].hexColor);
        }
        free(COLOR_OPTIONS);
        COLOR_OPTIONS = NULL;
        COLOR_OPTIONS_COUNT = 0;
    }
}

/**
 * @brief Update CLOCK_TEXT_COLOR in config file
 */
void WriteConfigColor(const char* color_input) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", color_input, config_path);
}