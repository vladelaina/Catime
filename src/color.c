/**
 * @file color.c
 * @brief Color processing functionality implementation
 * 
 * This file implements the application's color configuration management, including reading and saving color options,
 * color format conversion, and color preview functionality. Supports various color formats such as HEX, RGB, and color names.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/color.h"
#include "../include/language.h"
#include "../resource/resource.h"
#include "../include/dialog_procedure.h"

/// @name Global variable definitions
/// @{
PredefinedColor* COLOR_OPTIONS = NULL;    ///< Predefined color options array
size_t COLOR_OPTIONS_COUNT = 0;           ///< Number of predefined color options
char PREVIEW_COLOR[10] = "";              ///< Preview color value
BOOL IS_COLOR_PREVIEWING = FALSE;         ///< Whether color is being previewed
char CLOCK_TEXT_COLOR[10] = "#FFFFFF";    ///< Current text color value
/// @}

/// @name Function prototype declarations
/// @{
void GetConfigPath(char* path, size_t size);
void CreateDefaultConfig(const char* config_path);
void ReadConfig(void);
void WriteConfig(const char* config_path);
void replaceBlackColor(const char* color, char* output, size_t output_size);
/// @}

/**
 * @brief CSS standard color definitions
 * 
 * Contains CSS standard defined color names and their corresponding hexadecimal values
 */
static const CSSColor CSS_COLORS[] = {
    {"white", "#FFFFFF"},
    {"black", "#000000"},
    {"red", "#FF0000"},
    {"lime", "#00FF00"},
    {"blue", "#0000FF"},
    {"yellow", "#FFFF00"},
    {"cyan", "#00FFFF"},
    {"magenta", "#FF00FF"},
    {"silver", "#C0C0C0"},
    {"gray", "#808080"},
    {"maroon", "#800000"},
    {"olive", "#808000"},
    {"green", "#008000"},
    {"purple", "#800080"},
    {"teal", "#008080"},
    {"navy", "#000080"},
    {"orange", "#FFA500"},
    {"pink", "#FFC0CB"},
    {"brown", "#A52A2A"},
    {"violet", "#EE82EE"},
    {"indigo", "#4B0082"},
    {"gold", "#FFD700"},
    {"coral", "#FF7F50"},
    {"salmon", "#FA8072"},
    {"khaki", "#F0E68C"},
    {"plum", "#DDA0DD"},
    {"azure", "#F0FFFF"},
    {"ivory", "#FFFFF0"},
    {"wheat", "#F5DEB3"},
    {"snow", "#FFFAFA"}
};

#define CSS_COLORS_COUNT (sizeof(CSS_COLORS) / sizeof(CSS_COLORS[0]))

/**
 * @brief Default color options
 * 
 * Default color list used when color options are not specified in the configuration file
 */
static const char* DEFAULT_COLOR_OPTIONS[] = {
    "#FFFFFF",
    "#F9DB91",
    "#F4CAE0",
    "#FFB6C1",
    "#A8E7DF",
    "#A3CFB3",
    "#92CBFC",
    "#BDA5E7",
    "#9370DB",
    "#8C92CF",
    "#72A9A5",
    "#EB99A7",
    "#EB96BD",
    "#FFAE8B",
    "#FF7F50",
    "#CA6174"
};

#define DEFAULT_COLOR_OPTIONS_COUNT (sizeof(DEFAULT_COLOR_OPTIONS) / sizeof(DEFAULT_COLOR_OPTIONS[0]))

/// Original window procedure for the subclassed edit control
WNDPROC g_OldEditProc;

/**
 * @brief Initialize default language and color settings
 * 
 * Reads color settings from the configuration file, creates default config if not found.
 * Parses color options and stores them in the global color options array.
 */
#include <commdlg.h>

