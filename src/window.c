/**
 * @file window.c
 * @brief 窗口管理功能实现
 * 
 * 本文件实现了应用程序窗口管理相关的功能，
 * 包括窗口创建、位置调整、透明度、点击穿透和拖拽功能。
 */

#include "../include/window.h"
#include "../include/timer.h"
#include "../include/tray.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>

// Forward declaration of WindowProcedure (defined in main.c)
extern LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 窗口尺寸和位置变量
int CLOCK_BASE_WINDOW_WIDTH = 200;
int CLOCK_BASE_WINDOW_HEIGHT = 100;
float CLOCK_WINDOW_SCALE = 1.0f;
int CLOCK_WINDOW_POS_X = 100;
int CLOCK_WINDOW_POS_Y = 100;

// 窗口状态变量
BOOL CLOCK_EDIT_MODE = FALSE;
BOOL CLOCK_IS_DRAGGING = FALSE;
POINT CLOCK_LAST_MOUSE_POS = {0, 0};

// 文本区域变量
RECT CLOCK_TEXT_RECT = {0, 0, 0, 0};
BOOL CLOCK_TEXT_RECT_VALID = FALSE;

// DWM函数指针类型定义
typedef HRESULT (WINAPI *pfnDwmEnableBlurBehindWindow)(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind);
static pfnDwmEnableBlurBehindWindow _DwmEnableBlurBehindWindow = NULL;

