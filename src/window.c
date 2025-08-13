/**
 * @file window.c
 * @brief Window management functionality implementation
 * 
 * This file implements the functionality related to application window management,
 * including window creation, position adjustment, transparency, click-through, and drag functionality.
 */

#include "../include/window.h"
#include "../include/timer.h"
#include "../include/tray.h"
#include "../include/language.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/startup.h"
#include "../include/config.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

// Forward declaration of WindowProcedure (defined in main.c)
extern LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Add declaration for SetProcessDPIAware function
#ifndef _INC_WINUSER
// If not included by windows.h, add SetProcessDPIAware function declaration
WINUSERAPI BOOL WINAPI SetProcessDPIAware(VOID);
#endif

// Window size and position variables
int CLOCK_BASE_WINDOW_WIDTH = 200;
int CLOCK_BASE_WINDOW_HEIGHT = 100;
float CLOCK_WINDOW_SCALE = 1.0f;
int CLOCK_WINDOW_POS_X = 100;
int CLOCK_WINDOW_POS_Y = 100;

// Window state variables
BOOL CLOCK_EDIT_MODE = FALSE;
BOOL CLOCK_IS_DRAGGING = FALSE;
POINT CLOCK_LAST_MOUSE_POS = {0, 0};
BOOL CLOCK_WINDOW_TOPMOST = TRUE;  // Default topmost

// Text area variables
RECT CLOCK_TEXT_RECT = {0, 0, 0, 0};
BOOL CLOCK_TEXT_RECT_VALID = FALSE;

// DWM function pointer type definition
typedef HRESULT (WINAPI *pfnDwmEnableBlurBehindWindow)(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind);
static pfnDwmEnableBlurBehindWindow _DwmEnableBlurBehindWindow = NULL;

// Window composition attribute type definition
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
    
    // Clear previously set related styles
    exStyle &= ~WS_EX_TRANSPARENT;
    
    if (enable) {
        // Set click-through
        exStyle |= WS_EX_TRANSPARENT;
        
        // If the window is a layered window, ensure it properly handles mouse input
        if (exStyle & WS_EX_LAYERED) {
            SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
        }
    } else {
        // Ensure window receives all mouse input
        if (exStyle & WS_EX_LAYERED) {
            // Maintain transparency but allow receiving mouse input
            SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        }
    }
    
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    
    // Update window to apply new style
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
        policy.GradientColor = (180 << 24) | 0x00202020;  // Changed to dark gray background with 180 transparency
        
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
        // Do not force window to be on screen, return directly
        return;
    }
    
    // Original code to ensure window is on screen
    RECT rect;
    GetWindowRect(hwnd, &rect);
    
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    int x = rect.left;
    int y = rect.top;
    
    // Ensure window right edge doesn't exceed screen
    if (x + width > screenWidth) {
        x = screenWidth - width;
    }
    
    // Ensure window bottom edge doesn't exceed screen
    if (y + height > screenHeight) {
        y = screenHeight - height;
    }
    
    // Ensure window left edge doesn't exceed screen
    if (x < 0) {
        x = 0;
    }
    
    // Ensure window top edge doesn't exceed screen
    if (y < 0) {
        y = 0;
    }
    
    // If window position needs adjustment, move the window
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
        }
    }
    fclose(fp);
    
    // Apply position from config file directly, without additional adjustments
    SetWindowPos(hwnd, NULL, 
        CLOCK_WINDOW_POS_X, 
        CLOCK_WINDOW_POS_Y,
        (int)(CLOCK_BASE_WINDOW_WIDTH * CLOCK_WINDOW_SCALE),
        (int)(CLOCK_BASE_WINDOW_HEIGHT * CLOCK_WINDOW_SCALE),
        SWP_NOZORDER
    );
    
    // Don't call AdjustWindowPosition to avoid overriding user settings
}