COLORREF ShowColorDialog(HWND hwnd) {
    CHOOSECOLOR cc = {0};
    static COLORREF acrCustClr[16] = {0};
    static DWORD rgbCurrent;
    
    int r, g, b;
    if (CLOCK_TEXT_COLOR[0] == '#') {
        sscanf(CLOCK_TEXT_COLOR + 1, "%02x%02x%02x", &r, &g, &b);
    } else {
        sscanf(CLOCK_TEXT_COLOR, "%d,%d,%d", &r, &g, &b);
    }
    rgbCurrent = RGB(r, g, b);
    
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT && i < 16; i++) {
        const char* hexColor = COLOR_OPTIONS[i].hexColor;
        if (hexColor[0] == '#') {
            sscanf(hexColor + 1, "%02x%02x%02x", &r, &g, &b);
            acrCustClr[i] = RGB(r, g, b);
        }
    }
    
    cc.lStructSize = sizeof(CHOOSECOLOR);
    cc.hwndOwner = hwnd;
    cc.lpCustColors = acrCustClr;
    cc.rgbResult = rgbCurrent;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ENABLEHOOK;
    cc.lpfnHook = ColorDialogHookProc;

    if (ChooseColor(&cc)) {
        COLORREF finalColor;
        if (IS_COLOR_PREVIEWING && PREVIEW_COLOR[0] == '#') {
            int r, g, b;
            sscanf(PREVIEW_COLOR + 1, "%02x%02x%02x", &r, &g, &b);
            finalColor = RGB(r, g, b);
        } else {
            finalColor = cc.rgbResult;
        }
        
        char tempColor[10];
        snprintf(tempColor, sizeof(tempColor), "#%02X%02X%02X",
                GetRValue(finalColor),
                GetGValue(finalColor),
                GetBValue(finalColor));
        
        // 替换纯黑色为近似黑色
        char finalColorStr[10];
        replaceBlackColor(tempColor, finalColorStr, sizeof(finalColorStr));
        
        strncpy(CLOCK_TEXT_COLOR, finalColorStr, sizeof(CLOCK_TEXT_COLOR) - 1);
        CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
        
        WriteConfigColor(CLOCK_TEXT_COLOR);
        
        IS_COLOR_PREVIEWING = FALSE;
        
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
        return finalColor;
    }

    IS_COLOR_PREVIEWING = FALSE;
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
    return (COLORREF)-1;
}

UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hwndParent;
    static CHOOSECOLOR* pcc;
    static BOOL isColorLocked = FALSE;
    static DWORD rgbCurrent;
    static COLORREF lastCustomColors[16] = {0};

    switch (msg) {
        case WM_INITDIALOG:
            pcc = (CHOOSECOLOR*)lParam;
            hwndParent = pcc->hwndOwner;
            rgbCurrent = pcc->rgbResult;
            isColorLocked = FALSE;
            
            for (int i = 0; i < 16; i++) {
                lastCustomColors[i] = pcc->lpCustColors[i];
            }
            return TRUE;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            isColorLocked = !isColorLocked;
            
            if (!isColorLocked) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hdlg, &pt);
                
                HDC hdc = GetDC(hdlg);
                COLORREF color = GetPixel(hdc, pt.x, pt.y);
                ReleaseDC(hdlg, hdc);
                
                if (color != CLR_INVALID && color != RGB(240, 240, 240)) {
                    if (pcc) {
                        pcc->rgbResult = color;
                    }
                    
                    char colorStr[20];
                    sprintf(colorStr, "#%02X%02X%02X",
                            GetRValue(color),
                            GetGValue(color),
                            GetBValue(color));
                    
                    // 替换纯黑色为近似黑色
                    char finalColorStr[20];
                    replaceBlackColor(colorStr, finalColorStr, sizeof(finalColorStr));
                    
                    strncpy(PREVIEW_COLOR, finalColorStr, sizeof(PREVIEW_COLOR) - 1);
                    PREVIEW_COLOR[sizeof(PREVIEW_COLOR) - 1] = '\0';
                    IS_COLOR_PREVIEWING = TRUE;
                    
                    InvalidateRect(hwndParent, NULL, TRUE);
                    UpdateWindow(hwndParent);
                }
            }
            break;

        case WM_MOUSEMOVE:
            if (!isColorLocked) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hdlg, &pt);
                
                HDC hdc = GetDC(hdlg);
                COLORREF color = GetPixel(hdc, pt.x, pt.y);
                ReleaseDC(hdlg, hdc);
                
                if (color != CLR_INVALID && color != RGB(240, 240, 240)) {
                    if (pcc) {
                        pcc->rgbResult = color;
                    }
                    
                    char colorStr[20];
                    sprintf(colorStr, "#%02X%02X%02X",
                            GetRValue(color),
                            GetGValue(color),
                            GetBValue(color));
                    
                    // 替换纯黑色为近似黑色
                    char finalColorStr[20];
                    replaceBlackColor(colorStr, finalColorStr, sizeof(finalColorStr));
                    
                    strncpy(PREVIEW_COLOR, finalColorStr, sizeof(PREVIEW_COLOR) - 1);
                    PREVIEW_COLOR[sizeof(PREVIEW_COLOR) - 1] = '\0';
                    IS_COLOR_PREVIEWING = TRUE;
                    
                    InvalidateRect(hwndParent, NULL, TRUE);
                    UpdateWindow(hwndParent);
                }
            }
            break;

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch (LOWORD(wParam)) {
                    case IDOK: {
                        if (IS_COLOR_PREVIEWING && PREVIEW_COLOR[0] == '#') {
                        } else {
                            snprintf(PREVIEW_COLOR, sizeof(PREVIEW_COLOR), "#%02X%02X%02X",
                                    GetRValue(pcc->rgbResult),
                                    GetGValue(pcc->rgbResult),
                                    GetBValue(pcc->rgbResult));
                        }
                        break;
                    }
                    
                    case IDCANCEL:
                        IS_COLOR_PREVIEWING = FALSE;
                        InvalidateRect(hwndParent, NULL, TRUE);
                        UpdateWindow(hwndParent);
                        break;
                }
            }
            break;

        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
            if (pcc) {
                BOOL colorsChanged = FALSE;
                for (int i = 0; i < 16; i++) {
                    if (lastCustomColors[i] != pcc->lpCustColors[i]) {
                        colorsChanged = TRUE;
                        lastCustomColors[i] = pcc->lpCustColors[i];
                        
                        char colorStr[20];
                        snprintf(colorStr, sizeof(colorStr), "#%02X%02X%02X",
                            GetRValue(pcc->lpCustColors[i]),
                            GetGValue(pcc->lpCustColors[i]),
                            GetBValue(pcc->lpCustColors[i]));
                        
                    }
                }
                
                if (colorsChanged) {
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    
                    ClearColorOptions();
                    
                    for (int i = 0; i < 16; i++) {
                        if (pcc->lpCustColors[i] != 0) {
                            char hexColor[10];
                            snprintf(hexColor, sizeof(hexColor), "#%02X%02X%02X",
                                GetRValue(pcc->lpCustColors[i]),
                                GetGValue(pcc->lpCustColors[i]),
                                GetBValue(pcc->lpCustColors[i]));
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

void InitializeDefaultLanguage(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    ClearColorOptions();
    
    // Try to open the configuration file, create default config if it doesn't exist
    FILE *file = fopen(config_path, "r");
    if (!file) {
        CreateDefaultConfig(config_path);
        file = fopen(config_path, "r");
    }
    
    if (file) {
        char line[1024];
        BOOL found_colors = FALSE;
        
        // Read color options from the configuration file
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "COLOR_OPTIONS=", 13) == 0) {
                ClearColorOptions();
                
                char* colors = line + 13;
                while (*colors == '=' || *colors == ' ') {
                    colors++;
                }
                
                char* newline = strchr(colors, '\n');
                if (newline) *newline = '\0';
                
                // Split and process each color option
                char* token = strtok(colors, ",");
                while (token) {
                    while (*token == ' ') token++;
                    char* end = token + strlen(token) - 1;
                    while (end > token && *end == ' ') {
                        *end = '\0';
                        end--;
                    }
                    
                    if (*token) {
                        if (token[0] != '#') {
                            char colorWithHash[10];
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
        
        // Use default options if color options are not found or empty
        if (!found_colors || COLOR_OPTIONS_COUNT == 0) {
            for (size_t i = 0; i < DEFAULT_COLOR_OPTIONS_COUNT; i++) {
                AddColorOption(DEFAULT_COLOR_OPTIONS[i]);
            }
        }
    }
}

/**
 * @brief Add color option
 * @param hexColor Hexadecimal color value
 * 
 * Adds a color option to the global color options array.
 * Standardizes the color format and checks for duplicates.
 */
void AddColorOption(const char* hexColor) {
    if (!hexColor || !*hexColor) {
        return;
    }
    
    // Standardize color format
    char normalizedColor[10];
    const char* hex = (hexColor[0] == '#') ? hexColor + 1 : hexColor;
    
    size_t len = strlen(hex);
    if (len != 6) {
        return;
    }
    
    // Check if it's a valid hexadecimal value
    for (int i = 0; i < 6; i++) {
        if (!isxdigit((unsigned char)hex[i])) {
            return;
        }
    }
    
    unsigned int color;
    if (sscanf(hex, "%x", &color) != 1) {
        return;
    }
    
    // Format to standard hexadecimal color format
    snprintf(normalizedColor, sizeof(normalizedColor), "#%06X", color);
    
    // Check if the same color already exists
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (strcasecmp(normalizedColor, COLOR_OPTIONS[i].hexColor) == 0) {
            return;
        }
    }
    
    // Add new color option
    PredefinedColor* newArray = realloc(COLOR_OPTIONS, 
                                      (COLOR_OPTIONS_COUNT + 1) * sizeof(PredefinedColor));
    if (newArray) {
        COLOR_OPTIONS = newArray;
        COLOR_OPTIONS[COLOR_OPTIONS_COUNT].hexColor = _strdup(normalizedColor);
        COLOR_OPTIONS_COUNT++;
    }
}

/**
 * @brief Clear all color options
 * 
 * Frees memory used by all color options and resets the color option count.
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
 * @brief Write color to configuration file
 * @param color_input Color value to write
 * 
 * Updates the text color setting in the configuration file and reloads the configuration.
 */
void WriteConfigColor(const char* color_input) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // Open configuration file for reading
    FILE *file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "Failed to open config file for reading: %s\n", config_path);
        return;
    }

    // Read the entire configuration file content
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *config_content = (char *)malloc(file_size + 1);
    if (!config_content) {
        fprintf(stderr, "Memory allocation failed!\n");
        fclose(file);
        return;
    }
    fread(config_content, sizeof(char), file_size, file);
    config_content[file_size] = '\0';
    fclose(file);

    // Prepare new configuration content
    char *new_config = (char *)malloc(file_size + 100);
    if (!new_config) {
        fprintf(stderr, "Memory allocation failed!\n");
        free(config_content);
        return;
    }
    new_config[0] = '\0';

    // Process line by line, update the color setting line
    char *line = strtok(config_content, "\n");
    while (line) {
        if (strncmp(line, "CLOCK_TEXT_COLOR=", 17) == 0) {
            strcat(new_config, "CLOCK_TEXT_COLOR=");
            strcat(new_config, color_input);
            strcat(new_config, "\n");
        } else {
            strcat(new_config, line);
            strcat(new_config, "\n");
        }
        line = strtok(NULL, "\n");
    }

    free(config_content);

    // Write the new configuration content
    file = fopen(config_path, "w");
    if (!file) {
        fprintf(stderr, "Failed to open config file for writing: %s\n", config_path);
        free(new_config);
        return;
    }
    fwrite(new_config, sizeof(char), strlen(new_config), file);
    fclose(file);

    free(new_config);

    // Reload configuration
    ReadConfig();
}

/**
 * @brief Normalize color format
 * @param input Input color string
 * @param output Output buffer
 * @param output_size Output buffer size
 * 
 * Converts various color formats (color names, RGB, hex, etc.) to standard hexadecimal format.
 * Supports conversion of CSS color names, RGB format, and short hexadecimal format.
 */
void normalizeColor(const char* input, char* output, size_t output_size) {
    // Skip leading spaces
    while (isspace(*input)) input++;
    
    // Convert to lowercase for color name matching
    char color[32];
    strncpy(color, input, sizeof(color)-1);
    color[sizeof(color)-1] = '\0';
    for (char* p = color; *p; p++) {
        *p = tolower(*p);
    }
    
    // Check if it's a CSS color name
    for (size_t i = 0; i < CSS_COLORS_COUNT; i++) {
        if (strcmp(color, CSS_COLORS[i].name) == 0) {
            strncpy(output, CSS_COLORS[i].hex, output_size);
            return;
        }
    }
    
    // Clean input string, remove spaces and special characters
    char cleaned[32] = {0};
    int j = 0;
    for (int i = 0; color[i]; i++) {
        if (!isspace(color[i]) && color[i] != ',' && color[i] != '(' && color[i] != ')') {
            cleaned[j++] = color[i];
        }
    }
    cleaned[j] = '\0';
    
    // Remove possible # prefix
    if (cleaned[0] == '#') {
        memmove(cleaned, cleaned + 1, strlen(cleaned));
    }
    
    // Handle short hexadecimal format (#RGB)
    if (strlen(cleaned) == 3) {
        snprintf(output, output_size, "#%c%c%c%c%c%c",
            cleaned[0], cleaned[0], cleaned[1], cleaned[1], cleaned[2], cleaned[2]);
        return;
    }
    
    // Handle standard hexadecimal format (#RRGGBB)
    if (strlen(cleaned) == 6 && strspn(cleaned, "0123456789abcdefABCDEF") == 6) {
        snprintf(output, output_size, "#%s", cleaned);
        return;
    }
    
    // Try to parse RGB format
    int r = -1, g = -1, b = -1;
    char* rgb_str = color;
    
    // Skip "rgb" prefix
    if (strncmp(rgb_str, "rgb", 3) == 0) {
        rgb_str += 3;
        while (*rgb_str && (*rgb_str == '(' || isspace(*rgb_str))) rgb_str++;
    }
    
    // Try various delimiter formats
    if (sscanf(rgb_str, "%d,%d,%d", &r, &g, &b) == 3 ||
        sscanf(rgb_str, "%d，%d，%d", &r, &g, &b) == 3 ||
        sscanf(rgb_str, "%d;%d;%d", &r, &g, &b) == 3 ||
        sscanf(rgb_str, "%d；%d；%d", &r, &g, &b) == 3 ||
        sscanf(rgb_str, "%d %d %d", &r, &g, &b) == 3 ||
        sscanf(rgb_str, "%d|%d|%d", &r, &g, &b) == 3) {
        
        if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
            snprintf(output, output_size, "#%02X%02X%02X", r, g, b);
            return;
        }
    }
    
    // If unable to parse, return original input
    strncpy(output, input, output_size);
}

/**
 * @brief Check if color is valid
 * @param input Color string to check
 * @return BOOL Returns TRUE if color is valid, FALSE otherwise
 * 
 * Validates whether the given color representation can be parsed as a valid color.
 * Checks if it's a valid hexadecimal color format after normalization.
 */
BOOL isValidColor(const char* input) {
    if (!input || !*input) return FALSE;
    
    char normalized[32];
    normalizeColor(input, normalized, sizeof(normalized));
    
    if (normalized[0] != '#' || strlen(normalized) != 7) {
        return FALSE;
    }
    
    // Check hexadecimal characters
    for (int i = 1; i < 7; i++) {
        if (!isxdigit((unsigned char)normalized[i])) {
            return FALSE;
        }
    }
    
    // Check RGB value range
    int r, g, b;
    if (sscanf(normalized + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
        return (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255);
    }
    
    return FALSE;
}

/**
 * @brief Color edit box subclass procedure
 * @param hwnd Window handle
 * @param msg Message ID
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return LRESULT Message processing result
 * 
 * Handles keyboard input and clipboard operations for the color edit box, updating color preview in real-time.
 * Implements Ctrl+A for select all and Enter for confirmation shortcuts.
 */
LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                SendMessage(hwnd, EM_SETSEL, 0, -1);
                // Return 0 to prevent message from continuing to pass, disable beep
                return 0;
            }
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
            LRESULT result = CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
            
            // Get current input color text
            char color[32];
            GetWindowTextA(hwnd, color, sizeof(color));
            
            // Normalize color and update preview
            char normalized[32];
            normalizeColor(color, normalized, sizeof(normalized));
            
            if (normalized[0] == '#') {
                // Replace pure black with near-black
                char finalColor[32];
                replaceBlackColor(normalized, finalColor, sizeof(finalColor));
                
                strncpy(PREVIEW_COLOR, finalColor, sizeof(PREVIEW_COLOR)-1);
                PREVIEW_COLOR[sizeof(PREVIEW_COLOR)-1] = '\0';
                IS_COLOR_PREVIEWING = TRUE;
                
                HWND hwndMain = GetParent(GetParent(hwnd));
                InvalidateRect(hwndMain, NULL, TRUE);
                UpdateWindow(hwndMain);
            } else {
                IS_COLOR_PREVIEWING = FALSE;
                HWND hwndMain = GetParent(GetParent(hwnd));
                InvalidateRect(hwndMain, NULL, TRUE);
                UpdateWindow(hwndMain);
            }
            
            return result;

        case WM_PASTE:
        case WM_CUT: {
            LRESULT result = CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
            
            // Handle paste or cut operation text
            char color[32];
            GetWindowTextA(hwnd, color, sizeof(color));
            
            char normalized[32];
            normalizeColor(color, normalized, sizeof(normalized));
            
            if (normalized[0] == '#') {
                // Replace pure black with near-black
                char finalColor[32];
                replaceBlackColor(normalized, finalColor, sizeof(finalColor));
                
                strncpy(PREVIEW_COLOR, finalColor, sizeof(PREVIEW_COLOR)-1);
                PREVIEW_COLOR[sizeof(PREVIEW_COLOR)-1] = '\0';
                IS_COLOR_PREVIEWING = TRUE;
            } else {
                IS_COLOR_PREVIEWING = FALSE;
            }
            
            HWND hwndMain = GetParent(GetParent(hwnd));
            InvalidateRect(hwndMain, NULL, TRUE);
            UpdateWindow(hwndMain);
            
            return result;
        }
    }
    
    return CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
}

