#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/color.h"
#include "../include/language.h"
#include "../resource/resource.h"

// 全局变量定义
PredefinedColor* COLOR_OPTIONS = NULL;
size_t COLOR_OPTIONS_COUNT = 0;
char PREVIEW_COLOR[10] = "";
BOOL IS_COLOR_PREVIEWING = FALSE;
char CLOCK_TEXT_COLOR[10] = "#FFFFFF";

// CSS颜色定义
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

// 默认颜色选项
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

WNDPROC g_OldEditProc;

void GetConfigPath(char* path, size_t size);
void CreateDefaultConfig(const char* config_path);
void ReadConfig(void);

// 函数实现
void InitializeDefaultLanguage(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    ClearColorOptions();
    
    FILE *file = fopen(config_path, "r");
    if (!file) {
        CreateDefaultConfig(config_path);
        file = fopen(config_path, "r");
    }
    
    if (file) {
        char line[1024];
        BOOL found_colors = FALSE;
        
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "COLOR_OPTIONS=", 13) == 0) {
                ClearColorOptions();
                
                char* colors = line + 13;
                while (*colors == '=' || *colors == ' ') {
                    colors++;
                }
                
                char* newline = strchr(colors, '\n');
                if (newline) *newline = '\0';
                
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
        
        if (!found_colors || COLOR_OPTIONS_COUNT == 0) {
            for (size_t i = 0; i < DEFAULT_COLOR_OPTIONS_COUNT; i++) {
                AddColorOption(DEFAULT_COLOR_OPTIONS[i]);
            }
        }
    }
}

void AddColorOption(const char* hexColor) {
    if (!hexColor || !*hexColor) {
        return;
    }
    
    char normalizedColor[10];
    const char* hex = (hexColor[0] == '#') ? hexColor + 1 : hexColor;
    
    size_t len = strlen(hex);
    if (len != 6) {
        return;
    }
    
    for (int i = 0; i < 6; i++) {
        if (!isxdigit((unsigned char)hex[i])) {
            return;
        }
    }
    
    unsigned int color;
    if (sscanf(hex, "%x", &color) != 1) {
        return;
    }
    
    snprintf(normalizedColor, sizeof(normalizedColor), "#%06X", color);
    
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (strcasecmp(normalizedColor, COLOR_OPTIONS[i].hexColor) == 0) {
            return;
        }
    }
    
    PredefinedColor* newArray = realloc(COLOR_OPTIONS, 
                                      (COLOR_OPTIONS_COUNT + 1) * sizeof(PredefinedColor));
    if (newArray) {
        COLOR_OPTIONS = newArray;
        COLOR_OPTIONS[COLOR_OPTIONS_COUNT].hexColor = _strdup(normalizedColor);
        COLOR_OPTIONS_COUNT++;
    }
}

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

void WriteConfigColor(const char* color_input) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "Failed to open config file for reading: %s\n", config_path);
        return;
    }

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

    char *new_config = (char *)malloc(file_size + 100);
    if (!new_config) {
        fprintf(stderr, "Memory allocation failed!\n");
        free(config_content);
        return;
    }
    new_config[0] = '\0';

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

    file = fopen(config_path, "w");
    if (!file) {
        fprintf(stderr, "Failed to open config file for writing: %s\n", config_path);
        free(new_config);
        return;
    }
    fwrite(new_config, sizeof(char), strlen(new_config), file);
    fclose(file);

    free(new_config);

    ReadConfig();
}

