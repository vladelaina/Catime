// 声明GetConfigPath函数
extern void GetConfigPath(char* path, size_t size);

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")

// Window dimensions and properties
int CLOCK_BASE_WINDOW_WIDTH = 200;
int CLOCK_BASE_WINDOW_HEIGHT = 100;
float CLOCK_WINDOW_SCALE = 1.0f;
int CLOCK_BASE_FONT_SIZE = 24;
float CLOCK_FONT_SCALE_FACTOR = 1.0f;
int CLOCK_WINDOW_POS_X = 100;
int CLOCK_WINDOW_POS_Y = 100;

// Window state flags
BOOL CLOCK_EDIT_MODE = FALSE;
BOOL CLOCK_IS_DRAGGING = FALSE;
POINT CLOCK_LAST_MOUSE_POS = {0, 0};
RECT CLOCK_TEXT_RECT = {0, 0, 0, 0};
BOOL CLOCK_TEXT_RECT_VALID = FALSE;

// Function declarations
void SetClickThrough(HWND hwnd, BOOL enable);
void SetBlurBehind(HWND hwnd, BOOL enable);
void LoadWindowSettings(HWND hwnd);
void SaveWindowSettings(HWND hwnd);
void AdjustWindowPosition(HWND hwnd);

// Window click-through functionality
void SetClickThrough(HWND hwnd, BOOL enable) {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (enable) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
}

// Window blur effect
typedef HRESULT (WINAPI *pfnDwmEnableBlurBehindWindow)(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind);
static pfnDwmEnableBlurBehindWindow pDwmEnableBlurBehindWindow = NULL;

BOOL InitDWMFunctions() {
    HMODULE hDwmapi = LoadLibraryA("dwmapi.dll");
    if (hDwmapi) {
        pDwmEnableBlurBehindWindow = (pfnDwmEnableBlurBehindWindow)GetProcAddress(hDwmapi, "DwmEnableBlurBehindWindow");
        return pDwmEnableBlurBehindWindow != NULL;
    }
    return FALSE;
}

// 导出DwmEnableBlurBehindWindow函数包装
HRESULT DwmEnableBlurBehindWindow(HWND hwnd, const DWM_BLURBEHIND* pBlurBehind) {
    if (pDwmEnableBlurBehindWindow) {
        return pDwmEnableBlurBehindWindow(hwnd, pBlurBehind);
    }
    return E_NOTIMPL;
}

// Window composition attributes
typedef enum _WINDOWCOMPOSITIONATTRIB {
    WCA_ACCENT_POLICY = 19
} WINDOWCOMPOSITIONATTRIB;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

typedef enum _ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_INVALID_STATE = 5
} ACCENT_STATE;

typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
} ACCENT_POLICY;

WINUSERAPI BOOL WINAPI SetWindowCompositionAttribute(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA* pData);

void SetBlurBehind(HWND hwnd, BOOL enable) {
    if (enable) {
        ACCENT_POLICY accent = {ACCENT_ENABLE_BLURBEHIND, 0, 0, 0};
        WINDOWCOMPOSITIONATTRIBDATA data = {
            WCA_ACCENT_POLICY,
            &accent,
            sizeof(accent)
        };
        SetWindowCompositionAttribute(hwnd, &data);
    } else {
        ACCENT_POLICY accent = {ACCENT_DISABLED, 0, 0, 0};
        WINDOWCOMPOSITIONATTRIBDATA data = {
            WCA_ACCENT_POLICY,
            &accent,
            sizeof(accent)
        };
        SetWindowCompositionAttribute(hwnd, &data);
    }
}

// Window settings management
void SaveWindowSettings(HWND hwnd) {
    if (!hwnd) return;

    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) return;
    
    CLOCK_WINDOW_POS_X = rect.left;
    CLOCK_WINDOW_POS_Y = rect.top;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *fp = fopen(config_path, "r");
    if (!fp) return;
    
    size_t buffer_size = 8192;   
    char *config = malloc(buffer_size);
    char *new_config = malloc(buffer_size);
    if (!config || !new_config) {
        if (config) free(config);
        if (new_config) free(new_config);
        fclose(fp);
        return;
    }
    
    size_t total_size = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "CLOCK_WINDOW_POS_X=", 19) != 0 &&
            strncmp(line, "CLOCK_WINDOW_POS_Y=", 19) != 0 &&
            strncmp(line, "WINDOW_SCALE=", 13) != 0) {
            strcat(config, line);
        }
    }
    fclose(fp);
    
    fp = fopen(config_path, "w");
    if (!fp) {
        free(config);
        free(new_config);
        return;
    }
    
    fprintf(fp, "%s", config);
    fprintf(fp, "CLOCK_WINDOW_POS_X=%d\n", CLOCK_WINDOW_POS_X);
    fprintf(fp, "CLOCK_WINDOW_POS_Y=%d\n", CLOCK_WINDOW_POS_Y);
    fprintf(fp, "WINDOW_SCALE=%.2f\n", CLOCK_WINDOW_SCALE);
    
    fclose(fp);
    free(config);
    free(new_config);
}

void LoadWindowSettings(HWND hwnd) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *fp = fopen(config_path, "r");
    if (!fp) return;
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        
        if (strncmp(line, "CLOCK_WINDOW_POS_X=", 19) == 0) {
            CLOCK_WINDOW_POS_X = atoi(line + 19);
        } else if (strncmp(line, "CLOCK_WINDOW_POS_Y=", 19) == 0) {
            CLOCK_WINDOW_POS_Y = atoi(line + 19);
        } else if (strncmp(line, "WINDOW_SCALE=", 13) == 0) {
            CLOCK_WINDOW_SCALE = atof(line + 13);
            CLOCK_FONT_SCALE_FACTOR = CLOCK_WINDOW_SCALE;
        }
    }
    fclose(fp);
    
    SetWindowPos(hwnd, NULL, 
        CLOCK_WINDOW_POS_X, 
        CLOCK_WINDOW_POS_Y,
        0, 0,
        SWP_NOSIZE | SWP_NOZORDER
    );
}

void AdjustWindowPosition(HWND hwnd) {
    if (!hwnd) return;
    
    RECT rect;
    GetWindowRect(hwnd, &rect);
    
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;
    
    // Ensure window stays within screen bounds
    if (rect.left < 0) rect.left = 0;
    if (rect.top < 0) rect.top = 0;
    if (rect.right > screenWidth) rect.left = screenWidth - windowWidth;
    if (rect.bottom > screenHeight) rect.top = screenHeight - windowHeight;
    
    SetWindowPos(hwnd, NULL, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}