/**
 * @brief Color settings dialog procedure
 * @param hwndDlg Dialog window handle
 * @param msg Message ID
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return INT_PTR Message processing result
 * 
 * Handles color settings dialog initialization, user input validation, and confirmation.
 * Implements color input validation and real-time preview functionality.
 */
INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Use dialog resource static text
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            if (hwndEdit) {
                g_OldEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, 
                                                         (LONG_PTR)ColorEditSubclassProc);
                
                // Set current color to default text
                if (CLOCK_TEXT_COLOR[0] != '\0') {
                    SetWindowTextA(hwndEdit, CLOCK_TEXT_COLOR);
                }
            }
            return TRUE;
        }
        
        case WM_COMMAND: {
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                // Get edit box color text
                char color[32];
                GetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, color, sizeof(color));
                
                // Check for empty input
                BOOL isAllSpaces = TRUE;
                for (int i = 0; color[i]; i++) {
                    if (!isspace((unsigned char)color[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                if (color[0] == '\0' || isAllSpaces) {
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
                }
                
                // Validate and apply color
                if (isValidColor(color)) {
                    char normalized_color[10];
                    normalizeColor(color, normalized_color, sizeof(normalized_color));
                    strncpy(CLOCK_TEXT_COLOR, normalized_color, sizeof(CLOCK_TEXT_COLOR)-1);
                    CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR)-1] = '\0';
                    
                    // Write to config and close dialog
                    WriteConfigColor(CLOCK_TEXT_COLOR);
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                } else {
                    // Use unified error dialog
                    ShowErrorDialog(hwndDlg);
                    SetWindowTextA(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT), "");
                    SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
                    return TRUE;
                }
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}

/**
 * @brief Replace pure black color with near-black
 * @param color Original color string
 * @param output Output buffer
 * @param output_size Output buffer size
 * 
 * If the input color is pure black (#000000), replaces it with #000001
 */
void replaceBlackColor(const char* color, char* output, size_t output_size) {
    if (color && (strcasecmp(color, "#000000") == 0)) {
        strncpy(output, "#000001", output_size);
        output[output_size - 1] = '\0';
    } else {
        strncpy(output, color, output_size);
        output[output_size - 1] = '\0';
    }
}