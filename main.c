#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <dwmapi.h>
#include "resource.h"
#include <winnls.h>
#include <commdlg.h>  // 用于 CHOOSECOLOR 结构体和 ChooseColor 函数

// 函数声明
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english);
void InitializeDefaultLanguage(void);
COLORREF ShowColorDialog(HWND hwnd);  // 添加这一行
UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);

// 在文件开头的函数声明区域添加（其他函数声明的地方）
void WriteLog(const char* format, ...);

#define CATIME_VERSION "1.0.2"  
#define VK_MEDIA_PLAY_PAUSE 0xB3
#define VK_MEDIA_STOP 0xB2
#define KEYEVENTF_KEYUP 0x0002

#define UNICODE
#define _UNICODE

#define UPDATE_URL_GITHUB    "https://github.com/vladelaina/Catime/releases"
#define UPDATE_URL_123PAN    "https://www.123684.com/s/ruESjv-2CZUA"
#define UPDATE_URL_LANZOU    "https://wwrx.lanzoup.com/b00hqiiskj"
#define FEEDBACK_URL        "https://www.bilibili.com/video/BV1ztFeeQEYP"
#define FEEDBACK_URL_GITHUB  "https://github.com/vladelaina/Catime/issues"
#define FEEDBACK_URL_BILIBILI "https://message.bilibili.com/#/whisper/mid1862395225"

#define CLOCK_IDM_VERSION 131
#define CLOCK_IDM_CHECK_UPDATE 133   
#define CLOCK_IDM_UPDATE_GITHUB    134
#define CLOCK_IDM_UPDATE_123PAN    135
#define CLOCK_IDM_UPDATE_LANZOU    136
#define CLOCK_IDM_FEEDBACK 132   
#define CLOCK_IDM_LANGUAGE_MENU    160
#define CLOCK_IDM_LANG_CHINESE     161
#define CLOCK_IDM_LANG_ENGLISH     162
#define CLOCK_IDM_LANG_CHINESE_TRAD  163
#define CLOCK_IDM_LANG_SPANISH       164
#define CLOCK_IDM_LANG_FRENCH        165
#define CLOCK_IDM_LANG_GERMAN        166
#define CLOCK_IDM_LANG_RUSSIAN       167
#define CLOCK_IDM_LANG_PORTUGUESE    168
#define CLOCK_IDM_LANG_JAPANESE      169
#define CLOCK_IDM_LANG_KOREAN        170
#define CLOCK_IDM_FEEDBACK_GITHUB 137
#define CLOCK_IDM_FEEDBACK_BILIBILI 138

// 在文件开头的宏定义区域添加
#define CLOCK_IDC_TIMEOUT_BROWSE 140

// 语言枚举
typedef enum {
    APP_LANG_CHINESE_SIMP,    // 简体中文
    APP_LANG_CHINESE_TRAD,    // 繁体中文
    APP_LANG_ENGLISH,         // 英语
    APP_LANG_SPANISH,         // 西班牙语
    APP_LANG_FRENCH,          // 法语
    APP_LANG_GERMAN,          // 德语
    APP_LANG_RUSSIAN,         // 俄语
    APP_LANG_PORTUGUESE,      // 葡萄牙语
    APP_LANG_JAPANESE,        // 日语
    APP_LANG_KOREAN           // 韩语
} AppLanguage;

// 全局变量声明区域
AppLanguage CURRENT_LANGUAGE = APP_LANG_CHINESE_SIMP;  // 默认使用简体中文

void PauseMediaPlayback(void);

typedef struct {
    const char* hexColor;
} PredefinedColor;

static const PredefinedColor COLOR_OPTIONS[] = {
    {"#FFFFFF"},   
    {"#FFB6C1"},   
    {"#6495ED"},   
    {"#5EFFFF"},   
    {"#98FB98"},   
    {"#9370DB"},   
    {"#3E3A55"},   
    {"#7FFFD4"},   
    {"#F08080"},   
    {"#FF7F50"}    
};

#define COLOR_OPTIONS_COUNT (sizeof(COLOR_OPTIONS) / sizeof(COLOR_OPTIONS[0]))

void SetClickThrough(HWND hwnd, BOOL enable);

void SetClickThrough(HWND hwnd, BOOL enable) {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (enable) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
}

#define MIN_SCALE_FACTOR 0.5f
#define MAX_SCALE_FACTOR 100.0f

#define BLUR_OPACITY 192
#define BLUR_TRANSITION_MS 200

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")

typedef HRESULT (WINAPI *pfnDwmEnableBlurBehindWindow)(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind);
static pfnDwmEnableBlurBehindWindow _DwmEnableBlurBehindWindow = NULL;

BOOL InitDWMFunctions() {
    HMODULE hDwmapi = LoadLibraryA("dwmapi.dll");
    if (hDwmapi) {
        _DwmEnableBlurBehindWindow = (pfnDwmEnableBlurBehindWindow)GetProcAddress(hDwmapi, "DwmEnableBlurBehindWindow");
        return _DwmEnableBlurBehindWindow != NULL;
    }
    return FALSE;
}

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

void SetBlurBehind(HWND hwnd, BOOL enable);

#define CLOCK_IDC_EDIT               108
#define CLOCK_IDC_BUTTON_OK          109
#define CLOCK_IDD_DIALOG1            1002
#define CLOCK_ID_TRAY_APP_ICON       1001
#define CLOCK_IDC_CUSTOMIZE_LEFT     112
#define CLOCK_IDC_EDIT_MODE          113
#define CLOCK_IDC_MODIFY_OPTIONS     114

#define CLOCK_IDM_TIMEOUT_ACTION     120
#define CLOCK_IDM_SHOW_MESSAGE       121
#define CLOCK_IDM_LOCK_SCREEN        122
#define CLOCK_IDM_SHUTDOWN           123
#define CLOCK_IDM_RESTART            124
#define CLOCK_IDM_OPEN_FILE          125

#define CLOCK_IDC_FONT_MENU           113

#define CLOCK_WM_TRAYICON (WM_USER + 2)

#define MAX_TIME_OPTIONS 10
int time_options[MAX_TIME_OPTIONS];
int time_options_count = 0;

char CLOCK_TEXT_COLOR[10] = "#FFFFFF";
int CLOCK_BASE_WINDOW_WIDTH = 200;
int CLOCK_BASE_WINDOW_HEIGHT = 100;
float CLOCK_WINDOW_SCALE = 1.0f;
int CLOCK_BASE_FONT_SIZE = 24;
float CLOCK_FONT_SCALE_FACTOR = 1.0f;
int CLOCK_DEFAULT_START_TIME = 300;
int CLOCK_WINDOW_POS_X = 100;
int CLOCK_WINDOW_POS_Y = 100;

BOOL CLOCK_EDIT_MODE = FALSE;
BOOL CLOCK_IS_DRAGGING = FALSE;
POINT CLOCK_LAST_MOUSE_POS = {0, 0};

RECT CLOCK_TEXT_RECT = {0, 0, 0, 0};
BOOL CLOCK_TEXT_RECT_VALID = FALSE;

BOOL OpenFileDialog(HWND hwnd, char* filePath, DWORD maxPath);

typedef enum {
    TIMEOUT_ACTION_MESSAGE = 0,
    TIMEOUT_ACTION_LOCK = 1,
    TIMEOUT_ACTION_SHUTDOWN = 2,
    TIMEOUT_ACTION_RESTART = 3,
    TIMEOUT_ACTION_OPEN_FILE = 4   
} TimeoutActionType;

TimeoutActionType CLOCK_TIMEOUT_ACTION;

char inputText[256] = {0};
static int elapsed_time = 0;
static int CLOCK_TOTAL_TIME = 0;
NOTIFYICONDATA nid;
time_t last_config_time = 0;
int message_shown = 0;
char CLOCK_TIMEOUT_TEXT[50] = "";
char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH] = "";   

char FONT_FILE_NAME[100] = "Hack Nerd Font.ttf";

char FONT_INTERNAL_NAME[100];

typedef struct {
    int menuId;
    int resourceId;
    const char* fontName;
} FontResource;

FontResource fontResources[] = {
    {CLOCK_IDC_FONT_VICTORMONO, IDR_FONT_VICTORMONO, "VictorMono NFP Medium.ttf"},
    {CLOCK_IDC_FONT_LIBERATION, IDR_FONT_LIBERATION, "LiterationSerif Nerd Font.ttf"},
    {CLOCK_IDC_FONT_ZEDMONO, IDR_FONT_ZEDMONO, "ZedMono NF.ttf"},
    {CLOCK_IDC_FONT_RECMONO, IDR_FONT_RECMONO, "RecMonoCasual Nerd Font Mono.ttf"},
    {CLOCK_IDC_FONT_IOSEVKA_TERM, IDR_FONT_IOSEVKA_TERM, "IosevkaTermSlab NFP Medium.ttf"},
    {CLOCK_IDC_FONT_ENVYCODE, IDR_FONT_ENVYCODE, "EnvyCodeR Nerd Font.ttf"},
    {CLOCK_IDC_FONT_DADDYTIME, IDR_FONT_DADDYTIME, "DaddyTimeMono Nerd Font Propo.ttf"},
    {CLOCK_IDC_FONT_PROFONT, IDR_FONT_PROFONT, "ProFont IIx Nerd Font.ttf"},
    {CLOCK_IDC_FONT_HEAVYDATA, IDR_FONT_HEAVYDATA, "HeavyData Nerd Font.ttf"},
    {CLOCK_IDC_FONT_BIGBLUE, IDR_FONT_BIGBLUE, "BigBlueTermPlus Nerd Font.ttf"},
    {CLOCK_IDC_FONT_PROGGYCLEAN, IDR_FONT_PROGGYCLEAN, "ProggyCleanSZ Nerd Font Propo.ttf"},
    {CLOCK_IDC_FONT_DEPARTURE, IDR_FONT_DEPARTURE, "DepartureMono Nerd Font Propo.ttf"},
    {CLOCK_IDC_FONT_TERMINESS, IDR_FONT_TERMINESS, "Terminess Nerd Font Propo.ttf"},
    {CLOCK_IDC_FONT_GOHUFONT, IDR_FONT_GOHUFONT, "GohuFont uni11 Nerd Font Mono.ttf"}
};

BOOL LoadFontFromResource(HINSTANCE hInstance, int resourceId) {
    HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
    if (hResource == NULL) {
        return FALSE;
    }

    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (hMemory == NULL) {
        return FALSE;
    }

    void* fontData = LockResource(hMemory);
    if (fontData == NULL) {
        return FALSE;
    }

    DWORD fontLength = SizeofResource(hInstance, hResource);
    DWORD nFonts = 0;
    HANDLE handle = AddFontMemResourceEx(fontData, fontLength, NULL, &nFonts);
    return handle != NULL;
}

BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName) {
    for (int i = 0; i < sizeof(fontResources) / sizeof(FontResource); i++) {
        if (strcmp(fontResources[i].fontName, fontName) == 0) {
            return LoadFontFromResource(hInstance, fontResources[i].resourceId);
        }
    }
    return FALSE;
}


void LoadRecentFiles(void);
void SaveRecentFile(const char* filePath);

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
void ReadConfig();
void GetConfigPath(char* path, size_t size);
void SaveWindowSettings(HWND hwnd);
void LoadWindowSettings(HWND hwnd);
void WriteConfigColor(const char* color_input);
void WriteConfigFont(const char* font_filename);
void WriteConfigTimeoutAction(const char* action);
void WriteConfigEditMode(const char* mode);
void WriteConfigTimeOptions(const char* options);   
void FormatTime(int remaining_time, char* time_text);
void ExitProgram(HWND hwnd);
void ShowContextMenu(HWND hwnd);
void ShowColorMenu(HWND hwnd);
void ListAvailableFonts();
void SetBlurBehind(HWND hwnd, BOOL enable);
void AdjustWindowPosition(HWND hwnd);
int isValidInput(const char* input);
int ParseInput(const char* input, int* total_seconds);
int isValidColor(const char* input);
INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
int CALLBACK EnumFontFamExProc(
    const LOGFONT *lpelfe,
    const TEXTMETRIC *lpntme,
    DWORD FontType,
    LPARAM lParam
);

#define MAX_RECENT_FILES 5   
typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
} RecentFile;

RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];
int CLOCK_RECENT_FILES_COUNT = 0;

#define CLOCK_ABOUT_URL "https://github.com/vladelaina/Catime"
#define CLOCK_IDM_ABOUT 130   

char PREVIEW_FONT_NAME[100] = "";   
char PREVIEW_INTERNAL_NAME[100] = "";     
BOOL IS_PREVIEWING = FALSE;         

char PREVIEW_COLOR[10] = "";   
BOOL IS_COLOR_PREVIEWING = FALSE;   

#define WM_USER_SHELLICON WM_USER + 1

void ShowToastNotification(HWND hwnd, const char* message);

 
BOOL CLOCK_SHOW_CURRENT_TIME = FALSE;
time_t CLOCK_LAST_TIME_UPDATE = 0;
BOOL CLOCK_USE_24HOUR = TRUE;
BOOL CLOCK_SHOW_SECONDS = TRUE;

 
#define CLOCK_IDM_SHOW_CURRENT_TIME 150
#define CLOCK_IDM_24HOUR_FORMAT    151
#define CLOCK_IDM_SHOW_SECONDS     152

// 修改 UTF8ToANSI 函数的实现
char* UTF8ToANSI(const char* utf8Str) {
    // 先获取需要的缓冲区大小
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    if (wlen == 0) {
        return _strdup(utf8Str); // 如果转换失败，返回原始字符串的副本
    }

    // 分配 UTF-16 缓冲区
    wchar_t* wstr = (wchar_t*)malloc(sizeof(wchar_t) * wlen);
    if (!wstr) {
        return _strdup(utf8Str);
    }

    // 转换到 UTF-16
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wstr, wlen) == 0) {
        free(wstr);
        return _strdup(utf8Str);
    }

    // 获取 ANSI 所需的缓冲区大小
    int len = WideCharToMultiByte(936, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (len == 0) {
        free(wstr);
        return _strdup(utf8Str);
    }

    // 分配 ANSI 缓冲区
    char* str = (char*)malloc(len);
    if (!str) {
        free(wstr);
        return _strdup(utf8Str);
    }

    // 转换到 ANSI (GB2312/GBK)
    if (WideCharToMultiByte(936, 0, wstr, -1, str, len, NULL, NULL) == 0) {
        free(wstr);
        free(str);
        return _strdup(utf8Str);
    }

    free(wstr);
    return str;
}