BOOL HandleMouseWheel(HWND hwnd, int delta) {
    if (CLOCK_EDIT_MODE) {
        float old_scale = CLOCK_FONT_SCALE_FACTOR;
        
        // Remove original position calculation logic, directly use window center as scaling reference point
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);
        int oldWidth = windowRect.right - windowRect.left;
        int oldHeight = windowRect.bottom - windowRect.top;
        
        // Use window center as scaling reference
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
        
        // Maintain scale range limits
        if (CLOCK_FONT_SCALE_FACTOR < MIN_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MIN_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MIN_SCALE_FACTOR;
        }
        if (CLOCK_FONT_SCALE_FACTOR > MAX_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MAX_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MAX_SCALE_FACTOR;
        }
        
        if (old_scale != CLOCK_FONT_SCALE_FACTOR) {
            // Calculate new dimensions
            int newWidth = (int)(oldWidth * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            int newHeight = (int)(oldHeight * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            
            // Keep window center position unchanged
            int newX = windowRect.left + (oldWidth - newWidth)/2;
            int newY = windowRect.top + (oldHeight - newHeight)/2;
            
            SetWindowPos(hwnd, NULL, 
                newX, newY,
                newWidth, newHeight,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
            
            // Trigger redraw
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
    // Window class registration
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInstance;
    wc.lpszClassName = "CatimeWindow";
    
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return NULL;
    }

    // Set extended style
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    
    // If not in topmost mode, add WS_EX_NOACTIVATE extended style
    if (!CLOCK_WINDOW_TOPMOST) {
        exStyle |= WS_EX_NOACTIVATE;
    }
    
    // Create window
    HWND hwnd = CreateWindowEx(
        exStyle,
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

    // Set window transparency
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

    // Set blur effect
    SetBlurBehind(hwnd, FALSE);

    // Initialize tray icon
    InitTrayIcon(hwnd, hInstance);

    // Show window
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Set window position and parent based on topmost status
    if (CLOCK_WINDOW_TOPMOST) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, 
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        ReattachToDesktop(hwnd);
    }

    return hwnd;
}

float CLOCK_FONT_SCALE_FACTOR = 1.0f;
int CLOCK_BASE_FONT_SIZE = 24;

BOOL InitializeApplication(HINSTANCE hInstance) {
    // Set DPI awareness mode to Per-Monitor DPI Aware to properly handle scaling when moving window between displays with different DPIs
    // Use newer API SetProcessDpiAwarenessContext if available, otherwise fallback to older APIs
    
    // Define DPI awareness related constants and types
    #ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
    #define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
    #endif
    
    // Define PROCESS_DPI_AWARENESS enum
    typedef enum {
        PROCESS_DPI_UNAWARE = 0,
        PROCESS_SYSTEM_DPI_AWARE = 1,
        PROCESS_PER_MONITOR_DPI_AWARE = 2
    } PROCESS_DPI_AWARENESS;
    
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
        SetProcessDpiAwarenessContextFunc setProcessDpiAwarenessContextFunc =
            (SetProcessDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        
        if (setProcessDpiAwarenessContextFunc) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 is the latest DPI awareness mode
            // It provides better multi-monitor DPI support
            setProcessDpiAwarenessContextFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        } else {
            // Try using older API
            HMODULE hShcore = LoadLibraryA("shcore.dll");
            if (hShcore) {
                typedef HRESULT(WINAPI* SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);
                SetProcessDpiAwarenessFunc setProcessDpiAwarenessFunc =
                    (SetProcessDpiAwarenessFunc)GetProcAddress(hShcore, "SetProcessDpiAwareness");
                
                if (setProcessDpiAwarenessFunc) {
                    // PROCESS_PER_MONITOR_DPI_AWARE corresponds to per-monitor DPI awareness
                    setProcessDpiAwarenessFunc(PROCESS_PER_MONITOR_DPI_AWARE);
                } else {
                    // Finally try the oldest API
                    SetProcessDPIAware();
                }
                
                FreeLibrary(hShcore);
            } else {
                // If shcore.dll is not available, use the most basic DPI awareness API
                SetProcessDPIAware();
            }
        }
    }
    
    SetConsoleOutputCP(936);
    SetConsoleCP(936);

    // Modified initialization order: read config file first, then initialize other features
    ReadConfig();
    UpdateStartupShortcut();
    InitializeDefaultLanguage();

    int defaultFontIndex = -1;
    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
        if (strcmp(fontResources[i].fontName, FONT_FILE_NAME) == 0) {
            defaultFontIndex = i;
            break;
        }
    }

    if (defaultFontIndex != -1) {
        LoadFontFromResource(hInstance, fontResources[defaultFontIndex].resourceId);
    }

    CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
    
    return TRUE;
}

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

// Add function to set window topmost state
void SetWindowTopmost(HWND hwnd, BOOL topmost) {
    CLOCK_WINDOW_TOPMOST = topmost;
    
    // Get current window style
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    if (topmost) {
        // Topmost mode: remove no-activate style (if exists), add topmost style
        exStyle &= ~WS_EX_NOACTIVATE;
        
        // If window was previously set as desktop child window, need to restore
        // First set window as top-level window, clear parent window relationship
        SetParent(hwnd, NULL);
        
        // Reset window owner, ensure Z-order is correct
        SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, 0);
        
        // Set window position to top layer, and force window update
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    } else {
        // Non-topmost mode: add no-activate style to prevent window from gaining focus
        exStyle |= WS_EX_NOACTIVATE;
        ReattachToDesktop(hwnd);
    }
    
    // Apply new window style
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    
    // Force window update
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    
    // Save window topmost setting
    WriteConfigTopmost(topmost ? "TRUE" : "FALSE");
}

void ReattachToDesktop(HWND hwnd) {
    // Set as child window of desktop (WorkerW preferred), so Win+D won't minimize it
    HWND hProgman = FindWindow("Progman", NULL);
    HWND hDesktop = NULL;
    
    if (hProgman != NULL) {
        hDesktop = hProgman;
        HWND hWorkerW = FindWindowEx(NULL, NULL, "WorkerW", NULL);
        while (hWorkerW != NULL) {
            HWND hView = FindWindowEx(hWorkerW, NULL, "SHELLDLL_DefView", NULL);
            if (hView != NULL) {
                hDesktop = hWorkerW;
                break;
            }
            hWorkerW = FindWindowEx(NULL, hWorkerW, "WorkerW", NULL);
        }
    }
    
    if (hDesktop != NULL) {
        SetParent(hwnd, hDesktop);
        // Keep it at the bottom of Z in non-topmost case but visible
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        // Fallback: still place to bottom if desktop container not found
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}