// 窗口组合属性类型定义
typedef enum _WINDOWCOMPOSITIONATTRIB {
    WCA_UNDEFINED = 0,
    WCA_NCRENDERING_ENABLED = 1,
    WCA_NCRENDERING_POLICY = 2,
    WCA_TRANSITIONS_FORCEDISABLED = 3,
    WCA_ALLOW_NCPAINT = 4,
    WCA_CAPTION_BUTTON_BOUNDS = 5,
    WCA_NONCLIENT_RTL_LAYOUT = 6,
    WCA_FORCE_ICONIC_REPRESENTATION = 7,
    WCA_EXTENDED_FRAME_BOUNDS = 8,
    WCA_HAS_ICONIC_BITMAP = 9,
    WCA_THEME_ATTRIBUTES = 10,
    WCA_NCRENDERING_EXILED = 11,
    WCA_NCADORNMENTINFO = 12,
    WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
    WCA_VIDEO_OVERLAY_ACTIVE = 14,
    WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
    WCA_DISALLOW_PEEK = 16,
    WCA_CLOAK = 17,
    WCA_CLOAKED = 18,
    WCA_ACCENT_POLICY = 19,
    WCA_FREEZE_REPRESENTATION = 20,
    WCA_EVER_UNCLOAKED = 21,
    WCA_VISUAL_OWNER = 22,
    WCA_HOLOGRAPHIC = 23,
    WCA_EXCLUDED_FROM_DDA = 24,
    WCA_PASSIVEUPDATEMODE = 25,
    WCA_USEDARKMODECOLORS = 26,
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

void SetClickThrough(HWND hwnd, BOOL enable) {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    // 清除之前可能设置的相关样式
    exStyle &= ~WS_EX_TRANSPARENT;
    
    if (enable) {
        // 设置为点击穿透
        exStyle |= WS_EX_TRANSPARENT;
        
        // 如果窗口是分层窗口，确保它正确处理鼠标输入
        if (exStyle & WS_EX_LAYERED) {
            SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
        }
    } else {
        // 确保窗口接收所有鼠标输入
        if (exStyle & WS_EX_LAYERED) {
            // 保持透明度但允许接收鼠标输入
            SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        }
    }
    
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    
    // 更新窗口以应用新样式
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, 
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

BOOL InitDWMFunctions() {
    HMODULE hDwmapi = LoadLibraryA("dwmapi.dll");
    if (hDwmapi) {
        _DwmEnableBlurBehindWindow = (pfnDwmEnableBlurBehindWindow)GetProcAddress(hDwmapi, "DwmEnableBlurBehindWindow");
        return _DwmEnableBlurBehindWindow != NULL;
    }
    return FALSE;
}

void SetBlurBehind(HWND hwnd, BOOL enable) {
    if (enable) {
        ACCENT_POLICY policy = {0};
        policy.AccentState = ACCENT_ENABLE_BLURBEHIND;
        policy.AccentFlags = 0;
        policy.GradientColor = (BLUR_OPACITY << 24) | 0x00FFFFFF;
        
        WINDOWCOMPOSITIONATTRIBDATA data = {0};
        data.Attrib = WCA_ACCENT_POLICY;
        data.pvData = &policy;
        data.cbData = sizeof(policy);
        
        if (SetWindowCompositionAttribute) {
            SetWindowCompositionAttribute(hwnd, &data);
        } else if (_DwmEnableBlurBehindWindow) {
            DWM_BLURBEHIND bb = {0};
            bb.dwFlags = DWM_BB_ENABLE;
            bb.fEnable = TRUE;
            bb.hRgnBlur = NULL;
            _DwmEnableBlurBehindWindow(hwnd, &bb);
        }
    } else {
        ACCENT_POLICY policy = {0};
        policy.AccentState = ACCENT_DISABLED;
        
        WINDOWCOMPOSITIONATTRIBDATA data = {0};
        data.Attrib = WCA_ACCENT_POLICY;
        data.pvData = &policy;
        data.cbData = sizeof(policy);
        
        if (SetWindowCompositionAttribute) {
            SetWindowCompositionAttribute(hwnd, &data);
        } else if (_DwmEnableBlurBehindWindow) {
            DWM_BLURBEHIND bb = {0};
            bb.dwFlags = DWM_BB_ENABLE;
            bb.fEnable = FALSE;
            _DwmEnableBlurBehindWindow(hwnd, &bb);
        }
    }
}

void AdjustWindowPosition(HWND hwnd, BOOL forceOnScreen) {
    if (!forceOnScreen) {
        // 不强制窗口在屏幕内，直接返回
        return;
    }
    
    // 原有的代码，确保窗口在屏幕内
    RECT rect;
    GetWindowRect(hwnd, &rect);
    
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    int x = rect.left;
    int y = rect.top;
    
    // 确保窗口右边缘不会超出屏幕
    if (x + width > screenWidth) {
        x = screenWidth - width;
    }
    
    // 确保窗口下边缘不会超出屏幕
    if (y + height > screenHeight) {
        y = screenHeight - height;
    }
    
    // 确保窗口左边缘不会超出屏幕
    if (x < 0) {
        x = 0;
    }
    
    // 确保窗口上边缘不会超出屏幕
    if (y < 0) {
        y = 0;
    }
    
    // 如果窗口位置需要调整，则移动窗口
    if (x != rect.left || y != rect.top) {
        SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

extern void GetConfigPath(char* path, size_t size);
extern void WriteConfigEditMode(const char* mode);

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
    
    config[0] = new_config[0] = '\0';
    char line[256];
    size_t total_len = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        size_t line_len = strlen(line);
        if (total_len + line_len >= buffer_size - 1) {
            size_t new_size = buffer_size * 2;
            char *temp_config = realloc(config, new_size);
            char *temp_new_config = realloc(new_config, new_size);
            
            if (!temp_config || !temp_new_config) {
                free(config);
                free(new_config);
                fclose(fp);
                return;
            }
            
            config = temp_config;
            new_config = temp_new_config;
            buffer_size = new_size;
        }
        strcat(config, line);
        total_len += line_len;
    }
    fclose(fp);
    
    char *start = config;
    char *end = config + strlen(config);
    BOOL has_edit_mode = FALSE;
    BOOL has_window_scale = FALSE;
    size_t new_config_len = 0;
    
    while (start < end) {
        char *newline = strchr(start, '\n');
        if (!newline) newline = end;
        
        char temp[256] = {0};
        size_t line_len = newline - start;
        if (line_len >= sizeof(temp)) line_len = sizeof(temp) - 1;
        strncpy(temp, start, line_len);
        
        if (strncmp(temp, "CLOCK_WINDOW_POS_X=", 19) == 0) {
            new_config_len += snprintf(new_config + new_config_len, 
                buffer_size - new_config_len, 
                "CLOCK_WINDOW_POS_X=%d\n", CLOCK_WINDOW_POS_X);
        } else if (strncmp(temp, "CLOCK_WINDOW_POS_Y=", 19) == 0) {
            new_config_len += snprintf(new_config + new_config_len,
                buffer_size - new_config_len,
                "CLOCK_WINDOW_POS_Y=%d\n", CLOCK_WINDOW_POS_Y);
        } else if (strncmp(temp, "WINDOW_SCALE=", 13) == 0) {
            new_config_len += snprintf(new_config + new_config_len,
                buffer_size - new_config_len,
                "WINDOW_SCALE=%.2f\n", CLOCK_WINDOW_SCALE);
            has_window_scale = TRUE;
        } else if (strncmp(temp, "CLOCK_EDIT_MODE=", 15) == 0) {
            new_config_len += snprintf(new_config + new_config_len,
                buffer_size - new_config_len,
                "CLOCK_EDIT_MODE=%s\n", CLOCK_EDIT_MODE ? "TRUE" : "FALSE");
            has_edit_mode = TRUE;
        } else {
            size_t remaining = buffer_size - new_config_len;
            if (remaining > line_len + 1) {
                strncpy(new_config + new_config_len, start, line_len);
                new_config_len += line_len;
                new_config[new_config_len++] = '\n';
            }
        }
        
        start = newline + 1;
        if (start > end) break;
    }
    
    if (!has_edit_mode && buffer_size - new_config_len > 50) {
        new_config_len += snprintf(new_config + new_config_len,
            buffer_size - new_config_len,
            "CLOCK_EDIT_MODE=%s\n", CLOCK_EDIT_MODE ? "TRUE" : "FALSE");
    }
    if (!has_window_scale && buffer_size - new_config_len > 50) {
        new_config_len += snprintf(new_config + new_config_len,
            buffer_size - new_config_len,
            "WINDOW_SCALE=%.2f\n", CLOCK_WINDOW_SCALE);
    }
    
    if (new_config_len < buffer_size) {
        new_config[new_config_len] = '\0';
    } else {
        new_config[buffer_size - 1] = '\0';
    }
    
    fp = fopen(config_path, "w");
    if (fp) {
        fputs(new_config, fp);
        fclose(fp);
    }
    
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
        } else if (strncmp(line, "CLOCK_EDIT_MODE=", 16) == 0) {
            CLOCK_EDIT_MODE = (strncmp(line + 16, "TRUE", 4) == 0);
        }
    }
    fclose(fp);
    
    SetWindowPos(hwnd, NULL, 
        CLOCK_WINDOW_POS_X, 
        CLOCK_WINDOW_POS_Y,
        (int)(CLOCK_BASE_WINDOW_WIDTH * CLOCK_WINDOW_SCALE),
        (int)(CLOCK_BASE_WINDOW_HEIGHT * CLOCK_WINDOW_SCALE),
        SWP_NOZORDER
    );
}