void InitializeDefaultLanguage(void) {
    LANGID langId = GetUserDefaultUILanguage();
    WORD primaryLangId = PRIMARYLANGID(langId);
    WORD subLangId = SUBLANGID(langId);
    
    switch (primaryLangId) {
        case LANG_CHINESE:
            if (subLangId == SUBLANG_CHINESE_SIMPLIFIED) {
                CURRENT_LANGUAGE = APP_LANG_CHINESE_SIMP;
            } else if (subLangId == SUBLANG_CHINESE_TRADITIONAL) {
                CURRENT_LANGUAGE = APP_LANG_CHINESE_TRAD;
            }
            break;
        case LANG_SPANISH:
            CURRENT_LANGUAGE = APP_LANG_SPANISH;
            break;
        case LANG_FRENCH:
            CURRENT_LANGUAGE = APP_LANG_FRENCH;
            break;
        case LANG_GERMAN:
            CURRENT_LANGUAGE = APP_LANG_GERMAN;
            break;
        case LANG_RUSSIAN:
            CURRENT_LANGUAGE = APP_LANG_RUSSIAN;
            break;
        case LANG_PORTUGUESE:
            CURRENT_LANGUAGE = APP_LANG_PORTUGUESE;
            break;
        case LANG_JAPANESE:
            CURRENT_LANGUAGE = APP_LANG_JAPANESE;
            break;
        case LANG_KOREAN:
            CURRENT_LANGUAGE = APP_LANG_KOREAN;
            break;
        default:
            CURRENT_LANGUAGE = APP_LANG_ENGLISH;
            break;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 在程序启动时调用初始化语言函数
    InitializeDefaultLanguage();
    
    // 设置代码页为 GBK
    SetConsoleOutputCP(936);
    SetConsoleCP(936);
    
    ReadConfig();

    int defaultFontIndex = -1;
    for (int i = 0; i < sizeof(fontResources) / sizeof(FontResource); i++) {
        if (strcmp(fontResources[i].fontName, FONT_FILE_NAME) == 0) {
            defaultFontIndex = i;
            break;
        }
    }
    
    if (defaultFontIndex != -1) {
        if (!LoadFontFromResource(hInstance, fontResources[defaultFontIndex].resourceId)) {
            char errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: %s", fontResources[defaultFontIndex].fontName);
            MessageBox(NULL, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
        }
    }

    CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;

    HANDLE hMutex = CreateMutex(NULL, TRUE, "CatimeMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hwndExisting = FindWindow("CatimeWindow", "Catime");
        if (hwndExisting) {
            SendMessage(hwndExisting, WM_CLOSE, 0, 0);
        }
        Sleep(50);
    }

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInstance;
    wc.lpszClassName = "CatimeWindow";
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

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
        return 0;
    }

    EnableWindow(hwnd, TRUE);
    SetFocus(hwnd);

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

    SetBlurBehind(hwnd, FALSE);

    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CATIME));
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
    strcpy(nid.szTip, "Catime");
    Shell_NotifyIcon(NIM_ADD, &nid);

    if (SetTimer(hwnd, 1, 1000, NULL) == 0) {
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}

void GetConfigPath(char* path, size_t size) {
    if (!path || size == 0) return;

    char* appdata_path = getenv("LOCALAPPDATA");
    if (appdata_path) {
        if (snprintf(path, size, "%s\\Catime\\config.txt", appdata_path) >= size) {
            strncpy(path, ".\\asset\\config.txt", size - 1);
            path[size - 1] = '\0';
            return;
        }
        
        char dir_path[MAX_PATH];
        if (snprintf(dir_path, sizeof(dir_path), "%s\\Catime", appdata_path) < sizeof(dir_path)) {
            if (!CreateDirectoryA(dir_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                strncpy(path, ".\\asset\\config.txt", size - 1);
                path[size - 1] = '\0';
            }
        }
    } else {
        strncpy(path, ".\\asset\\config.txt", size - 1);
        path[size - 1] = '\0';
    }
}

void CreateDefaultConfig(const char* config_path) {
    FILE *file = fopen(config_path, "r");
    if (file) {
        fclose(file);
        return;   
    }

    file = fopen(config_path, "w");
    if (!file) {
        fprintf(stderr, "Failed to create config file: %s\n", config_path);
        return;
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int centerX = screenWidth / 2;
    
    float scale = (screenHeight * 0.03f) / 20.0f;

    fprintf(file, "CLOCK_TEXT_COLOR=#FFB6C1\n");
    fprintf(file, "CLOCK_BASE_FONT_SIZE=20\n");
    fprintf(file, "FONT_FILE_NAME=GohuFont uni11 Nerd Font Mono.ttf\n");
    fprintf(file, "CLOCK_WINDOW_POS_X=%d\n", centerX);
    fprintf(file, "CLOCK_WINDOW_POS_Y=-7\n");   
    fprintf(file, "WINDOW_SCALE=%.2f\n", scale);
    fprintf(file, "CLOCK_DEFAULT_START_TIME=1500\n");
    fprintf(file, "CLOCK_TIME_OPTIONS=25,10,5\n");
    fprintf(file, "CLOCK_TIMEOUT_TEXT=0\n");
    fprintf(file, "CLOCK_EDIT_MODE=FALSE\n");
    fprintf(file, "CLOCK_TIMEOUT_ACTION=LOCK\n");

    fclose(file);
}

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

void ReadConfig() {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *file = fopen(config_path, "r");
    if (!file) {
        CreateDefaultConfig(config_path);
        file = fopen(config_path, "r");
        if (!file) {
            fprintf(stderr, "Failed to open config file after creation: %s\n", config_path);
            return;
        }
    }

    time_options_count = 0;
    memset(time_options, 0, sizeof(time_options));

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        if (strncmp(line, "CLOCK_TIME_OPTIONS=", 19) == 0) {
            char *token = strtok(line + 19, ",");
            while (token && time_options_count < MAX_TIME_OPTIONS) {
                while (*token == ' ') token++;
                time_options[time_options_count++] = atoi(token);
                token = strtok(NULL, ",");
            }
        }
        else if (strncmp(line, "FONT_FILE_NAME=", 15) == 0) {
            strncpy(FONT_FILE_NAME, line + 15, sizeof(FONT_FILE_NAME) - 1);
            FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
            
            size_t name_len = strlen(FONT_FILE_NAME);
            if (name_len > 4 && strcmp(FONT_FILE_NAME + name_len - 4, ".ttf") == 0) {
                strncpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, name_len - 4);
                FONT_INTERNAL_NAME[name_len - 4] = '\0';
            } else {
                strncpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
                FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
            }
        }
        else if (strncmp(line, "CLOCK_TEXT_COLOR=", 17) == 0) {
            strncpy(CLOCK_TEXT_COLOR, line + 17, sizeof(CLOCK_TEXT_COLOR) - 1);
            CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
        }
        else if (strncmp(line, "CLOCK_DEFAULT_START_TIME=", 25) == 0) {
            sscanf(line + 25, "%d", &CLOCK_DEFAULT_START_TIME);
        }
        else if (strncmp(line, "CLOCK_WINDOW_POS_X=", 19) == 0) {
            sscanf(line + 19, "%d", &CLOCK_WINDOW_POS_X);
        }
        else if (strncmp(line, "CLOCK_WINDOW_POS_Y=", 19) == 0) {
            sscanf(line + 19, "%d", &CLOCK_WINDOW_POS_Y);
        }
        else if (strncmp(line, "CLOCK_TIMEOUT_TEXT=", 19) == 0) {
            sscanf(line + 19, "%49[^\n]", CLOCK_TIMEOUT_TEXT);
        }
        else if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 20) == 0) {
            char action[8] = {0};
            sscanf(line + 20, "%7s", action);
            if (strcmp(action, "MESSAGE") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
            } else if (strcmp(action, "LOCK") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
            } else if (strcmp(action, "SHUTDOWN") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHUTDOWN;
            } else if (strcmp(action, "RESTART") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_RESTART;
            } else if (strcmp(action, "OPEN_FILE") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
            }
        }
        else if (strncmp(line, "CLOCK_EDIT_MODE=", 15) == 0) {
            char edit_mode[8] = {0};
            sscanf(line + 15, "%7s", edit_mode);
            if (strcmp(edit_mode, "TRUE") == 0) {
                CLOCK_EDIT_MODE = TRUE;
            } else if (strcmp(edit_mode, "FALSE") == 0) {
                CLOCK_EDIT_MODE = FALSE;
            }
        }
        else if (strncmp(line, "WINDOW_SCALE=", 13) == 0) {
            CLOCK_WINDOW_SCALE = atof(line + 13);
        }
        else if (strncmp(line, "CLOCK_TIMEOUT_FILE=", 19) == 0) {
            char *path = line + 19;
            char *newline = strchr(path, '\n');
            if (newline) *newline = '\0';
            
            // 清理路径
            while (*path == '=' || *path == ' ' || *path == '"') path++;
            size_t len = strlen(path);
            if (len > 0 && path[len-1] == '"') path[len-1] = '\0';
            
            // 验证文件是否存在
            if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES) {
                strncpy(CLOCK_TIMEOUT_FILE_PATH, path, sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
                CLOCK_TIMEOUT_FILE_PATH[sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1] = '\0';
                
                // 确保动作设置正确
                if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
                }
            } else {
                // 如果文件不存在，清空路径
                memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE; // 默认回退到消息提示
            }
        }
    }

    fclose(file);
    last_config_time = time(NULL);

    HWND hwnd = FindWindow("CatimeWindow", "Catime");
    if (hwnd) {
        SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        InvalidateRect(hwnd, NULL, TRUE);
    }

    LoadRecentFiles();   
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

void WriteConfigFont(const char* font_filename) {
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
        if (strncmp(line, "FONT_FILE_NAME=", 15) == 0) {
            strcat(new_config, "FONT_FILE_NAME=");
            strcat(new_config, font_filename);
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

void WriteConfigTimeoutAction(const char* action) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    FILE* temp = fopen(temp_path, "w");
    FILE* file = fopen(config_path, "r");
    
    if (!temp || !file) {
        if (temp) fclose(temp);
        if (file) fclose(file);
        return;
    }

    char line[MAX_PATH];
    int success = 1;

    // 读取所有非超时相关的配置
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 20) != 0 && 
            strncmp(line, "CLOCK_TIMEOUT_FILE=", 19) != 0) {
            if (fputs(line, temp) == EOF) {
                success = 0;
                break;
            }
        }
    }

    // 写入新的超时动作配置
    if (success) {
        if (fprintf(temp, "CLOCK_TIMEOUT_ACTION=%s\n", action) < 0) {
            success = 0;
        }
    }
    
    // 如果动作是打开文件且文件路径存在，写入文件路径
    if (success && strcmp(action, "OPEN_FILE") == 0 && strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        // 验证文件是否存在
        if (GetFileAttributes(CLOCK_TIMEOUT_FILE_PATH) != INVALID_FILE_ATTRIBUTES) {
            if (fprintf(temp, "CLOCK_TIMEOUT_FILE=%s\n", CLOCK_TIMEOUT_FILE_PATH) < 0) {
                success = 0;
            }
        }
    }

    fclose(file);
    fclose(temp);

    if (success) {
        remove(config_path);
        rename(temp_path, config_path);
    } else {
        remove(temp_path);
    }
}

void WriteConfigEditMode(const char* mode) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    char temp_path[MAX_PATH];
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    FILE *file, *temp_file;
    char line[256];
    int found = 0;
    
    file = fopen(config_path, "r");
    temp_file = fopen(temp_path, "w");
    
    if (!file || !temp_file) {
        if (file) fclose(file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_EDIT_MODE=", 15) == 0) {
            fprintf(temp_file, "CLOCK_EDIT_MODE=%s\n", mode);
            found = 1;
        } else {
            fputs(line, temp_file);
        }
    }
    
    if (!found) {
        fprintf(temp_file, "CLOCK_EDIT_MODE=%s\n", mode);
    }
    
    fclose(file);
    fclose(temp_file);
    
    remove(config_path);
    rename(temp_path, config_path);
}

void WriteConfigTimeOptions(const char* options) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    FILE *file, *temp_file;
    char line[256];
    int found = 0;
    
    file = fopen(config_path, "r");
    temp_file = fopen(temp_path, "w");
    
    if (!file || !temp_file) {
        if (file) fclose(file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_TIME_OPTIONS=", 19) == 0) {
            fprintf(temp_file, "CLOCK_TIME_OPTIONS=%s\n", options);
            found = 1;
        } else {
            fputs(line, temp_file);
        }
    }
    
    if (!found) {
        fprintf(temp_file, "CLOCK_TIME_OPTIONS=%s\n", options);
    }
    
    fclose(file);
    fclose(temp_file);
    
    remove(config_path);
    rename(temp_path, config_path);
}

int isValidInput(const char* input) {
    if (input == NULL || strlen(input) == 0) return 0;

    BOOL hasDigit = FALSE;
    for (size_t i = 0; i < strlen(input); i++) {
        char c = tolower((unsigned char)input[i]);
        if (isdigit((unsigned char)c)) {
            hasDigit = TRUE;
        } else if (c != 'h' && c != 'm' && c != 's' && !isspace((unsigned char)c)) {
            return 0;
        }
    }

    return hasDigit;   
}

int ParseInput(const char* input, int* total_seconds) {
    if (!isValidInput(input)) return 0;

    int hours = 0, minutes = 0, seconds = 0;
    char input_copy[256];
    strncpy(input_copy, input, sizeof(input_copy)-1);
    input_copy[sizeof(input_copy)-1] = '\0';

    char *tokens[3] = {0};
    int token_count = 0;

    char *token = strtok(input_copy, " ");
    while (token && token_count < 3) {
        tokens[token_count++] = token;
        token = strtok(NULL, " ");
    }

    if (token_count == 1) {
        char unit = tolower((unsigned char)tokens[0][strlen(tokens[0]) - 1]);
        if (unit == 'h' || unit == 'm' || unit == 's') {
            tokens[0][strlen(tokens[0]) - 1] = '\0';   
            int value = atoi(tokens[0]);
            switch (unit) {
                case 'h': hours = value; break;
                case 'm': minutes = value; break;
                case 's': seconds = value; break;
            }
        } else {
            minutes = atoi(tokens[0]);
        }
    } else if (token_count == 2) {
        char unit = tolower((unsigned char)tokens[1][strlen(tokens[1]) - 1]);
        if (unit == 'h' || unit == 'm' || unit == 's') {
            tokens[1][strlen(tokens[1]) - 1] = '\0';   
            int value1 = atoi(tokens[0]);
            int value2 = atoi(tokens[1]);
            switch (unit) {
                case 'h': 
                    minutes = value1;
                    hours = value2;
                    break;
                case 'm': 
                    hours = value1;
                    minutes = value2;
                    break;
                case 's':
                    minutes = value1;
                    seconds = value2;
                    break;
            }
        } else {
            minutes = atoi(tokens[0]);
            seconds = atoi(tokens[1]);
        }
    } else if (token_count == 3) {
        hours = atoi(tokens[0]);
        minutes = atoi(tokens[1]);
        seconds = atoi(tokens[2]);
    }

    *total_seconds = hours * 3600 + minutes * 60 + seconds;
    if (*total_seconds <= 0) return 0;

    if (hours < 0 || hours > 99 ||     
        minutes < 0 || minutes > 59 ||  
        seconds < 0 || seconds > 59) {  
        return 0;
    }

    if (hours > INT_MAX/3600 || 
        (*total_seconds) > INT_MAX) {
        return 0;
    }

    return 1;
}

INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
            SendMessage(hwndDlg, DM_SETDEFID, CLOCK_IDC_BUTTON_OK, 0);
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));
            return FALSE;  
        }

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkColor(hdcStatic, RGB(0xF3, 0xF3, 0xF3));
            if (!hBackgroundBrush) {
                hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            }
            return (INT_PTR)hBackgroundBrush;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetBkColor(hdcEdit, RGB(0xFF, 0xFF, 0xFF));
            if (!hEditBrush) {
                hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            }
            return (INT_PTR)hEditBrush;
        }

        case WM_CTLCOLORBTN: {
            HDC hdcBtn = (HDC)wParam;
            SetBkColor(hdcBtn, RGB(0xFD, 0xFD, 0xFD));
            if (!hButtonBrush) {
                hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));
            }
            return (INT_PTR)hButtonBrush;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, inputText, sizeof(inputText));
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                return TRUE;
            }
            break;

        case WM_DESTROY:
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hEditBrush) {
                DeleteObject(hEditBrush);
                hEditBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }
            break;
    }
    return FALSE;
}

void FormatTime(int remaining_time, char* time_text) {
    if (CLOCK_SHOW_CURRENT_TIME) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        int hour = tm_info->tm_hour;
        
        if (!CLOCK_USE_24HOUR) {
            if (hour == 0) {
                hour = 12;
            } else if (hour > 12) {
                hour -= 12;
            } else if (hour == 12) {
                 
            }
        }

        if (CLOCK_SHOW_SECONDS) {
            sprintf(time_text, "%d:%02d:%02d", 
                    hour, tm_info->tm_min, tm_info->tm_sec);
        } else {
            sprintf(time_text, "%d:%02d", 
                    hour, tm_info->tm_min);
        }
        return;
    }

    int hours = remaining_time / 3600;
    int minutes = (remaining_time % 3600) / 60;
    int seconds = remaining_time % 60;

    if (hours > 0) {
        sprintf(time_text, "%d:%02d:%02d", hours, minutes, seconds);
    } else if (minutes > 0) {
        if (minutes >= 10) {
            sprintf(time_text, "    %d:%02d", minutes, seconds);   
        } else {
            sprintf(time_text, "    %d:%02d", minutes, seconds);   
        }
    } else {
        if (seconds < 10) {
            sprintf(time_text, "          %d", seconds);   
        } else {
            sprintf(time_text, "        %d", seconds);   
        }
    }
}

void ExitProgram(HWND hwnd) {
    Shell_NotifyIcon(NIM_DELETE, &nid);

    PostQuitMessage(0);
}

void ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    
    AppendMenuW(hMenu, MF_STRING, 101, 
                GetLocalizedString(L"设置时间", L"Set Time"));
    
    HMENU hTimeMenu = CreatePopupMenu();
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_SHOW_CURRENT_TIME ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHOW_CURRENT_TIME,
               GetLocalizedString(L"显示当前时间", L"Show Current Time"));
    AppendMenuW(hTimeMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_USE_24HOUR ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_24HOUR_FORMAT,
               GetLocalizedString(L"24小时制", L"24-Hour Format"));
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_SHOW_SECONDS ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHOW_SECONDS,
               GetLocalizedString(L"显示秒数", L"Show Seconds"));
    
    AppendMenuW(hMenu, MF_POPUP | (CLOCK_SHOW_CURRENT_TIME ? MF_CHECKED : MF_UNCHECKED), 
               (UINT_PTR)hTimeMenu,
               GetLocalizedString(L"时间显示", L"Time Display"));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    for (int i = 0; i < time_options_count; i++) {
        wchar_t menu_item[20];
        _snwprintf(menu_item, sizeof(menu_item)/sizeof(wchar_t), L"%d", time_options[i]);
        AppendMenuW(hMenu, MF_STRING, 102 + i, menu_item);
    }

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

void ShowColorMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    HMENU hColorSubMenu = CreatePopupMenu();
    HMENU hFontSubMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING | (CLOCK_EDIT_MODE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDC_EDIT_MODE, 
               GetLocalizedString(L"编辑模式", L"Edit Mode"));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    HMENU hTimeoutMenu = CreatePopupMenu();
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHOW_MESSAGE, 
               GetLocalizedString(L"显示消息", L"Show Message"));

    HMENU hOpenFileMenu = CreatePopupMenu();
    if (CLOCK_RECENT_FILES_COUNT > 0) {
        for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
            BOOL isCurrentFile = (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && 
                                strcmp(CLOCK_RECENT_FILES[i].path, CLOCK_TIMEOUT_FILE_PATH) == 0);
            
            // 转换文件名到宽字符
            wchar_t wFileName[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[i].name, -1, wFileName, MAX_PATH);
            
            AppendMenuW(hOpenFileMenu, MF_STRING | (isCurrentFile ? MF_CHECKED : MF_UNCHECKED), 
                      CLOCK_IDM_RECENT_FILE_1 + i, 
                      wFileName);  // 使用转换后的宽字符文件名
        }
        AppendMenuW(hOpenFileMenu, MF_SEPARATOR, 0, NULL);
    }
    AppendMenuW(hOpenFileMenu, MF_STRING, CLOCK_IDM_BROWSE_FILE, 
                GetLocalizedString(L"浏览...", L"Browse..."));

    const wchar_t* menuText = GetLocalizedString(L"打开文件", L"Open File");
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        static wchar_t displayText[MAX_PATH];
        char *filename = strrchr(CLOCK_TIMEOUT_FILE_PATH, '\\');
        if (filename) {
            filename++;
            // 转换文件名到宽字符
            wchar_t wFileName[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, filename, -1, wFileName, MAX_PATH);
            
            _snwprintf(displayText, MAX_PATH, 
                      GetLocalizedString(L"打开: %ls", L"Open: %ls"), 
                      wFileName);  // 使用%ls而不是%hs，并使用转换后的宽字符文件名
            menuText = displayText;
        }
    }

    AppendMenuW(hTimeoutMenu, MF_POPUP | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE ? MF_CHECKED : MF_UNCHECKED),
               (UINT_PTR)hOpenFileMenu, menuText);
               
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_LOCK ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_LOCK_SCREEN, 
               GetLocalizedString(L"锁定屏幕", L"Lock Screen"));
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHUTDOWN, 
               GetLocalizedString(L"关机", L"Shutdown"));
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_RESTART, 
               GetLocalizedString(L"重启", L"Restart"));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeoutMenu, 
                GetLocalizedString(L"超时动作", L"Timeout Action"));
    AppendMenuW(hMenu, MF_STRING, CLOCK_IDC_MODIFY_OPTIONS, 
                GetLocalizedString(L"修改时间选项", L"Modify Time Options"));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // 颜色选项保持不变，因为是用颜色块显示的
    for (int i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        const char* hexColor = COLOR_OPTIONS[i].hexColor;
        
        MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
        mii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE | MIIM_FTYPE;
        mii.fType = MFT_STRING | MFT_OWNERDRAW;
        mii.fState = strcmp(CLOCK_TEXT_COLOR, hexColor) == 0 ? MFS_CHECKED : MFS_UNCHECKED;
        mii.wID = 201 + i;
        mii.dwTypeData = (LPSTR)hexColor;
        
        InsertMenuItem(hColorSubMenu, i, TRUE, &mii);
    }
    AppendMenuW(hColorSubMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hColorSubMenu, MF_STRING, CLOCK_IDC_CUSTOMIZE_LEFT, 
                GetLocalizedString(L"自定义", L"Customize"));

    // 字体菜单项
    for (int i = 0; i < sizeof(fontResources) / sizeof(fontResources[0]); i++) {
        BOOL isCurrentFont = strcmp(FONT_FILE_NAME, fontResources[i].fontName) == 0;
        
        char displayName[100];
        strncpy(displayName, fontResources[i].fontName, sizeof(displayName) - 1);
        displayName[sizeof(displayName) - 1] = '\0';
        
        char* dot = strstr(displayName, ".ttf");
        if (dot) {
            *dot = '\0';
        }
        
        AppendMenu(hFontSubMenu, MF_STRING | (isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                  fontResources[i].menuId, displayName);
    }

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hColorSubMenu, 
                GetLocalizedString(L"颜色", L"Color"));
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu, 
                GetLocalizedString(L"字体", L"Font"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    HMENU hAboutMenu = CreatePopupMenu();
    wchar_t version_text[32];
    _snwprintf(version_text, sizeof(version_text)/sizeof(wchar_t), 
               GetLocalizedString(L"当前版本: %hs", L"Version: %hs"), 
               CATIME_VERSION);
    AppendMenuW(hAboutMenu, MF_STRING | MF_DISABLED, 0, version_text);

    // 创建反馈子菜单
    HMENU hFeedbackMenu = CreatePopupMenu();
    AppendMenuW(hFeedbackMenu, MF_STRING, CLOCK_IDM_FEEDBACK_GITHUB, L"GitHub");
    AppendMenuW(hFeedbackMenu, MF_STRING, CLOCK_IDM_FEEDBACK_BILIBILI, L"BiliBili");
    AppendMenuW(hAboutMenu, MF_POPUP, (UINT_PTR)hFeedbackMenu, 
                GetLocalizedString(L"反馈", L"Feedback"));

    // 添加语言选择子菜单
    HMENU hLangMenu = CreatePopupMenu();
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_CHINESE, L"简体中文");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_CHINESE_TRAD ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_CHINESE_TRAD, L"繁體中文");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_ENGLISH ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_ENGLISH, L"English");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_SPANISH ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_SPANISH, L"Español");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_FRENCH ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_FRENCH, L"Français");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_GERMAN ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_GERMAN, L"Deutsch");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_RUSSIAN ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_RUSSIAN, L"Русский");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_PORTUGUESE ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_PORTUGUESE, L"Português");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_JAPANESE ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_JAPANESE, L"日本語");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_KOREAN ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_KOREAN, L"한국어");

    AppendMenuW(hAboutMenu, MF_POPUP, (UINT_PTR)hLangMenu, GetLocalizedString(L"语言", L"Language"));

    HMENU hUpdateMenu = CreatePopupMenu();
    AppendMenuW(hUpdateMenu, MF_STRING, CLOCK_IDM_UPDATE_GITHUB, L"GitHub");
    AppendMenuW(hUpdateMenu, MF_STRING, CLOCK_IDM_UPDATE_123PAN,
                GetLocalizedString(L"123云盘", L"123Pan"));
    AppendMenuW(hUpdateMenu, MF_STRING, CLOCK_IDM_UPDATE_LANZOU,
                GetLocalizedString(L"蓝奏云 (密码: 1234)", L"LanzouCloud (pwd: 1234)"));

    AppendMenuW(hAboutMenu, MF_POPUP, (UINT_PTR)hUpdateMenu,
                GetLocalizedString(L"检查更新", L"Check for Updates"));
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAboutMenu,
                GetLocalizedString(L"关于", L"About"));
    AppendMenuW(hMenu, MF_STRING, 200,
                GetLocalizedString(L"重置", L"Reset"));
    AppendMenuW(hMenu, MF_STRING, 109,
                GetLocalizedString(L"退出", L"Exit"));

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static char time_text[50];
    UINT uID;
    UINT uMouseMsg;

    switch(msg)
    {
        case WM_CREATE: {
            HWND hwndParent = GetParent(hwnd);
            if (hwndParent != NULL) {
                EnableWindow(hwndParent, TRUE);
            }
            LoadWindowSettings(hwnd);
            SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
            AdjustWindowPosition(hwnd);
            break;
        }

        case WM_LBUTTONDOWN: {
            if (CLOCK_EDIT_MODE) {
                CLOCK_IS_DRAGGING = TRUE;
                SetCapture(hwnd);
                GetCursorPos(&CLOCK_LAST_MOUSE_POS);
                return 0;
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (CLOCK_EDIT_MODE && CLOCK_IS_DRAGGING) {
                CLOCK_IS_DRAGGING = FALSE;
                ReleaseCapture();
                AdjustWindowPosition(hwnd);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }

        case WM_MOUSEWHEEL: {
            if (CLOCK_EDIT_MODE) {
                int delta = GET_WHEEL_DELTA_WPARAM(wp);
                float old_scale = CLOCK_FONT_SCALE_FACTOR;
                
                 
                POINT mousePos;
                GetCursorPos(&mousePos);
                
                 
                RECT windowRect;
                GetWindowRect(hwnd, &windowRect);
                int oldWidth = windowRect.right - windowRect.left;
                int oldHeight = windowRect.bottom - windowRect.top;
                
                 
                float relativeX = (float)(mousePos.x - windowRect.left) / oldWidth;
                float relativeY = (float)(mousePos.y - windowRect.top) / oldHeight;
                
                 
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
                    
                     
                    int newX = mousePos.x - (int)(relativeX * newWidth);
                    int newY = mousePos.y - (int)(relativeY * newHeight);
                    
                     
                    SetWindowPos(hwnd, NULL, 
                        newX, newY,
                        newWidth, newHeight,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
                    
                     
                    static UINT_PTR timerId = 0;
                    if (timerId) {
                        KillTimer(hwnd, timerId);
                    }
                    timerId = SetTimer(hwnd, 3, 200, NULL);   
                    
                     
                    InvalidateRect(hwnd, NULL, FALSE);
                    UpdateWindow(hwnd);
                }
            }
            break;
        }

        case WM_MOUSEMOVE: {
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
                
                return 0;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            SetGraphicsMode(memDC, GM_ADVANCED);
            SetBkMode(memDC, TRANSPARENT);
            SetStretchBltMode(memDC, HALFTONE);
            SetBrushOrgEx(memDC, 0, 0, NULL);

            int remaining_time = CLOCK_TOTAL_TIME - elapsed_time;
            if (elapsed_time >= CLOCK_TOTAL_TIME) {
                if (strcmp(CLOCK_TIMEOUT_TEXT, "0") == 0) {
                    time_text[0] = '\0';
                } else if (strlen(CLOCK_TIMEOUT_TEXT) > 0) {
                    strncpy(time_text, CLOCK_TIMEOUT_TEXT, sizeof(time_text) - 1);
                    time_text[sizeof(time_text) - 1] = '\0';
                } else {
                    time_text[0] = '\0';
                }
            } else {
                FormatTime(remaining_time, time_text);
            }

            const char* fontToUse = IS_PREVIEWING ? PREVIEW_FONT_NAME : FONT_FILE_NAME;
            HFONT hFont = CreateFont(
                -CLOCK_BASE_FONT_SIZE * CLOCK_FONT_SCALE_FACTOR,
                0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_TT_PRECIS,
                CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,   
                VARIABLE_PITCH | FF_SWISS,
                IS_PREVIEWING ? PREVIEW_INTERNAL_NAME : FONT_INTERNAL_NAME
            );
            HFONT oldFont = (HFONT)SelectObject(memDC, hFont);

            SetTextAlign(memDC, TA_LEFT | TA_TOP);
            SetTextCharacterExtra(memDC, 0);
            SetMapMode(memDC, MM_TEXT);

            DWORD quality = SetICMMode(memDC, ICM_ON);
            SetLayout(memDC, 0);

            int r = 255, g = 255, b = 255;
            const char* colorToUse = IS_COLOR_PREVIEWING ? PREVIEW_COLOR : CLOCK_TEXT_COLOR;
            
            if (strlen(colorToUse) > 0) {
                if (colorToUse[0] == '#') {
                    if (strlen(colorToUse) == 7) {
                        sscanf(colorToUse + 1, "%02x%02x%02x", &r, &g, &b);
                    }
                } else {
                    sscanf(colorToUse, "%d,%d,%d", &r, &g, &b);
                }
            }
            SetTextColor(memDC, RGB(r, g, b));

            if (CLOCK_EDIT_MODE) {
                HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(memDC, &rect, hBrush);
                DeleteObject(hBrush);
            } else {
                HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(memDC, &rect, hBrush);
                DeleteObject(hBrush);
            }

            if (strlen(time_text) > 0) {
                SIZE textSize;
                GetTextExtentPoint32(memDC, time_text, strlen(time_text), &textSize);

                if (textSize.cx != (rect.right - rect.left) || 
                    textSize.cy != (rect.bottom - rect.top)) {
                    RECT windowRect;
                    GetWindowRect(hwnd, &windowRect);
                    SetWindowPos(hwnd, NULL,
                        windowRect.left, windowRect.top,
                        textSize.cx, textSize.cy,
                        SWP_NOZORDER | SWP_NOACTIVATE);
                    GetClientRect(hwnd, &rect);
                }

                int x = (rect.right - textSize.cx) / 2;
                int y = (rect.bottom - textSize.cy) / 2;

                SetTextColor(memDC, RGB(r, g, b));
                
                for (int i = 0; i < 8; i++) {
                    TextOutA(memDC, x, y, time_text, strlen(time_text));
                }
            }

            BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldFont);
            DeleteObject(hFont);
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            break;
        }
        case WM_TIMER: {
            if (wp == 1) {
                if (CLOCK_SHOW_CURRENT_TIME) {
                    time_t current_time = time(NULL);
                    if (current_time != CLOCK_LAST_TIME_UPDATE) {
                        CLOCK_LAST_TIME_UPDATE = current_time;
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                } else {
                    if (elapsed_time < CLOCK_TOTAL_TIME && !message_shown) {
                        elapsed_time++;
                        InvalidateRect(hwnd, NULL, TRUE);
                    } else if (elapsed_time >= CLOCK_TOTAL_TIME && !message_shown) {
                        message_shown = 1;
                        KillTimer(hwnd, 1);
                        PauseMediaPlayback();

                        switch (CLOCK_TIMEOUT_ACTION) {
                            case TIMEOUT_ACTION_MESSAGE:
                                ShowToastNotification(hwnd, "Time's up!");
                                break;
                            case TIMEOUT_ACTION_LOCK:
                                LockWorkStation();
                                break;
                            case TIMEOUT_ACTION_SHUTDOWN:
                                system("shutdown /s /t 0");  // Changed from 60 to 0
                                break;
                            case TIMEOUT_ACTION_RESTART:
                                system("shutdown /r /t 0");  // Changed from 60 to 0
                                break;
                            case TIMEOUT_ACTION_OPEN_FILE: {
                                if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
                                    // 转换为 Unicode 路径
                                    wchar_t wPath[MAX_PATH];
                                    MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_FILE_PATH, -1, wPath, MAX_PATH);
                                    
                                    // 使用 ShellExecuteW 打开文件
                                    HINSTANCE result = ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
                                    
                                    // 检查是否成功打开文件
                                    if ((INT_PTR)result <= 32) {
                                        // 如果打开失败，显示错误消息
                                        MessageBoxW(hwnd, 
                                            GetLocalizedString(L"无法打开文件", L"Failed to open file"),
                                            GetLocalizedString(L"错误", L"Error"),
                                            MB_ICONERROR);
                                    }
                                }
                                break;
                            }
                        }
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            break;
        }
        case WM_DESTROY: {
            ExitProgram(hwnd);
            break;
        }
        case CLOCK_WM_TRAYICON: {
            uID = (UINT)wp;
            uMouseMsg = (UINT)lp;

            if (uMouseMsg == WM_RBUTTONUP) {
                ShowColorMenu(hwnd);
            }
            else if (uMouseMsg == WM_LBUTTONUP) {
                ShowContextMenu(hwnd);
            }
            break;
        }
        case WM_COMMAND: {
            WORD cmd = LOWORD(wp);
            switch (cmd) {
                case 101: {   
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        CLOCK_LAST_TIME_UPDATE = 0;
                        KillTimer(hwnd, 1);  // 添加这行，确保先关闭已有计时器
                    }
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_DIALOG1), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            break;
                        }

                        int total_seconds = 0;
                        if (ParseInput(inputText, &total_seconds)) {
                            KillTimer(hwnd, 1);  // 添加这行，确保先关闭已有计时器
                            CLOCK_TOTAL_TIME = total_seconds;
                            elapsed_time = 0;
                            message_shown = 0;
                            InvalidateRect(hwnd, NULL, TRUE);
                            SetTimer(hwnd, 1, 1000, NULL);
                            break;
                        } else {
                            MessageBoxW(hwnd, 
                                GetLocalizedString(
                                    L"25    = 25分钟\n"
                                    L"25h   = 25小时\n"
                                    L"25s   = 25秒\n"
                                    L"25 30 = 25分钟30秒\n"
                                    L"25 30m = 25小时30分钟\n"
                                    L"1 30 20 = 1小时30分钟20秒",
                                    
                                    L"25    = 25 minutes\n"
                                    L"25h   = 25 hours\n"
                                    L"25s   = 25 seconds\n"
                                    L"25 30 = 25 minutes 30 seconds\n"
                                    L"25 30m = 25 hours 30 minutes\n"
                                    L"1 30 20 = 1 hour 30 minutes 20 seconds"),
                                GetLocalizedString(L"输入格式", L"Input Format"),
                                MB_OK);
                        }
                    }
                    break;
                }
                case CLOCK_IDC_MODIFY_OPTIONS: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_DIALOG1), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            break;
                        }

                        char* token = strtok(inputText, " ");
                        char options[256] = {0};
                        int valid = 1;
                        int count = 0;
                        
                        while (token && count < MAX_TIME_OPTIONS) {
                            int num = atoi(token);
                            if (num <= 0) {
                                valid = 0;
                                break;
                            }
                            
                            if (count > 0) {
                                strcat(options, ",");
                            }
                            strcat(options, token);
                            count++;
                            token = strtok(NULL, " ");
                        }

                        if (valid && count > 0) {
                            WriteConfigTimeOptions(options);
                            ReadConfig();
                            break;
                        } else {
                            MessageBoxW(hwnd,
                                GetLocalizedString(
                                    L"请输入用空格分隔的数字\n"
                                    L"例如: 25 10 5",
                                    L"Enter numbers separated by spaces\n"
                                    L"Example: 25 10 5"),
                                GetLocalizedString(L"无效输入", L"Invalid Input"), 
                                MB_OK);
                        }
                    }
                    break;
                }
                case 200: {   
                    int current_elapsed = elapsed_time;
                    int current_total = CLOCK_TOTAL_TIME;
                    BOOL was_timing = (current_elapsed < current_total);
                    
                    CLOCK_EDIT_MODE = FALSE;
                    SetClickThrough(hwnd, TRUE);
                    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
                    
                    // Add this line to clear timeout file path
                    memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                    
                    // 检查并重置语言到系统默认
                    AppLanguage defaultLanguage;
                    LANGID langId = GetUserDefaultUILanguage();
                    WORD primaryLangId = PRIMARYLANGID(langId);
                    WORD subLangId = SUBLANGID(langId);
                    
                    // 根据系统语言设置默认语言
                    switch (primaryLangId) {
                        case LANG_CHINESE:
                            defaultLanguage = (subLangId == SUBLANG_CHINESE_SIMPLIFIED) ? 
                                             APP_LANG_CHINESE_SIMP : APP_LANG_CHINESE_TRAD;
                            break;
                        case LANG_SPANISH:
                            defaultLanguage = APP_LANG_SPANISH;
                            break;
                        case LANG_FRENCH:
                            defaultLanguage = APP_LANG_FRENCH;
                            break;
                        case LANG_GERMAN:
                            defaultLanguage = APP_LANG_GERMAN;
                            break;
                        case LANG_RUSSIAN:
                            defaultLanguage = APP_LANG_RUSSIAN;
                            break;
                        case LANG_PORTUGUESE:
                            defaultLanguage = APP_LANG_PORTUGUESE;
                            break;
                        case LANG_JAPANESE:
                            defaultLanguage = APP_LANG_JAPANESE;
                            break;
                        case LANG_KOREAN:
                            defaultLanguage = APP_LANG_KOREAN;
                            break;
                        default:
                            defaultLanguage = APP_LANG_ENGLISH;
                            break;
                    }
                    
                    // 如果当前语言不是系统默认语言，则重置
                    if (CURRENT_LANGUAGE != defaultLanguage) {
                        CURRENT_LANGUAGE = defaultLanguage;
                    }
                    
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    remove(config_path);   
                    CreateDefaultConfig(config_path);   
                    
                    ReadConfig();   
                    
                    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                    for (int i = 0; i < sizeof(fontResources) / sizeof(FontResource); i++) {
                        if (strcmp(fontResources[i].fontName, "GohuFont uni11 Nerd Font Mono.ttf") == 0) {
                            LoadFontFromResource(hInstance, fontResources[i].resourceId);
                            break;
                        }
                    }
                    
                    if (was_timing) {
                        elapsed_time = current_elapsed;
                        CLOCK_TOTAL_TIME = current_total;
                    }
                    
                    CLOCK_WINDOW_SCALE = 1.0f;
                    CLOCK_FONT_SCALE_FACTOR = 1.0f;
                    
                    HDC hdc = GetDC(hwnd);
                    HFONT hFont = CreateFont(
                        -CLOCK_BASE_FONT_SIZE,   
                        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, FONT_INTERNAL_NAME
                    );
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    char time_text[50];
                    FormatTime(CLOCK_TOTAL_TIME, time_text);
                    SIZE textSize;
                    GetTextExtentPoint32(hdc, time_text, strlen(time_text), &textSize);
                    
                    SelectObject(hdc, hOldFont);
                    DeleteObject(hFont);
                    ReleaseDC(hwnd, hdc);
                    
                    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                    
                    float defaultScale = (screenHeight * 0.03f) / 20.0f;
                    CLOCK_WINDOW_SCALE = defaultScale;
                    CLOCK_FONT_SCALE_FACTOR = defaultScale;
                    
                    
                    SetWindowPos(hwnd, NULL, 
                        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
                        textSize.cx * defaultScale, textSize.cy * defaultScale,
                        SWP_NOZORDER | SWP_NOACTIVATE
                    );
                    
                    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
                    RedrawWindow(hwnd, NULL, NULL, 
                        RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
                    
                    break;
                }
                case CLOCK_IDM_CHECK_UPDATE: {
                    ShellExecuteA(NULL, "open", UPDATE_URL_GITHUB, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_UPDATE_GITHUB: {
                    ShellExecuteA(NULL, "open", UPDATE_URL_GITHUB, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_UPDATE_123PAN: {
                    ShellExecuteA(NULL, "open", UPDATE_URL_123PAN, NULL, NULL, SW_SHOWNORMAL);   
                    break;
                }
                case CLOCK_IDM_UPDATE_LANZOU: {
                    ShellExecuteA(NULL, "open", UPDATE_URL_LANZOU, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_FEEDBACK: {
                    ShellExecuteA(NULL, "open", FEEDBACK_URL, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_LANG_CHINESE: {
                    CURRENT_LANGUAGE = APP_LANG_CHINESE_SIMP;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_CHINESE_TRAD: {
                    CURRENT_LANGUAGE = APP_LANG_CHINESE_TRAD;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_ENGLISH: {
                    CURRENT_LANGUAGE = APP_LANG_ENGLISH;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_SPANISH: {
                    CURRENT_LANGUAGE = APP_LANG_SPANISH;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_FRENCH: {
                    CURRENT_LANGUAGE = APP_LANG_FRENCH;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_GERMAN: {
                    CURRENT_LANGUAGE = APP_LANG_GERMAN;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_RUSSIAN: {
                    CURRENT_LANGUAGE = APP_LANG_RUSSIAN;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_PORTUGUESE: {
                    CURRENT_LANGUAGE = APP_LANG_PORTUGUESE;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_JAPANESE: {
                    CURRENT_LANGUAGE = APP_LANG_JAPANESE;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_KOREAN: {
                    CURRENT_LANGUAGE = APP_LANG_KOREAN;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                default: {
                    int cmd = LOWORD(wp);
                    if (cmd >= 102 && cmd < 102 + time_options_count) {
                         
                        if (CLOCK_SHOW_CURRENT_TIME) {
                            CLOCK_SHOW_CURRENT_TIME = FALSE;
                            CLOCK_LAST_TIME_UPDATE = 0;
                        }
                        
                         
                        int index = cmd - 102;
                        CLOCK_TOTAL_TIME = time_options[index] * 60;
                        elapsed_time = 0;
                        message_shown = 0;
                        InvalidateRect(hwnd, NULL, TRUE);
                        SetTimer(hwnd, 1, 1000, NULL);
                        break;
                    }
                    
                    if (cmd >= 201 && cmd < 201 + COLOR_OPTIONS_COUNT) {
                        int colorIndex = cmd - 201;
                        const char* hexColor = COLOR_OPTIONS[colorIndex].hexColor;
                        WriteConfigColor(hexColor);
                        goto refresh_window;
                    }

                    if (cmd == 109) {
                        ExitProgram(hwnd);
                        break;
                    }

                    if (cmd == CLOCK_IDM_SHOW_MESSAGE) {
                        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                        WriteConfigTimeoutAction("MESSAGE");
                        break;
                    }
                    else if (cmd == CLOCK_IDM_LOCK_SCREEN) {
                        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
                        WriteConfigTimeoutAction("LOCK");
                        break;
                    }
                    else if (cmd == CLOCK_IDM_SHUTDOWN) {
                        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHUTDOWN;
                        WriteConfigTimeoutAction("SHUTDOWN");
                        break;
                    }
                    else if (cmd == CLOCK_IDM_RESTART) {
                        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_RESTART;
                        WriteConfigTimeoutAction("RESTART");
                        break;
                    }
                    else if (cmd == CLOCK_IDM_OPEN_FILE) {
                        char filePath[MAX_PATH] = "";
                        if (OpenFileDialog(hwnd, filePath, MAX_PATH)) {
                            strncpy(CLOCK_TIMEOUT_FILE_PATH, filePath, sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
                            CLOCK_TIMEOUT_FILE_PATH[sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1] = '\0';
                            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
                            WriteConfigTimeoutAction("OPEN_FILE");
                        }
                        break;
                    }
                    else if (cmd == CLOCK_IDM_BROWSE_FILE) {
                        char filePath[MAX_PATH] = "";
                        if (OpenFileDialog(hwnd, filePath, MAX_PATH)) {
                            strncpy(CLOCK_TIMEOUT_FILE_PATH, filePath, sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
                            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
                            WriteConfigTimeoutAction("OPEN_FILE");
                            SaveRecentFile(filePath);
                        }
                        break;
                    }
                    else if (cmd >= CLOCK_IDM_RECENT_FILE_1 && cmd <= CLOCK_IDM_RECENT_FILE_3) {
                        int index = cmd - CLOCK_IDM_RECENT_FILE_1;
                        if (index < CLOCK_RECENT_FILES_COUNT) {
                            // 将路径转换为宽字符
                            wchar_t wPath[MAX_PATH];
                            MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[index].path, -1, wPath, MAX_PATH);
                            
                            if (GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES) {
                                // 更新超时文件路径
                                strncpy(CLOCK_TIMEOUT_FILE_PATH, CLOCK_RECENT_FILES[index].path, 
                                        sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
                                CLOCK_TIMEOUT_FILE_PATH[sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1] = '\0';
                                
                                // 设置动作为打开文件
                                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
                                
                                // 保存配置
                                WriteConfigTimeoutAction("OPEN_FILE");
                                
                                // 将选中的文件移到最近文件列表的首位
                                SaveRecentFile(CLOCK_RECENT_FILES[index].path);
                                
                                // 重新加载配置以确保所有设置都正确
                                ReadConfig();
                            } else {
                                // 如果文件不存在，显示错误消息
                                MessageBoxW(hwnd, 
                                    GetLocalizedString(L"所选文件不存在", L"Selected file does not exist"),
                                    GetLocalizedString(L"错误", L"Error"),
                                    MB_ICONERROR);
                                
                                // 清除无效的文件路径
                                memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                                WriteConfigTimeoutAction("MESSAGE");
                                
                                // 从最近文件列表中移除无效文件
                                for (int i = index; i < CLOCK_RECENT_FILES_COUNT - 1; i++) {
                                    CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i + 1];
                                }
                                CLOCK_RECENT_FILES_COUNT--;
                            }
                        }
                        break;
                    }
                }
                case CLOCK_IDC_EDIT_MODE: {
                    CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
                    WriteConfigEditMode(CLOCK_EDIT_MODE ? "TRUE" : "FALSE");
                    
                    if (CLOCK_EDIT_MODE) {
                        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
                        SetBlurBehind(hwnd, TRUE);
                    } else {
                        SetBlurBehind(hwnd, FALSE);
                        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
                    }
                    
                    SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDC_CUSTOMIZE_LEFT: {
                    COLORREF color = ShowColorDialog(hwnd);
                    if (color != (COLORREF)-1) {
                        char hex_color[10];
                        snprintf(hex_color, sizeof(hex_color), "#%02X%02X%02X", 
                                GetRValue(color), GetGValue(color), GetBValue(color));
                        WriteConfigColor(hex_color);
                        ReadConfig();
                    }
                    break;
                }
                case CLOCK_IDC_FONT_VICTORMONO:
                    WriteConfigFont("VictorMono NFP Medium.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "VictorMono NFP Medium.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "VictorMono NFP Medium.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_LIBERATION:
                    WriteConfigFont("LiterationSerif Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "LiterationSerif Nerd Font.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "LiterationSerif Nerd Font.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_ZEDMONO:
                    WriteConfigFont("ZedMono NF.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "ZedMono NF.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "ZedMono NF.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_RECMONO:
                    WriteConfigFont("RecMonoCasual Nerd Font Mono.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "RecMonoCasual Nerd Font Mono.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "RecMonoCasual Nerd Font Mono.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_IOSEVKA_TERM:
                    WriteConfigFont("IosevkaTermSlab NFP Medium.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "IosevkaTermSlab NFP Medium.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "IosevkaTermSlab NFP Medium.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_ENVYCODE:
                    WriteConfigFont("EnvyCodeR Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "EnvyCodeR Nerd Font.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "EnvyCodeR Nerd Font.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_DADDYTIME:
                    WriteConfigFont("DaddyTimeMono Nerd Font Propo.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "DaddyTimeMono Nerd Font Propo.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "DaddyTimeMono Nerd Font Propo.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_PROFONT:
                    WriteConfigFont("ProFont IIx Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "ProFont IIx Nerd Font.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "ProFont IIx Nerd Font.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_HEAVYDATA:
                    WriteConfigFont("HeavyData Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "HeavyData Nerd Font.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "HeavyData Nerd Font.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_BIGBLUE:
                    WriteConfigFont("BigBlueTermPlus Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "BigBlueTermPlus Nerd Font.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "BigBlueTermPlus Nerd Font.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_PROGGYCLEAN:
                    WriteConfigFont("ProggyCleanSZ Nerd Font Propo.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "ProggyCleanSZ Nerd Font Propo.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "ProggyCleanSZ Nerd Font Propo.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_DEPARTURE:
                    WriteConfigFont("DepartureMono Nerd Font Propo.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "DepartureMono Nerd Font Propo.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "DepartureMono Nerd Font Propo.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_TERMINESS:
                    WriteConfigFont("Terminess Nerd Font Propo.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Terminess Nerd Font Propo.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "Terminess Nerd Font Propo.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_GOHUFONT:
                    WriteConfigFont("GohuFont uni11 Nerd Font Mono.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "GohuFont uni11 Nerd Font Mono.ttf")) {
                        wchar_t errorMsg[256];
                        _snwprintf(errorMsg, sizeof(errorMsg)/sizeof(wchar_t),
                            GetLocalizedString(L"无法加载字体: %hs", L"Failed to load font: %hs"),
                            "GohuFont uni11 Nerd Font Mono.ttf");
                        MessageBoxW(hwnd, errorMsg, 
                            GetLocalizedString(L"错误", L"Error"),
                            MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDM_SHOW_CURRENT_TIME: {  
                    CLOCK_SHOW_CURRENT_TIME = !CLOCK_SHOW_CURRENT_TIME;
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        KillTimer(hwnd, 1);   
                        elapsed_time = 0;
                        CLOCK_LAST_TIME_UPDATE = time(NULL);
                        SetTimer(hwnd, 1, 1000, NULL);   
                    } else {
                        KillTimer(hwnd, 1);   
                        elapsed_time = CLOCK_TOTAL_TIME;   
                        message_shown = 1;   
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_24HOUR_FORMAT: {  
                    CLOCK_USE_24HOUR = !CLOCK_USE_24HOUR;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_SHOW_SECONDS: {  
                    CLOCK_SHOW_SECONDS = !CLOCK_SHOW_SECONDS;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_FEEDBACK_GITHUB: {
                    ShellExecuteA(NULL, "open", FEEDBACK_URL_GITHUB, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_FEEDBACK_BILIBILI: {
                    ShellExecuteA(NULL, "open", FEEDBACK_URL_BILIBILI, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_RECENT_FILE_1:
                case CLOCK_IDM_RECENT_FILE_2:
                case CLOCK_IDM_RECENT_FILE_3: {
                    int index = cmd - CLOCK_IDM_RECENT_FILE_1;
                    if (index < CLOCK_RECENT_FILES_COUNT) {
                        // 将路径转换为宽字符
                        wchar_t wPath[MAX_PATH];
                        MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[index].path, -1, wPath, MAX_PATH);
                        
                        if (GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES) {
                            // 更新超时文件路径
                            strncpy(CLOCK_TIMEOUT_FILE_PATH, CLOCK_RECENT_FILES[index].path, 
                                    sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
                            CLOCK_TIMEOUT_FILE_PATH[sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1] = '\0';
                            
                            // 设置动作为打开文件
                            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
                            
                            // 保存配置
                            WriteConfigTimeoutAction("OPEN_FILE");
                            
                            // 将选中的文件移到最近文件列表的首位
                            SaveRecentFile(CLOCK_RECENT_FILES[index].path);
                            
                            // 重新加载配置以确保所有设置都正确
                            ReadConfig();
                        } else {
                            // 如果文件不存在，显示错误消息
                            MessageBoxW(hwnd, 
                                GetLocalizedString(L"所选文件不存在", L"Selected file does not exist"),
                                GetLocalizedString(L"错误", L"Error"),
                                MB_ICONERROR);
                            
                            // 清除无效的文件路径
                            memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                            WriteConfigTimeoutAction("MESSAGE");
                            
                            // 从最近文件列表中移除无效文件
                            for (int i = index; i < CLOCK_RECENT_FILES_COUNT - 1; i++) {
                                CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i + 1];
                            }
                            CLOCK_RECENT_FILES_COUNT--;
                        }
                    }
                    break;
                }
                case CLOCK_IDC_TIMEOUT_BROWSE: {
                    OPENFILENAMEW ofn;
                    wchar_t szFile[MAX_PATH] = L"";
                    
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    if (GetOpenFileNameW(&ofn)) {
                        // 将 Unicode 路径转换为 UTF-8
                        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, 
                                           CLOCK_TIMEOUT_FILE_PATH, 
                                           sizeof(CLOCK_TIMEOUT_FILE_PATH), 
                                           NULL, NULL);
                        
                        // 更新配置文件
                        char config_path[MAX_PATH];
                        GetConfigPath(config_path, MAX_PATH);
                        WriteConfigTimeoutAction("OPEN_FILE");  // 使用 WriteConfigTimeoutAction 而不是 WriteConfig
                        SaveRecentFile(CLOCK_TIMEOUT_FILE_PATH);  // 保存到最近文件列表
                    }
                    break;
                }
            }
            break;

refresh_window:
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        case WM_WINDOWPOSCHANGED: {
            if (CLOCK_EDIT_MODE) {
                SaveWindowSettings(hwnd);
            }
            break;
        }
        case WM_RBUTTONUP: {
            if (CLOCK_EDIT_MODE) {
                CLOCK_EDIT_MODE = FALSE;
                WriteConfigEditMode("FALSE");
                
                SetBlurBehind(hwnd, FALSE);
                SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
                
                SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
                
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            break;
        }
        case WM_MEASUREITEM:
        {
            LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lp;
            if (lpmis->CtlType == ODT_MENU) {
                lpmis->itemHeight = 25;
                lpmis->itemWidth = 100;
                return TRUE;
            }
            return FALSE;
        }
        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lp;
            if (lpdis->CtlType == ODT_MENU) {
                int colorIndex = lpdis->itemID - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    const char* hexColor = COLOR_OPTIONS[colorIndex].hexColor;
                    int r, g, b;
                    sscanf(hexColor + 1, "%02x%02x%02x", &r, &g, &b);
                    
                    HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
                    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
                    
                    HGDIOBJ oldBrush = SelectObject(lpdis->hDC, hBrush);
                    HGDIOBJ oldPen = SelectObject(lpdis->hDC, hPen);
                    
                    Rectangle(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top,
                             lpdis->rcItem.right, lpdis->rcItem.bottom);
                    
                    SelectObject(lpdis->hDC, oldPen);
                    SelectObject(lpdis->hDC, oldBrush);
                    DeleteObject(hPen);
                    DeleteObject(hBrush);
                    
                    if (lpdis->itemState & ODS_SELECTED) {
                        DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
                    }
                    
                    return TRUE;
                }
            }
            return FALSE;
        }
        case WM_MENUSELECT: {
            UINT menuItem = LOWORD(wp);
            UINT flags = HIWORD(wp);
            HMENU hMenu = (HMENU)lp;

            if (!(flags & MF_POPUP) && hMenu != NULL) {
                int colorIndex = menuItem - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    strncpy(PREVIEW_COLOR, COLOR_OPTIONS[colorIndex].hexColor, sizeof(PREVIEW_COLOR) - 1);
                    PREVIEW_COLOR[sizeof(PREVIEW_COLOR) - 1] = '\0';
                    IS_COLOR_PREVIEWING = TRUE;
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }

                for (int i = 0; i < sizeof(fontResources) / sizeof(FontResource); i++) {
                    if (fontResources[i].menuId == menuItem) {
                        strncpy(PREVIEW_FONT_NAME, fontResources[i].fontName, sizeof(PREVIEW_FONT_NAME) - 1);
                        PREVIEW_FONT_NAME[sizeof(PREVIEW_FONT_NAME) - 1] = '\0';
                        
                        strncpy(PREVIEW_INTERNAL_NAME, PREVIEW_FONT_NAME, sizeof(PREVIEW_INTERNAL_NAME) - 1);
                        PREVIEW_INTERNAL_NAME[sizeof(PREVIEW_INTERNAL_NAME) - 1] = '\0';
                        char* dot = strrchr(PREVIEW_INTERNAL_NAME, '.');
                        if (dot) *dot = '\0';
                        
                        LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), 
                                     fontResources[i].fontName);
                        
                        IS_PREVIEWING = TRUE;
                        InvalidateRect(hwnd, NULL, TRUE);
                        return 0;
                    }
                }
                
                if (IS_PREVIEWING || IS_COLOR_PREVIEWING) {
                    IS_PREVIEWING = FALSE;
                    IS_COLOR_PREVIEWING = FALSE;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (flags & MF_POPUP) {
                if (IS_PREVIEWING || IS_COLOR_PREVIEWING) {
                    IS_PREVIEWING = FALSE;
                    IS_COLOR_PREVIEWING = FALSE;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            break;
        }
        case WM_EXITMENULOOP: {
            if (IS_PREVIEWING || IS_COLOR_PREVIEWING) {
                IS_PREVIEWING = FALSE;
                IS_COLOR_PREVIEWING = FALSE;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

void AdjustWindowPosition(HWND hwnd) {
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;
    
    POINT windowCenter;
    windowCenter.x = windowRect.left + windowWidth / 2;
    windowCenter.y = windowRect.top + windowHeight / 2;
    
    HMONITOR hMonitor = MonitorFromPoint(windowCenter, MONITOR_DEFAULTTONEAREST);
    
    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hMonitor, &monitorInfo);
    
    RECT workArea = monitorInfo.rcWork;
    
    int newX = windowRect.left;
    int newY = windowRect.top;
    
    int maxOutside = windowWidth / 2;
    if (newX < workArea.left - maxOutside) {
        newX = workArea.left - maxOutside;
    }
    else if (newX + windowWidth > workArea.right + maxOutside) {
        newX = workArea.right + maxOutside - windowWidth;
    }
    
    maxOutside = windowHeight / 2;
    if (newY < workArea.top - maxOutside) {
        newY = workArea.top - maxOutside;
    }
    else if (newY + windowHeight > workArea.bottom + maxOutside) {
        newY = workArea.bottom + maxOutside - windowHeight;
    }
    
    if (newX != windowRect.left || newY != windowRect.top) {
        SetWindowPos(hwnd, NULL, 
            newX, newY,
            windowWidth, windowHeight,
            SWP_NOZORDER | SWP_NOACTIVATE
        );
    }
}

void ListAvailableFonts() {
    HDC hdc = GetDC(NULL);
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfCharSet = DEFAULT_CHARSET;

    HFONT hFont = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             lf.lfCharSet, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, NULL);
    SelectObject(hdc, hFont);

    EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC)EnumFontFamExProc, 0, 0);

    DeleteObject(hFont);
    ReleaseDC(NULL, hdc);
}

int CALLBACK EnumFontFamExProc(
  const LOGFONT *lpelfe,
  const TEXTMETRIC *lpntme,
  DWORD FontType,
  LPARAM lParam
) {
    return 1;
}

int isValidColor(const char* input) {
    if (input == NULL) return 0;
    
    char temp[20];
    strncpy(temp, input, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    for (char *p = temp; *p; p++) {
        *p = tolower(*p);
    }

    const char* ptr = temp;
    if (*ptr == '#') ptr++;

    if (strlen(ptr) == 6) {
        for (int i = 0; i < 6; i++) {
            if (!isxdigit((unsigned char)ptr[i])) return 0;
        }
        return 1;
    }

    int r, g, b;
    if (sscanf(temp, "%d,%d,%d", &r, &g, &b) == 3) {
        if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
            return 1;
        }
    }

    return 0;
}

INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, inputText, sizeof(inputText));
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

typedef struct _ACCENTPOLICY {
    int nAccentState;
    int nFlags;
    int nColor;
    int nAnimationId;
} ACCENTPOLICY;

typedef struct _WINCOMPATTR {
    int nAttribute;
    PVOID pData;
    ULONG ulDataSize;
} WINCOMPATTR;

#define ACCENT_DISABLED 0
#define ACCENT_ENABLE_BLURBEHIND 3
#define WCA_ACCENT_POLICY 19

void SetBlurBehind(HWND hwnd, BOOL enable) {
    if (!hwnd) return;

    static BOOL dwmInitialized = FALSE;
    static BOOL dwmAvailable = FALSE;
    
    if (!dwmInitialized) {
        dwmInitialized = TRUE;
        dwmAvailable = InitDWMFunctions();
    }
    
    BOOL success = FALSE;
    
    if (dwmAvailable && _DwmEnableBlurBehindWindow) {
        if (enable) {
            DWM_BLURBEHIND bb = {0};
            bb.dwFlags = DWM_BB_ENABLE;
            bb.fEnable = TRUE;
            bb.hRgnBlur = NULL;
            
            HRESULT hr = _DwmEnableBlurBehindWindow(hwnd, &bb);
            if (SUCCEEDED(hr)) {
                success = TRUE;
                SetLayeredWindowAttributes(hwnd, 0, BLUR_OPACITY, LWA_ALPHA);
            }
        } else {
            DWM_BLURBEHIND bb = {0};
            bb.dwFlags = DWM_BB_ENABLE;
            bb.fEnable = FALSE;
            
            HRESULT hr = _DwmEnableBlurBehindWindow(hwnd, &bb);
            if (SUCCEEDED(hr)) {
                success = TRUE;
                SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
            }
        }
    }
    
    if (!success) {
        if (enable) {
            SetLayeredWindowAttributes(hwnd, 0, BLUR_OPACITY, LWA_ALPHA);
        } else {
            SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
        }
    }
}

void PauseMediaPlayback(void) {
     
    keybd_event(VK_MEDIA_STOP, 0, 0, 0);
    Sleep(50);   
    keybd_event(VK_MEDIA_STOP, 0, KEYEVENTF_KEYUP, 0);
    Sleep(50);   

     
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
    Sleep(50);   
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
    Sleep(50);   

     
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
    Sleep(50);
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
    Sleep(100);   
}

BOOL OpenFileDialog(HWND hwnd, char* filePath, DWORD maxPath) {
    OPENFILENAMEW ofn = {0};
    wchar_t szFile[MAX_PATH] = L"";
    
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"All Files\0*.*\0"
                      L"Audio Files\0*.mp3;*.wav;*.m4a;*.wma\0"
                      L"Video Files\0*.mp4;*.avi;*.mkv;*.wmv\0"
                      L"Applications\0*.exe\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        // 将 Unicode 路径转换为 UTF-8
        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, 
                           filePath, maxPath, NULL, NULL);
        return TRUE;
    }
    return FALSE;
}


void LoadRecentFiles(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *file = fopen(config_path, "r");
    if (!file) return;
    
    char line[MAX_PATH];
    CLOCK_RECENT_FILES_COUNT = 0;
    
    while (fgets(line, sizeof(line), file) && CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
        if (strncmp(line, "CLOCK_RECENT_FILE=", 17) == 0) {
            char *path = line + 17;
            char *newline = strchr(path, '\n');
            if (newline) *newline = '\0';
            
            // 清理路径
            while (*path == '=' || *path == ' ' || *path == '"') path++;
            size_t len = strlen(path);
            while (len > 0 && (path[len-1] == ' ' || path[len-1] == '"' || path[len-1] == '\n' || path[len-1] == '\r')) {
                path[--len] = '\0';
            }
            
            // 转换为宽字符以验证文件是否存在
            wchar_t wPath[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH);
            
            if (GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES) {
                strncpy(CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path, path, MAX_PATH - 1);
                CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path[MAX_PATH - 1] = '\0';
                
                // 获取文件名
                wchar_t* wFilename = wcsrchr(wPath, L'\\');
                if (wFilename) {
                    wFilename++;  // 跳过反斜杠
                    // 将文件名转换回UTF-8
                    WideCharToMultiByte(CP_UTF8, 0, wFilename, -1,
                                      CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].name,
                                      MAX_PATH, NULL, NULL);
                } else {
                    WideCharToMultiByte(CP_UTF8, 0, wPath, -1,
                                      CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].name,
                                      MAX_PATH, NULL, NULL);
                }
                
                CLOCK_RECENT_FILES_COUNT++;
            }
        }
    }
    
    fclose(file);
    
    // 检查当前超时文件是否存在
    if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        wchar_t wTimeoutPath[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_FILE_PATH, -1, wTimeoutPath, MAX_PATH);
        if (GetFileAttributesW(wTimeoutPath) == INVALID_FILE_ATTRIBUTES) {
            memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
            WriteConfigTimeoutAction("MESSAGE");
        }
    }
}

void SaveRecentFile(const char* filePath) {
    // 先转换为宽字符进行比较
    wchar_t wFilePath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH);
    
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        wchar_t wExistingPath[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[i].path, -1, wExistingPath, MAX_PATH);
        
        if (wcscmp(wExistingPath, wFilePath) == 0) {
            RecentFile temp = CLOCK_RECENT_FILES[i];
            for (int j = i; j > 0; j--) {
                CLOCK_RECENT_FILES[j] = CLOCK_RECENT_FILES[j-1];
            }
            CLOCK_RECENT_FILES[0] = temp;
            return;
        }
    }
    
    if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
        CLOCK_RECENT_FILES_COUNT++;
    }
    for (int i = CLOCK_RECENT_FILES_COUNT - 1; i > 0; i--) {
        CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i-1];
    }
    
    strncpy(CLOCK_RECENT_FILES[0].path, filePath, MAX_PATH - 1);
    CLOCK_RECENT_FILES[0].path[MAX_PATH - 1] = '\0';
    
    // 使用宽字符处理文件名
    wchar_t* wFilename = wcsrchr(wFilePath, L'\\');
    if (wFilename) {
        wFilename++;  
        WideCharToMultiByte(CP_UTF8, 0, wFilename, -1,
                           CLOCK_RECENT_FILES[0].name,
                           MAX_PATH, NULL, NULL);
    } else {
        WideCharToMultiByte(CP_UTF8, 0, wFilePath, -1,
                           CLOCK_RECENT_FILES[0].name,
                           MAX_PATH, NULL, NULL);
    }
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *file = fopen(config_path, "r");
    if (!file) return;
    
    char *config_content = NULL;
    long file_size;
    
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    config_content = (char *)malloc(file_size + MAX_RECENT_FILES * (MAX_PATH + 20));
    if (!config_content) {
        fclose(file);
        return;
    }
    
    size_t bytes_read = fread(config_content, 1, file_size, file);
    config_content[bytes_read] = '\0';
    fclose(file);
    
    char *new_config = (char *)malloc(strlen(config_content) + MAX_RECENT_FILES * (MAX_PATH + 20));
    if (!new_config) {
        free(config_content);
        return;
    }
    new_config[0] = '\0';
    
    char *line = strtok(config_content, "\n");
    while (line) {
        // 保留所有非文件相关的配置
        if (strncmp(line, "CLOCK_RECENT_FILE", 16) != 0 && 
            strncmp(line, "CLOCK_TIMEOUT_FILE", 17) != 0 &&
            strncmp(line, "CLOCK_TIMEOUT_ACTION", 19) != 0) {  // 也排除 action 配置
            strcat(new_config, line);
            strcat(new_config, "\n");
        }
        line = strtok(NULL, "\n");
    }
    
    // 添加最近文件记录
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        char recent_file_line[MAX_PATH + 20];
        snprintf(recent_file_line, sizeof(recent_file_line), 
                "CLOCK_RECENT_FILE=%s\n", CLOCK_RECENT_FILES[i].path);
        strcat(new_config, recent_file_line);
    }

    // 添加超时文件路径（如果存在）
    if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        // 先写入动作配置
        strcat(new_config, "CLOCK_TIMEOUT_ACTION=OPEN_FILE\n");
        
        // 然后写入文件路径，确保没有多余的等号
        char timeout_file_line[MAX_PATH + 20];
        char clean_path[MAX_PATH];
        strncpy(clean_path, CLOCK_TIMEOUT_FILE_PATH, MAX_PATH - 1);
        clean_path[MAX_PATH - 1] = '\0';
        
        // 移除路径中可能存在的等号
        char* p = clean_path;
        while (*p == '=' || *p == ' ') p++;
        
        snprintf(timeout_file_line, sizeof(timeout_file_line),
                "CLOCK_TIMEOUT_FILE=%s\n", p);
        strcat(new_config, timeout_file_line);
    }
    
    file = fopen(config_path, "w");
    if (file) {
        fputs(new_config, file);
        fclose(file);
    }
    
    free(config_content);
    free(new_config);
}

void ShowToastNotification(HWND hwnd, const char* message) {
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_NONE;
    
    const wchar_t* timeUpMsg = GetLocalizedString(L"时间到了!", L"Time's up!");
    wchar_t wTimeUpMsg[64];
    wcscpy(wTimeUpMsg, timeUpMsg);
    
    WideCharToMultiByte(CP_ACP, 0, wTimeUpMsg, -1, 
                        nid.szInfo, sizeof(nid.szInfo), NULL, NULL);
    
    nid.szInfoTitle[0] = '\0';
    nid.uTimeout = 10000;

    Shell_NotifyIcon(NIM_MODIFY, &nid);

    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.szInfo[0] = '\0';
    nid.szInfoTitle[0] = '\0';
}

// 添加获取本地化字符串的函数
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english) {
    switch (CURRENT_LANGUAGE) {
        case APP_LANG_CHINESE_SIMP:
            if (wcscmp(english, L"Time's up!") == 0) return L"时间到啦！";
            if (wcscmp(english, L"Input Format") == 0) return L"输入格式";
            if (wcscmp(english, L"Invalid Input") == 0) return L"无效输入";
            if (wcscmp(english, L"Error") == 0) return L"错误";
            if (wcscmp(english, L"Failed to load font: %hs") == 0) return L"无法加载字体: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25分钟\n25h   = 25小时\n25s   = 25秒\n25 30 = 25分钟30秒\n25 30m = 25小时30分钟\n1 30 20 = 1小时30分钟20秒";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"请输入用空格分隔的数字\n例如: 25 10 5";
            if (wcscmp(english, L"HEX: FF5733 or #FF5733\nRGB: 255,87,51") == 0)
                return L"十六进制: FF5733 或 #FF5733\nRGB: 255,87,51";
            if (wcscmp(english, L"123Pan") == 0) return L"123云盘";
            if (wcscmp(english, L"LanzouCloud (pwd: 1234)") == 0) return L"蓝奏云 (密码: 1234)";
            if (wcscmp(english, L"About") == 0) return L"关于";
            if (wcscmp(english, L"Version: %hs") == 0) return L"版本: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"检查更新";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Reset") == 0) return english; // Return English "Reset" instead of Chinese
            return chinese;
            
        case APP_LANG_CHINESE_TRAD:
            if (wcscmp(english, L"Time's up!") == 0) return L"時間到啦！";
            if (wcscmp(english, L"Input Format") == 0) return L"輸入格式";
            if (wcscmp(english, L"Invalid Input") == 0) return L"無效輸入";
            if (wcscmp(english, L"Error") == 0) return L"錯誤";
            if (wcscmp(english, L"Failed to load font: %hs") == 0) return L"無法加載字體: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25分鐘\n25h   = 25小時\n25s   = 25秒\n25 30 = 25分鐘30秒\n25 30m = 25小時30分鐘\n1 30 20 = 1小時30分鐘20秒";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"請輸入用空格分隔的數字\n例如: 25 10 5";
            if (wcscmp(english, L"HEX: FF5733 or #FF5733\nRGB: 255,87,51") == 0)
                return L"十六進制: FF5733 或 #FF5733\nRGB: 255,87,51";
            if (wcscmp(english, L"123Pan") == 0) return L"123雲盤";
            if (wcscmp(english, L"LanzouCloud (pwd: 1234)") == 0) return L"藍奏雲 (密碼: 1234)";
            if (wcscmp(english, L"About") == 0) return L"關於";
            if (wcscmp(english, L"Version: %hs") == 0) return L"版本: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"檢查更新";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Reset") == 0) return english; // Return English "Reset" instead of Traditional Chinese
            return chinese;

        case APP_LANG_SPANISH:
            if (wcscmp(english, L"Set Time") == 0) return L"Establecer tiempo";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Modo de edición";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Mostrar hora actual";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"Formato 24 horas";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Mostrar segundos";
            if (wcscmp(english, L"Time Display") == 0) return L"Visualización de tiempo";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Acción de tiempo";
            if (wcscmp(english, L"Show Message") == 0) return L"Mostrar mensaje";
            if (wcscmp(english, L"Browse...") == 0) return L"Explorar...";
            if (wcscmp(english, L"Open File") == 0) return L"Abrir archivo";
            if (wcscmp(english, L"Open: %hs") == 0) return L"Abrir: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Bloquear pantalla";
            if (wcscmp(english, L"Shutdown") == 0) return L"Apagar";
            if (wcscmp(english, L"Restart") == 0) return L"Reiniciar";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Modificar opciones";
            if (wcscmp(english, L"Customize") == 0) return L"Personalizar";
            if (wcscmp(english, L"Color") == 0) return L"Color";
            if (wcscmp(english, L"Font") == 0) return L"Fuente";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Versión: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Comentarios";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"Buscar actualizaciones";
            if (wcscmp(english, L"About") == 0) return L"Acerca de";
            if (wcscmp(english, L"Reset") == 0) return english; // Return English "Reset" instead of "Restablecer"
            if (wcscmp(english, L"Exit") == 0) return L"Salir";
            if (wcscmp(english, L"¡Tiempo terminado!") == 0) return L"¡Tiempo terminado!";
            if (wcscmp(english, L"Formato de entrada") == 0) return L"Formato de entrada";
            if (wcscmp(english, L"Entrada inválida") == 0) return L"Entrada inválida";
            if (wcscmp(english, L"Error") == 0) return L"Error";
            if (wcscmp(english, L"Error al cargar la fuente: %hs") == 0) return L"Error al cargar la fuente: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 minutos\n25h   = 25 horas\n25s   = 25 segundos\n25 30 = 25 minutos 30 segundos\n25 30m = 25 horas 30 minutos\n1 30 20 = 1 hora 30 minutos 20 segundos";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"Ingrese números separados por espacios\nEjemplo: 25 10 5";
            if (wcscmp(english, L"HEX: FF5733 or #FF5733\nRGB: 255,87,51") == 0)
                return L"HEX: FF5733 o #FF5733\nRGB: 255,87,51";
            if (wcscmp(english, L"123Pan") == 0) return L"123Pan";
            if (wcscmp(english, L"LanzouCloud (pwd: 1234)") == 0) return L"LanzouCloud (contraseña: 1234)";
            if (wcscmp(english, L"About") == 0) return L"Acerca de";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Versión: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Buscar actualizaciones";
            if (wcscmp(english, L"Language") == 0) return english;
            return english;

        case APP_LANG_FRENCH:
            if (wcscmp(english, L"Set Time") == 0) return L"Régler l'heure";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Mode édition";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Afficher l'heure actuelle";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"Format 24 heures";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Afficher les secondes";
            if (wcscmp(english, L"Time Display") == 0) return L"Affichage de l'heure";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Action de temporisation";
            if (wcscmp(english, L"Show Message") == 0) return L"Afficher le message";
            if (wcscmp(english, L"Browse...") == 0) return L"Parcourir...";
            if (wcscmp(english, L"Open File") == 0) return L"Ouvrir le fichier";
            if (wcscmp(english, L"Open: %hs") == 0) return L"Ouvrir: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Verrouiller l'écran";
            if (wcscmp(english, L"Shutdown") == 0) return L"Arrêter";
            if (wcscmp(english, L"Restart") == 0) return L"Redémarrer";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Modifier les options";
            if (wcscmp(english, L"Customize") == 0) return L"Personnaliser";
            if (wcscmp(english, L"Color") == 0) return L"Couleur";
            if (wcscmp(english, L"Font") == 0) return L"Police";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Version: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Retour";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"Vérifier les mises à jour";
            if (wcscmp(english, L"About") == 0) return L"À propos";
            if (wcscmp(english, L"Reset") == 0) return english; // Return English "Reset" instead of "Réinitialiser"
            if (wcscmp(english, L"Exit") == 0) return L"Quitter";
            if (wcscmp(english, L"Temps écoulé !") == 0) return L"Temps écoulé !";
            if (wcscmp(english, L"Format d'entrée") == 0) return L"Format d'entrée";
            if (wcscmp(english, L"Entrée invalide") == 0) return L"Entrée invalide";
            if (wcscmp(english, L"Erreur") == 0) return L"Erreur";
            if (wcscmp(english, L"Échec du chargement de la police: %hs") == 0) return L"Échec du chargement de la police: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 minutes\n25h   = 25 heures\n25s   = 25 secondes\n25 30 = 25 minutes 30 secondes\n25 30m = 25 heures 30 minutes\n1 30 20 = 1 heure 30 minutes 20 secondes";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"Entrez des nombres séparés par des espaces\nExemple : 25 10 5";
            if (wcscmp(english, L"HEX: FF5733 or #FF5733\nRGB: 255,87,51") == 0)
                return L"HEX : FF5733 ou #FF5733\nRGB : 255,87,51";
            if (wcscmp(english, L"123Pan") == 0) return L"123Pan";
            if (wcscmp(english, L"LanzouCloud (pwd: 1234)") == 0) return L"LanzouCloud (mdp : 1234)";
            if (wcscmp(english, L"About") == 0) return L"À propos";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Version: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Vérifier les mises à jour";
            if (wcscmp(english, L"Language") == 0) return english;
            return english;

        case APP_LANG_GERMAN:
            if (wcscmp(english, L"Set Time") == 0) return L"Zeit einstellen";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Bearbeitungsmodus";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Aktuelle Zeit anzeigen";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24-Stunden-Format";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Sekunden anzeigen";
            if (wcscmp(english, L"Time Display") == 0) return L"Zeitanzeige";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Zeitüberschreitungsaktion";
            if (wcscmp(english, L"Show Message") == 0) return L"Nachricht anzeigen";
            if (wcscmp(english, L"Browse...") == 0) return L"Durchsuchen...";
            if (wcscmp(english, L"Open File") == 0) return L"Datei öffnen";
            if (wcscmp(english, L"Open: %hs") == 0) return L"Öffnen: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Bildschirm sperren";
            if (wcscmp(english, L"Shutdown") == 0) return L"Herunterfahren";
            if (wcscmp(english, L"Restart") == 0) return L"Neustart";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Zeitoptionen ändern";
            if (wcscmp(english, L"Customize") == 0) return L"Anpassen";
            if (wcscmp(english, L"Color") == 0) return L"Farbe";
            if (wcscmp(english, L"Font") == 0) return L"Schriftart";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Version: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Feedback";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"Nach Updates suchen";
            if (wcscmp(english, L"About") == 0) return L"Über";
            if (wcscmp(english, L"Reset") == 0) return english; // Return English "Reset" instead of "Zurücksetzen"
            if (wcscmp(english, L"Exit") == 0) return L"Beenden";
            if (wcscmp(english, L"Zeit ist um!") == 0) return L"Zeit ist um!";
            if (wcscmp(english, L"Eingabeformat") == 0) return L"Eingabeformat";
            if (wcscmp(english, L"Ungültige Eingabe") == 0) return L"Ungültige Eingabe";
            if (wcscmp(english, L"Fehler") == 0) return L"Fehler";
            if (wcscmp(english, L"Schriftart konnte nicht geladen werden: %hs") == 0) return L"Schriftart konnte nicht geladen werden: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 Minuten\n25h   = 25 Stunden\n25s   = 25 Sekunden\n25 30 = 25 Minuten 30 Sekunden\n25 30m = 25 Stunden 30 Minuten\n1 30 20 = 1 Stunde 30 Minuten 20 Sekunden";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"Geben Sie durch Leerzeichen getrennte Zahlen ein\nBeispiel: 25 10 5";
            if (wcscmp(english, L"HEX: FF5733 or #FF5733\nRGB: 255,87,51") == 0)
                return L"HEX: FF5733 oder #FF5733\nRGB: 255,87,51";
            if (wcscmp(english, L"123Pan") == 0) return L"123Pan";
            if (wcscmp(english, L"LanzouCloud (pwd: 1234)") == 0) return L"LanzouCloud (Kennwort: 1234)";
            if (wcscmp(english, L"About") == 0) return L"Über";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Version: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Nach Updates suchen";
            if (wcscmp(english, L"Language") == 0) return english;
            return english;

        case APP_LANG_RUSSIAN:
            if (wcscmp(english, L"Set Time") == 0) return L"Установить время";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Режим редактирования";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Показать текущее время";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24-часовой формат";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Показать секунды";
            if (wcscmp(english, L"Time Display") == 0) return L"Отображение времени";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Действие по таймауту";
            if (wcscmp(english, L"Show Message") == 0) return L"Показать сообщение";
            if (wcscmp(english, L"Browse...") == 0) return L"Обзор...";
            if (wcscmp(english, L"Open File") == 0) return L"Открыть файл";
            if (wcscmp(english, L"Open: %hs") == 0) return L"Открыть: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Заблокировать экран";
            if (wcscmp(english, L"Shutdown") == 0) return L"Выключение";
            if (wcscmp(english, L"Restart") == 0) return L"Перезагрузка";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Изменить параметры времени";
            if (wcscmp(english, L"Customize") == 0) return L"Настроить";
            if (wcscmp(english, L"Color") == 0) return L"Цвет";
            if (wcscmp(english, L"Font") == 0) return L"Шрифт";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Версия: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Обратная связь";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"Проверить обновления";
            if (wcscmp(english, L"About") == 0) return L"О программе";
            if (wcscmp(english, L"Reset") == 0) return english; // Return English "Reset" instead of "Сброс"
            if (wcscmp(english, L"Exit") == 0) return L"Выход";
            if (wcscmp(english, L"Время вышло!") == 0) return L"Время вышло!";
            if (wcscmp(english, L"Формат ввода") == 0) return L"Формат ввода";
            if (wcscmp(english, L"Неверный ввод") == 0) return L"Неверный ввод";
            if (wcscmp(english, L"Ошибка") == 0) return L"Ошибка";
            if (wcscmp(english, L"Не удалось загрузить шрифт: %hs") == 0) return L"Не удалось загрузить шрифт: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 минут\n25h   = 25 часов\n25s   = 25 секунд\n25 30 = 25 минут 30 секунд\n25 30m = 25 часов 30 минут\n1 30 20 = 1 час 30 минут 20 секунд";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"Введите числа, разделенные пробелами\nПример: 25 10 5";
            if (wcscmp(english, L"HEX: FF5733 or #FF5733\nRGB: 255,87,51") == 0)
                return L"HEX: FF5733 или #FF5733\nRGB: 255,87,51";
            if (wcscmp(english, L"123Pan") == 0) return L"123Pan";
            if (wcscmp(english, L"LanzouCloud (pwd: 1234)") == 0) return L"LanzouCloud (пароль: 1234)";
            if (wcscmp(english, L"About") == 0) return L"О программе";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Версия: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Проверить обновления";
            if (wcscmp(english, L"Language") == 0) return english;
            return english;

        case APP_LANG_PORTUGUESE:
            if (wcscmp(english, L"Set Time") == 0) return L"Definir tempo";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Modo de edição";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Mostrar hora atual";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"Formato 24 horas";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Mostrar segundos";
            if (wcscmp(english, L"Time Display") == 0) return L"Exibição de tempo";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Ação de timeout";
            if (wcscmp(english, L"Show Message") == 0) return L"Mostrar mensagem";
            if (wcscmp(english, L"Browse...") == 0) return L"Navegar...";
            if (wcscmp(english, L"Open File") == 0) return L"Abrir arquivo";
            if (wcscmp(english, L"Open: %hs") == 0) return L"Abrir: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Bloquear tela";
            if (wcscmp(english, L"Shutdown") == 0) return L"Desligar";
            if (wcscmp(english, L"Restart") == 0) return L"Reiniciar";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Modificar opções";
            if (wcscmp(english, L"Customize") == 0) return L"Personalizar";
            if (wcscmp(english, L"Color") == 0) return L"Cor";
            if (wcscmp(english, L"Font") == 0) return L"Fonte";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Versão: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Feedback";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"Verificar atualizações";
            if (wcscmp(english, L"About") == 0) return L"Sobre";
            if (wcscmp(english, L"Reset") == 0) return english; // Return English "Reset" instead of "Redefinir"
            if (wcscmp(english, L"Exit") == 0) return L"Sair";
            if (wcscmp(english, L"Tempo esgotado!") == 0) return L"Tempo esgotado!";
            if (wcscmp(english, L"Formato de entrada") == 0) return L"Formato de entrada";
            if (wcscmp(english, L"Entrada inválida") == 0) return L"Entrada inválida";
            if (wcscmp(english, L"Erro") == 0) return L"Erro";
            if (wcscmp(english, L"Falha ao carregar fonte: %hs") == 0) return L"Falha ao carregar fonte: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 minutos\n25h   = 25 horas\n25s   = 25 segundos\n25 30 = 25 minutos 30 segundos\n25 30m = 25 horas 30 minutos\n1 30 20 = 1 hora 30 minutos 20 segundos";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"Insira números separados por espaços\nExemplo: 25 10 5";
            if (wcscmp(english, L"HEX: FF5733 or #FF5733\nRGB: 255,87,51") == 0)
                return L"HEX: FF5733 o #FF5733\nRGB: 255,87,51";
            if (wcscmp(english, L"123Pan") == 0) return L"123Pan";
            if (wcscmp(english, L"LanzouCloud (pwd: 1234)") == 0) return L"LanzouCloud (senha: 1234)";
            if (wcscmp(english, L"About") == 0) return L"Sobre";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Versão: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Verificar atualizações";
            if (wcscmp(english, L"Language") == 0) return english;
            return english;

        case APP_LANG_JAPANESE:
            if (wcscmp(english, L"Set Time") == 0) return L"時間設定";
            if (wcscmp(english, L"Edit Mode") == 0) return L"編集モード";
            if (wcscmp(english, L"Show Current Time") == 0) return L"現在時刻を表示";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24時間表示";
            if (wcscmp(english, L"Show Seconds") == 0) return L"秒を表示";
            if (wcscmp(english, L"Time Display") == 0) return L"時間表示";
            if (wcscmp(english, L"Timeout Action") == 0) return L"タイムアウト動作";
            if (wcscmp(english, L"Show Message") == 0) return L"メッセージを表示";
            if (wcscmp(english, L"Browse...") == 0) return L"参照...";
            if (wcscmp(english, L"Open File") == 0) return L"ファイルを開く";
            if (wcscmp(english, L"Open: %hs") == 0) return L"開く: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"画面をロック";
            if (wcscmp(english, L"Shutdown") == 0) return L"シャットダウン";
            if (wcscmp(english, L"Restart") == 0) return L"再起動";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"時間オプションを変更";
            if (wcscmp(english, L"Customize") == 0) return L"カスタマイズ";
            if (wcscmp(english, L"Color") == 0) return L"色";
            if (wcscmp(english, L"Font") == 0) return L"フォント";
            if (wcscmp(english, L"Version: %hs") == 0) return L"バージョン: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"フィードバック";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"更新を確認";
            if (wcscmp(english, L"About") == 0) return L"について";
            if (wcscmp(english, L"Reset") == 0) return english; // Return English "Reset" instead of "リセット"
            if (wcscmp(english, L"Exit") == 0) return L"終了";
            if (wcscmp(english, L"時間切れです!") == 0) return L"時間切れです!";
            if (wcscmp(english, L"入力形式") == 0) return L"入力形式";
            if (wcscmp(english, L"無効な入力") == 0) return L"無効な入力";
            if (wcscmp(english, L"エラー") == 0) return L"エラー";
            if (wcscmp(english, L"フォントの読み込みに失敗しました: %hs") == 0) return L"フォントの読み込みに失敗しました: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25分\n25h   = 25時間\n25s   = 25秒\n25 30 = 25分30秒\n25 30m = 25時間30分\n1 30 20 = 1時間30分20秒";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"日本語で入力してください\n例: 25 10 5";
            if (wcscmp(english, L"HEX: FF5733 or #FF5733\nRGB: 255,87,51") == 0)
                return L"HEX: FF5733 o #FF5733\nRGB: 255,87,51";
            if (wcscmp(english, L"123Pan") == 0) return L"123Pan";
            if (wcscmp(english, L"LanzouCloud (pwd: 1234)") == 0) return L"LanzouCloud (パスワード: 1234)";
            if (wcscmp(english, L"About") == 0) return L"について";
            if (wcscmp(english, L"Version: %hs") == 0) return L"バージョン: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"更新を確認";
            if (wcscmp(english, L"Language") == 0) return english;
            return english;

        case APP_LANG_KOREAN:
            if (wcscmp(english, L"Set Time") == 0) return L"시간 설정";
            if (wcscmp(english, L"Edit Mode") == 0) return L"편집 모드";
            if (wcscmp(english, L"Show Current Time") == 0) return L"현재 시간 표시";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24시간 형식";
            if (wcscmp(english, L"Show Seconds") == 0) return L"초 표시";
            if (wcscmp(english, L"Time Display") == 0) return L"시간 표시";
            if (wcscmp(english, L"Timeout Action") == 0) return L"시간 초과 동작";
            if (wcscmp(english, L"Show Message") == 0) return L"메시지 표시";
            if (wcscmp(english, L"Browse...") == 0) return L"찾아보기...";
            if (wcscmp(english, L"Open File") == 0) return L"파일 열기";
            if (wcscmp(english, L"Open: %hs") == 0) return L"열기: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"화면 잠금";
            if (wcscmp(english, L"Shutdown") == 0) return L"시스템 종료";
            if (wcscmp(english, L"Restart") == 0) return L"다시 시작";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"시간 옵션 수정";
            if (wcscmp(english, L"Customize") == 0) return L"사용자 지정";
            if (wcscmp(english, L"Color") == 0) return L"색상";
            if (wcscmp(english, L"Font") == 0) return L"글꼴";
            if (wcscmp(english, L"Version: %hs") == 0) return L"버전: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"피드백";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"업데이트 확인";
            if (wcscmp(english, L"About") == 0) return L"정보";
            if (wcscmp(english, L"Reset") == 0) return english; // Return English "Reset" instead of "초기화"
            if (wcscmp(english, L"Exit") == 0) return L"종료";
            if (wcscmp(english, L"시간이 종료되었습니다!") == 0) return L"시간이 종료되었습니다!";
            if (wcscmp(english, L"입력 형식") == 0) return L"입력 형식";
            if (wcscmp(english, L"잘못된 입력") == 0) return L"잘못된 입력";
            if (wcscmp(english, L"오류") == 0) return L"오류";
            if (wcscmp(english, L"글꼴을 불러올 수 없습니다: %hs") == 0) return L"글꼴을 불러올 수 없습니다: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25분\n25h   = 25시간\n25s   = 25초\n25 30 = 25분30초\n25 30m = 25시간30분\n1 30 20 = 1시간30분20초";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"한국어로 입력해주세요\n예: 25 10 5";
            if (wcscmp(english, L"HEX: FF5733 or #FF5733\nRGB: 255,87,51") == 0)
                return L"HEX: FF5733 o #FF5733\nRGB: 255,87,51";
            if (wcscmp(english, L"123Pan") == 0) return L"123Pan";
            if (wcscmp(english, L"LanzouCloud (pwd: 1234)") == 0) return L"LanzouCloud (비밀번호: 1234)";
            if (wcscmp(english, L"About") == 0) return L"정보";
            if (wcscmp(english, L"Version: %hs") == 0) return L"버전: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"업데이트 확인";
            if (wcscmp(english, L"Language") == 0) return english;
            return english;

        case APP_LANG_ENGLISH:
        default:
            return english;
    }
}

void WriteConfig(const char* config_path) {
    FILE* file = fopen(config_path, "w");
    if (!file) return;
    
    // 写入所有配置项
    fprintf(file, "CLOCK_TEXT_COLOR=%s\n", CLOCK_TEXT_COLOR);
    fprintf(file, "FONT_FILE_NAME=%s\n", FONT_FILE_NAME);
    fprintf(file, "CLOCK_DEFAULT_START_TIME=%d\n", CLOCK_DEFAULT_START_TIME);
    fprintf(file, "CLOCK_WINDOW_POS_X=%d\n", CLOCK_WINDOW_POS_X);
    fprintf(file, "CLOCK_WINDOW_POS_Y=%d\n", CLOCK_WINDOW_POS_Y);
    fprintf(file, "CLOCK_EDIT_MODE=%s\n", CLOCK_EDIT_MODE ? "TRUE" : "FALSE");
    fprintf(file, "WINDOW_SCALE=%.2f\n", CLOCK_WINDOW_SCALE);
    
    // 写入超时动作配置
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        fprintf(file, "CLOCK_TIMEOUT_ACTION=OPEN_FILE\n");
        fprintf(file, "CLOCK_TIMEOUT_FILE=%s\n", CLOCK_TIMEOUT_FILE_PATH);
    } else {
        switch (CLOCK_TIMEOUT_ACTION) {
            case TIMEOUT_ACTION_MESSAGE:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=MESSAGE\n");
                break;
            case TIMEOUT_ACTION_LOCK:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=LOCK\n");
                break;
            case TIMEOUT_ACTION_SHUTDOWN:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=SHUTDOWN\n");
                break;
            case TIMEOUT_ACTION_RESTART:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=RESTART\n");
                break;
        }
    }
    
    // 写入最近文件列表
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        fprintf(file, "CLOCK_RECENT_FILE=%s\n", CLOCK_RECENT_FILES[i].path);
    }
    
    fclose(file);
}

COLORREF ShowColorDialog(HWND hwnd) {
    WriteLog("ShowColorDialog: Started");
    
    CHOOSECOLOR cc = {0};
    static COLORREF acrCustClr[16] = {0};
    static DWORD rgbCurrent;
    
    // 将当前颜色转换为 COLORREF
    int r, g, b;
    if (CLOCK_TEXT_COLOR[0] == '#') {
        sscanf(CLOCK_TEXT_COLOR + 1, "%02x%02x%02x", &r, &g, &b);
    } else {
        sscanf(CLOCK_TEXT_COLOR, "%d,%d,%d", &r, &g, &b);
    }
    rgbCurrent = RGB(r, g, b);
    
    WriteLog("ShowColorDialog: Current color: %s (RGB: %d,%d,%d)", 
             CLOCK_TEXT_COLOR, r, g, b);

    cc.lStructSize = sizeof(CHOOSECOLOR);
    cc.hwndOwner = hwnd;
    cc.lpCustColors = acrCustClr;
    cc.rgbResult = rgbCurrent;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ENABLEHOOK;
    cc.lpfnHook = ColorDialogHookProc;

    if (ChooseColor(&cc)) {
        WriteLog("ShowColorDialog: Color chosen successfully - Color=#%02X%02X%02X",
                GetRValue(cc.rgbResult),
                GetGValue(cc.rgbResult),
                GetBValue(cc.rgbResult));
        
        rgbCurrent = cc.rgbResult;
        IS_COLOR_PREVIEWING = FALSE;
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
        return rgbCurrent;
    }
    
    WriteLog("ShowColorDialog: Dialog cancelled or failed");
    IS_COLOR_PREVIEWING = FALSE;
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
    return (COLORREF)-1;
}

// 添加颜色对话框钩子函数
UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hwndParent;
    static CHOOSECOLOR* pcc;
    static BOOL isColorSelecting = FALSE;
    
    switch (uiMsg) {
        case WM_INITDIALOG:
            WriteLog("ColorDialog: Initialized");
            pcc = (CHOOSECOLOR*)lParam;
            hwndParent = pcc->hwndOwner;
            return TRUE;
            
        case WM_LBUTTONDOWN:
            WriteLog("ColorDialog: Mouse button down - Starting color selection");
            isColorSelecting = TRUE;
            
        case WM_MOUSEMOVE:
            if (isColorSelecting) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hdlg, &pt);
                
                HDC hdc = GetDC(hdlg);
                COLORREF color = GetPixel(hdc, pt.x, pt.y);
                ReleaseDC(hdlg, hdc);
                
                if (color != CLR_INVALID && color != RGB(240, 240, 240)) {
                    // 确定鼠标所在的区域
                    const char* areaName;
                    if (pt.y < 200) {
                        areaName = "Basic Colors";  // 基本颜色区域
                    } else if (pt.y < 250) {
                        areaName = "Custom Colors"; // 自定义颜色区域
                    } else if (pt.x > 280) {
                        if (pt.y > 340) {
                            areaName = "Color Preview";  // 颜色预览区
                        } else {
                            areaName = "Color Matrix";   // 颜色矩阵/色谱区
                        }
                    } else if (pt.x > 260) {
                        areaName = "Luminance Bar"; // 亮度条
                    } else {
                        areaName = "Color Picker";   // 颜色选择区
                    }
                    
                    WriteLog("ColorDialog: Mouse preview - Area: %s, Position(%d,%d) Color=#%02X%02X%02X", 
                            areaName,
                            pt.x, pt.y, 
                            GetRValue(color), 
                            GetGValue(color), 
                            GetBValue(color));
                    
                    char colorStr[20];
                    sprintf(colorStr, "#%02X%02X%02X", 
                        GetRValue(color),
                        GetGValue(color),
                        GetBValue(color));
                    
                    if (pcc) {
                        pcc->rgbResult = color;
                    }
                    
                    strncpy(PREVIEW_COLOR, colorStr, sizeof(PREVIEW_COLOR) - 1);
                    PREVIEW_COLOR[sizeof(PREVIEW_COLOR) - 1] = '\0';
                    IS_COLOR_PREVIEWING = TRUE;
                    
                    InvalidateRect(hwndParent, NULL, TRUE);
                    UpdateWindow(hwndParent);
                }
            }
            break;
            
        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE) {
                const char* controlName;
                switch(LOWORD(wParam)) {
                    case 0x02c0:
                        controlName = "Red Input";
                        break;
                    case 0x02c1:
                        controlName = "Green Input";
                        break;
                    case 0x02c2:
                        controlName = "Blue Input";
                        break;
                    default:
                        controlName = "Unknown";
                }
                WriteLog("ColorDialog: Control changed - %s (ID: 0x%x)", 
                        controlName, LOWORD(wParam));
                
                HWND hwndRed = GetDlgItem(hdlg, 0x02c0);
                HWND hwndGreen = GetDlgItem(hdlg, 0x02c1);
                HWND hwndBlue = GetDlgItem(hdlg, 0x02c2);
                
                if (hwndRed && hwndGreen && hwndBlue) {
                    char redStr[4], greenStr[4], blueStr[4];
                    
                    GetWindowTextA(hwndRed, redStr, 4);
                    GetWindowTextA(hwndGreen, greenStr, 4);
                    GetWindowTextA(hwndBlue, blueStr, 4);
                    
                    int r = atoi(redStr);
                    int g = atoi(greenStr);
                    int b = atoi(blueStr);
                    
                    r = min(255, max(0, r));
                    g = min(255, max(0, g));
                    b = min(255, max(0, b));
                    
                    WriteLog("ColorDialog: RGB input change - R:%d G:%d B:%d (Control ID: 0x%x)", 
                            r, g, b, LOWORD(wParam));
                    
                    COLORREF color = RGB(r, g, b);
                    if (pcc) {
                        pcc->rgbResult = color;
                    }
                    
                    char colorStr[20];
                    sprintf(colorStr, "#%02X%02X%02X", r, g, b);
                    
                    strncpy(PREVIEW_COLOR, colorStr, sizeof(PREVIEW_COLOR) - 1);
                    PREVIEW_COLOR[sizeof(PREVIEW_COLOR) - 1] = '\0';
                    IS_COLOR_PREVIEWING = TRUE;
                    
                    InvalidateRect(hwndParent, NULL, TRUE);
                    UpdateWindow(hwndParent);
                }
            } else if (HIWORD(wParam) == BN_CLICKED) {
                const char* buttonName;
                switch (LOWORD(wParam)) {
                    case 0x1:
                        buttonName = "OK";
                        break;
                    case 0x2:
                        buttonName = "Cancel";
                        break;
                    case 0x02c8:
                        buttonName = "Add to Custom Colors";
                        break;
                    default:
                        buttonName = "Unknown";
                }
                WriteLog("ColorDialog: Button clicked - %s button (Control ID: 0x%x)", 
                        buttonName, LOWORD(wParam));
            }
            break;
            
        case WM_DESTROY:
            WriteLog("ColorDialog: Dialog destroyed");
            pcc = NULL;
            break;
    }
    return 0;
}

// 在文件开头的函数声明区域添加（其他函数声明的地方）
void WriteLog(const char* format, ...) {
    char logPath[MAX_PATH];
    snprintf(logPath, MAX_PATH, "%s\\Desktop\\catime_color.log", getenv("USERPROFILE"));
    
    FILE* logFile = fopen(logPath, "a");
    if (!logFile) return;
    
    // 添加时间戳
    time_t now = time(NULL);
    char timeStr[26];
    ctime_s(timeStr, sizeof(timeStr), &now);
    timeStr[24] = '\0'; // 移除换行符
    fprintf(logFile, "[%s] ", timeStr);
    
    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    
    fprintf(logFile, "\n");
    fclose(logFile);
}
