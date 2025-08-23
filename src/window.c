/**
 * @file window.c
 * @brief Main window management with advanced visual effects and DPI awareness
 * Handles window creation, positioning, transparency, blur effects, and user interactions
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
#include <string.h>
#include <windows.h>

extern LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#ifndef _INC_WINUSER
WINUSERAPI BOOL WINAPI SetProcessDPIAware(VOID);
#endif

/** @brief Base window dimensions and scaling */
int CLOCK_BASE_WINDOW_WIDTH = 200;
int CLOCK_BASE_WINDOW_HEIGHT = 100;
float CLOCK_WINDOW_SCALE = 1.0f;
int CLOCK_WINDOW_POS_X = 100;
int CLOCK_WINDOW_POS_Y = 100;

/** @brief Window interaction state */
BOOL CLOCK_EDIT_MODE = FALSE;           /**< Edit mode enables dragging and resizing */
BOOL CLOCK_IS_DRAGGING = FALSE;         /**< Current drag operation state */
POINT CLOCK_LAST_MOUSE_POS = {0, 0};    /**< Last recorded mouse position for dragging */
BOOL CLOCK_WINDOW_TOPMOST = TRUE;       /**< Window always-on-top state */

/** @brief Text rendering optimization */
RECT CLOCK_TEXT_RECT = {0, 0, 0, 0};    /**< Cached text rectangle for repainting */
BOOL CLOCK_TEXT_RECT_VALID = FALSE;     /**< Validity flag for cached text rectangle */

/** @brief Dynamic loading of DWM functions for blur effects */
typedef HRESULT (WINAPI *pfnDwmEnableBlurBehindWindow)(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind);
static pfnDwmEnableBlurBehindWindow _DwmEnableBlurBehindWindow = NULL;


/** @brief Windows composition attributes for advanced visual effects */
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
    WCA_ACCENT_POLICY = 19,             /**< Accent policy for blur/transparency effects */
    WCA_FREEZE_REPRESENTATION = 20,
    WCA_EVER_UNCLOAKED = 21,
    WCA_VISUAL_OWNER = 22,
    WCA_HOLOGRAPHIC = 23,
    WCA_EXCLUDED_FROM_DDA = 24,
    WCA_PASSIVEUPDATEMODE = 25,
    WCA_USEDARKMODECOLORS = 26,
    WCA_LAST = 27
} WINDOWCOMPOSITIONATTRIB;

/** @brief Structure for setting window composition attributes */
typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;    /**< Attribute type to set */
    PVOID pvData;                      /**< Pointer to attribute data */
    SIZE_T cbData;                     /**< Size of attribute data */
} WINDOWCOMPOSITIONATTRIBDATA;

WINUSERAPI BOOL WINAPI SetWindowCompositionAttribute(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA* pData);

/** @brief Accent states for window transparency and blur effects */
typedef enum _ACCENT_STATE {
    ACCENT_DISABLED = 0,                        /**< No transparency effect */
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,              /**< Blur behind window effect */
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,       /**< Acrylic blur effect */
    ACCENT_INVALID_STATE = 5
} ACCENT_STATE;

/** @brief Accent policy configuration for window effects */
typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;          /**< Type of accent effect */
    DWORD AccentFlags;                 /**< Additional flags for effect */
    DWORD GradientColor;               /**< Color for gradient effects */
    DWORD AnimationId;                 /**< Animation identifier */
} ACCENT_POLICY;

/**
 * @brief Configure window click-through behavior for edit mode
 * @param hwnd Window handle to modify
 * @param enable TRUE to enable click-through, FALSE to disable
 * Toggles WS_EX_TRANSPARENT style and adjusts layered window attributes
 */