BOOL HandleMouseWheel(HWND hwnd, int delta) {
    if (CLOCK_EDIT_MODE) {
        float old_scale = CLOCK_FONT_SCALE_FACTOR;
        
        // 移除原有的位置计算逻辑，直接使用窗口中心作为缩放基准点
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);
        int oldWidth = windowRect.right - windowRect.left;
        int oldHeight = windowRect.bottom - windowRect.top;
        
        // 使用窗口中心作为缩放基准
        float relativeX = 0.5f;
        float relativeY = 0.5f;
        
        float scaleFactor = 1.1f;
        if (delta > 0) {
            CLOCK_FONT_SCALE_FACTOR *= scaleFactor;
            CLOCK_WINDOW_SCALE = CLOCK_FONT_SCALE_FACTOR;
        } else {
            CLOCK_FONT_SCALE_FACTOR /= scaleFactor;
            CLOCK_WINDOW_SCALE = CLOCK_FONT_SCALE_FACTOR;
        }
        
        // 保持缩放范围限制
        if (CLOCK_FONT_SCALE_FACTOR < MIN_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MIN_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MIN_SCALE_FACTOR;
        }
        if (CLOCK_FONT_SCALE_FACTOR > MAX_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MAX_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MAX_SCALE_FACTOR;
        }
        
        if (old_scale != CLOCK_FONT_SCALE_FACTOR) {
            // 计算新尺寸
            int newWidth = (int)(oldWidth * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            int newHeight = (int)(oldHeight * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            
            // 保持窗口中心位置不变
            int newX = windowRect.left + (oldWidth - newWidth)/2;
            int newY = windowRect.top + (oldHeight - newHeight)/2;
            
            SetWindowPos(hwnd, NULL, 
                newX, newY,
                newWidth, newHeight,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
            
            // 触发重绘
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            
            // Save settings after resizing
            SaveWindowSettings(hwnd);
        }
        return TRUE;
    }
    return FALSE;
}

BOOL HandleMouseMove(HWND hwnd) {
    if (CLOCK_EDIT_MODE && CLOCK_IS_DRAGGING) {
        POINT currentPos;
        GetCursorPos(&currentPos);
        
        int deltaX = currentPos.x - CLOCK_LAST_MOUSE_POS.x;
        int deltaY = currentPos.y - CLOCK_LAST_MOUSE_POS.y;
        
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);
        
        SetWindowPos(hwnd, NULL,
            windowRect.left + deltaX,
            windowRect.top + deltaY,
            windowRect.right - windowRect.left,   
            windowRect.bottom - windowRect.top,   
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW   
        );
        
        CLOCK_LAST_MOUSE_POS = currentPos;
        
        UpdateWindow(hwnd);
        
        // Update the position variables and save settings
        CLOCK_WINDOW_POS_X = windowRect.left + deltaX;
        CLOCK_WINDOW_POS_Y = windowRect.top + deltaY;
        SaveWindowSettings(hwnd);
        
        return TRUE;
    }
    return FALSE;
}

HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    // 窗口类注册
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProcedure;  // 注：假设WindowProcedure已在其他地方定义
    wc.hInstance = hInstance;
    wc.lpszClassName = "CatimeWindow";
    
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return NULL;
    }

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "CatimeWindow",
        "Catime",
        WS_POPUP,
        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
        CLOCK_BASE_WINDOW_WIDTH, CLOCK_BASE_WINDOW_HEIGHT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return NULL;
    }

    EnableWindow(hwnd, TRUE);
    SetFocus(hwnd);

    // 设置窗口透明度
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

    // 设置模糊效果
    SetBlurBehind(hwnd, FALSE);

    // 初始化托盘图标
    InitTrayIcon(hwnd, hInstance);

    // 显示窗口
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    return hwnd;
}

float CLOCK_FONT_SCALE_FACTOR = 1.0f;
int CLOCK_BASE_FONT_SIZE = 24;

BOOL OpenFileDialog(HWND hwnd, char* filePath, DWORD maxPath) {
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = maxPath;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "";
    
    return GetOpenFileName(&ofn);
}