void normalizeColor(const char* input, char* output, size_t output_size) {
    while (isspace(*input)) input++;
    
    char color[32];
    strncpy(color, input, sizeof(color)-1);
    color[sizeof(color)-1] = '\0';
    for (char* p = color; *p; p++) {
        *p = tolower(*p);
    }
    
    for (size_t i = 0; i < CSS_COLORS_COUNT; i++) {
        if (strcmp(color, CSS_COLORS[i].name) == 0) {
            strncpy(output, CSS_COLORS[i].hex, output_size);
            return;
        }
    }
    
    char cleaned[32] = {0};
    int j = 0;
    for (int i = 0; color[i]; i++) {
        if (!isspace(color[i]) && color[i] != ',' && color[i] != '(' && color[i] != ')') {
            cleaned[j++] = color[i];
        }
    }
    cleaned[j] = '\0';
    
    if (cleaned[0] == '#') {
        memmove(cleaned, cleaned + 1, strlen(cleaned));
    }
    
    if (strlen(cleaned) == 3) {
        snprintf(output, output_size, "#%c%c%c%c%c%c",
            cleaned[0], cleaned[0], cleaned[1], cleaned[1], cleaned[2], cleaned[2]);
        return;
    }
    
    if (strlen(cleaned) == 6 && strspn(cleaned, "0123456789abcdefABCDEF") == 6) {
        snprintf(output, output_size, "#%s", cleaned);
        return;
    }
    
    int r = -1, g = -1, b = -1;
    char* rgb_str = color;
    
    if (strncmp(rgb_str, "rgb", 3) == 0) {
        rgb_str += 3;
        while (*rgb_str && (*rgb_str == '(' || isspace(*rgb_str))) rgb_str++;
    }
    
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
    
    strncpy(output, input, output_size);
}

BOOL isValidColor(const char* input) {
    if (!input || !*input) return FALSE;
    
    char normalized[32];
    normalizeColor(input, normalized, sizeof(normalized));
    
    if (normalized[0] != '#' || strlen(normalized) != 7) {
        return FALSE;
    }
    
    for (int i = 1; i < 7; i++) {
        if (!isxdigit((unsigned char)normalized[i])) {
            return FALSE;
        }
    }
    
    int r, g, b;
    if (sscanf(normalized + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
        return (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255);
    }
    
    return FALSE;
}



LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                SendMessage(hwnd, EM_SETSEL, 0, -1);
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
            
            char color[32];
            GetWindowTextA(hwnd, color, sizeof(color));
            
            char normalized[32];
            normalizeColor(color, normalized, sizeof(normalized));
            
            if (normalized[0] == '#') {
                strncpy(PREVIEW_COLOR, normalized, sizeof(PREVIEW_COLOR)-1);
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
            
            char color[32];
            GetWindowTextA(hwnd, color, sizeof(color));
            
            char normalized[32];
            normalizeColor(color, normalized, sizeof(normalized));
            
            if (normalized[0] == '#') {
                strncpy(PREVIEW_COLOR, normalized, sizeof(PREVIEW_COLOR)-1);
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

INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_STATIC, GetLocalizedString(
                L"支持：HEX RGB 颜色名字",
                L"Supported: HEX RGB Color Names"));

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            if (hwndEdit) {
                g_OldEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, 
                                                         (LONG_PTR)ColorEditSubclassProc);
                
                if (CLOCK_TEXT_COLOR[0] != '\0') {
                    SetWindowTextA(hwndEdit, CLOCK_TEXT_COLOR);
                }
            }
            return TRUE;
        }
        
        case WM_COMMAND: {
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                char color[32];
                GetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, color, sizeof(color));
                
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
                
                if (isValidColor(color)) {
                    char normalized_color[10];
                    normalizeColor(color, normalized_color, sizeof(normalized_color));
                    strncpy(CLOCK_TEXT_COLOR, normalized_color, sizeof(CLOCK_TEXT_COLOR)-1);
                    CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR)-1] = '\0';
                    
                    WriteConfigColor(CLOCK_TEXT_COLOR);
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                } else {
                    MessageBoxW(hwndDlg, 
                        GetLocalizedString(
                            L"支持：HEX RGB 颜色名字",
                            L"Supported: HEX RGB Color Names"),
                        GetLocalizedString(L"颜色格式错误", L"Color Format Error"),
                        MB_OK);
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