void SetClickThrough(HWND hwnd, BOOL enable) {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    /** Clear existing transparent flag */
    exStyle &= ~WS_EX_TRANSPARENT;
    
    if (enable) {
        /** Enable click-through for overlay behavior */
        exStyle |= WS_EX_TRANSPARENT;
        
        /** Set color key for transparency */
        if (exStyle & WS_EX_LAYERED) {
            SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
        }
    } else {
        /** Disable click-through for normal interaction */
        if (exStyle & WS_EX_LAYERED) {
            /** Use alpha transparency instead of color key */
            SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        }
    }
    
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    
    /** Force window frame update to apply style changes */
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, 
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

/**
 * @brief Initialize DWM (Desktop Window Manager) functions for blur effects
 * @return TRUE if DWM functions loaded successfully, FALSE otherwise
 * Dynamically loads dwmapi.dll to access blur functionality
 */
BOOL InitDWMFunctions() {
    HMODULE hDwmapi = LoadLibraryW(L"dwmapi.dll");
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
        policy.GradientColor = (180 << 24) | 0x00202020;
        
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

        return;
    }
    

    RECT rect;
    GetWindowRect(hwnd, &rect);
    
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    int x = rect.left;
    int y = rect.top;
    

    if (x + width > screenWidth) {
        x = screenWidth - width;
    }
    

    if (y + height > screenHeight) {
        y = screenHeight - height;
    }
    

    if (x < 0) {
        x = 0;
    }
    

    if (y < 0) {
        y = 0;
    }
    

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
        

        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);
        int oldWidth = windowRect.right - windowRect.left;
        int oldHeight = windowRect.bottom - windowRect.top;
        

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
        

        if (CLOCK_FONT_SCALE_FACTOR < MIN_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MIN_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MIN_SCALE_FACTOR;
        }
        if (CLOCK_FONT_SCALE_FACTOR > MAX_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MAX_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MAX_SCALE_FACTOR;
        }
        
        if (old_scale != CLOCK_FONT_SCALE_FACTOR) {

            int newWidth = (int)(oldWidth * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            int newHeight = (int)(oldHeight * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            

            int newX = windowRect.left + (oldWidth - newWidth)/2;
            int newY = windowRect.top + (oldHeight - newHeight)/2;
            
            SetWindowPos(hwnd, NULL, 
                newX, newY,
                newWidth, newHeight,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
            

            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            

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
        

        CLOCK_WINDOW_POS_X = windowRect.left + deltaX;
        CLOCK_WINDOW_POS_Y = windowRect.top + deltaY;
        SaveWindowSettings(hwnd);
        
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Create and initialize the main application window
 * @param hInstance Application instance handle
 * @param nCmdShow Window show command
 * @return Window handle on success, NULL on failure
 * Sets up layered window with transparency, tray icon, and topmost behavior
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    /** Register window class */
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CatimeWindow";
    
    if (!RegisterClassW(&wc)) {
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return NULL;
    }

    /** Configure extended window styles for layered transparency */
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    
    /** Add no-activate style for non-topmost windows */
    if (!CLOCK_WINDOW_TOPMOST) {
        exStyle |= WS_EX_NOACTIVATE;
    }
    
    /** Create popup window with no frame */
    HWND hwnd = CreateWindowExW(
        exStyle,
        L"CatimeWindow",
        L"Catime",
        WS_POPUP,
        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
        CLOCK_BASE_WINDOW_WIDTH, CLOCK_BASE_WINDOW_HEIGHT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd) {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return NULL;
    }

    EnableWindow(hwnd, TRUE);
    SetFocus(hwnd);

    /** Set up color key transparency for black background */
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

    /** Disable blur effects initially */
    SetBlurBehind(hwnd, FALSE);

    /** Initialize system tray icon */
    InitTrayIcon(hwnd, hInstance);

    /** Show window and apply topmost behavior */
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    /** Apply topmost or desktop attachment based on settings */
    if (CLOCK_WINDOW_TOPMOST) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, 
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        /** Attach to desktop for wallpaper-like behavior */
        ReattachToDesktop(hwnd);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }

    return hwnd;
}

/** @brief Font scaling configuration */
float CLOCK_FONT_SCALE_FACTOR = 1.0f;
int CLOCK_BASE_FONT_SIZE = 24;

/**
 * @brief Initialize application with DPI awareness and load configurations
 * @param hInstance Application instance handle
 * @return TRUE if initialization succeeded, FALSE on error
 * Sets up DPI awareness, loads config, fonts, and language settings
 */
BOOL InitializeApplication(HINSTANCE hInstance) {
    #ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
    #define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
    #endif
    
    /** DPI awareness levels for Windows compatibility */
    typedef enum {
        PROCESS_DPI_UNAWARE = 0,
        PROCESS_SYSTEM_DPI_AWARE = 1,
        PROCESS_PER_MONITOR_DPI_AWARE = 2
    } PROCESS_DPI_AWARENESS;
    
    /** Set up DPI awareness with fallback to older APIs */
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
        SetProcessDpiAwarenessContextFunc setProcessDpiAwarenessContextFunc =
            (SetProcessDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        
        if (setProcessDpiAwarenessContextFunc) {
            /** Windows 10 version 1703+ per-monitor DPI awareness */
            setProcessDpiAwarenessContextFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        } else {
            /** Fallback to Windows 8.1+ DPI awareness */
            HMODULE hShcore = LoadLibraryW(L"shcore.dll");
            if (hShcore) {
                typedef HRESULT(WINAPI* SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);
                SetProcessDpiAwarenessFunc setProcessDpiAwarenessFunc =
                    (SetProcessDpiAwarenessFunc)GetProcAddress(hShcore, "SetProcessDpiAwareness");
                
                if (setProcessDpiAwarenessFunc) {
                    /** Windows 8.1+ per-monitor DPI awareness */
                    setProcessDpiAwarenessFunc(PROCESS_PER_MONITOR_DPI_AWARE);
                } else {
                    /** Fallback to basic DPI awareness */
                    SetProcessDPIAware();
                }
                
                FreeLibrary(hShcore);
            } else {
                /** Final fallback for older Windows versions */
                SetProcessDPIAware();
            }
        }
    }
    
    /** Set console code page for Chinese character support */
    SetConsoleOutputCP(936);
    SetConsoleCP(936);

    /** Load application configuration and initialize components */
    ReadConfig();
    
    /** Check if this is the first run and extract embedded fonts if needed */
    if (IsFirstRun()) {
        extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);
        if (ExtractEmbeddedFontsToFolder(hInstance)) {
            /** Successfully extracted fonts, mark first run as completed */
            SetFirstRunCompleted();
        }
    }
    
    /** Check font license version acceptance - this will happen on first run and version updates */
    extern BOOL NeedsFontLicenseVersionAcceptance(void);
    if (NeedsFontLicenseVersionAcceptance()) {
        /** Font license version needs acceptance, will be handled later in main window messages */
        /** This is just a check during initialization - actual dialog will be shown when needed */
    }
    
    UpdateStartupShortcut();
    InitializeDefaultLanguage();

    /** Load font from configuration */
    char actualFontFileName[MAX_PATH];
    
    /** Check if FONT_FILE_NAME has LOCALAPPDATA path prefix */
    const char* localappdata_prefix = "%LOCALAPPDATA%\\Catime\\resources\\fonts\\";
    if (_strnicmp(FONT_FILE_NAME, localappdata_prefix, strlen(localappdata_prefix)) == 0) {
        /** Extract just the filename */
        strncpy(actualFontFileName, FONT_FILE_NAME + strlen(localappdata_prefix), sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
    } else {
        /** Use as-is for embedded fonts */
        strncpy(actualFontFileName, FONT_FILE_NAME, sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
    }
    
    /** Load font from fonts folder and update FONT_INTERNAL_NAME */
    extern BOOL LoadFontByNameAndGetRealName(HINSTANCE hInstance, const char* fontFileName, char* realFontName, size_t realFontNameSize);
    LoadFontByNameAndGetRealName(hInstance, actualFontFileName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));

    /** Set initial timer value from configuration */
    CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
    
    return TRUE;
}

/**
 * @brief Open standard Windows file selection dialog
 * @param hwnd Parent window handle
 * @param filePath Buffer to receive selected file path
 * @param maxPath Maximum size of file path buffer
 * @return TRUE if file selected, FALSE if cancelled
 * Uses common dialog API for file browsing functionality
 */
BOOL OpenFileDialog(HWND hwnd, wchar_t* filePath, DWORD maxPath) {
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = maxPath;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"";
    
    return GetOpenFileNameW(&ofn);
}


/**
 * @brief Toggle window always-on-top behavior
 * @param hwnd Window handle to modify
 * @param topmost TRUE for topmost, FALSE for desktop attachment
 * Switches between HWND_TOPMOST and desktop wallpaper level positioning
 */
void SetWindowTopmost(HWND hwnd, BOOL topmost) {
    CLOCK_WINDOW_TOPMOST = topmost;
    
    /** Modify window extended styles based on topmost state */
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    if (topmost) {
        /** Enable activation for topmost windows */
        exStyle &= ~WS_EX_NOACTIVATE;
        
        /** Detach from desktop parent and set topmost z-order */
        SetParent(hwnd, NULL);
        SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, 0);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    } else {
        /** Disable activation and attach to desktop */
        exStyle |= WS_EX_NOACTIVATE;
        ReattachToDesktop(hwnd);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    }
    
    /** Apply extended style changes */
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    
    /** Force frame change update */
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    
    /** Persist topmost setting to configuration */
    WriteConfigTopmost(topmost ? "TRUE" : "FALSE");
}

/**
 * @brief Attach window to desktop wallpaper level for non-intrusive display
 * @param hwnd Window handle to attach to desktop
 * Searches for WorkerW window hosting desktop and parents window to it
 */
void ReattachToDesktop(HWND hwnd) {
    /** Find desktop window hierarchy (Progman -> WorkerW -> SHELLDLL_DefView) */
    HWND hProgman = FindWindowW(L"Progman", NULL);
    HWND hDesktop = NULL;
    
    if (hProgman != NULL) {
        hDesktop = hProgman;
        HWND hWorkerW = FindWindowExW(NULL, NULL, L"WorkerW", NULL);
        while (hWorkerW != NULL) {
            HWND hView = FindWindowExW(hWorkerW, NULL, L"SHELLDLL_DefView", NULL);
            if (hView != NULL) {
                hDesktop = hWorkerW;
                break;
            }
            hWorkerW = FindWindowExW(NULL, hWorkerW, L"WorkerW", NULL);
        }
    }
    
    if (hDesktop != NULL) {
        /** Parent window to desktop worker for wallpaper-level display */
        SetParent(hwnd, hDesktop);
        ShowWindow(hwnd, SW_SHOW);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else {
        /** Fallback if desktop hierarchy not found */
        SetParent(hwnd, NULL);
        ShowWindow(hwnd, SW_SHOW);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}
