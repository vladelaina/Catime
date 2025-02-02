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

#define VK_MEDIA_PLAY_PAUSE 0xB3
#define VK_MEDIA_STOP 0xB2
#define KEYEVENTF_KEYUP 0x0002

void PauseMediaPlayback(void);

typedef struct {
    const char* hexColor;
} PredefinedColor;

static const PredefinedColor COLOR_OPTIONS[] = {
    {"#FFFFFF"},  // White
    {"#FFB6C1"},  // Light Pink
    {"#6495ED"},  // Cornflower Blue
    {"#5EFFFF"},  // Bright Cyan
    {"#98FB98"},  // Pale Green
    {"#9370DB"},  // Medium Purple
    {"#7FFFD4"},  // Aquamarine
    {"#F08080"},  // Light Coral
    {"#FF7F50"}   // Coral
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
    TIMEOUT_ACTION_OPEN_FILE = 4  // 新增
} TimeoutActionType;

TimeoutActionType CLOCK_TIMEOUT_ACTION;

char inputText[256] = {0};
static int elapsed_time = 0;
static int CLOCK_TOTAL_TIME = 0;
NOTIFYICONDATA nid;
time_t last_config_time = 0;
int message_shown = 0;
char CLOCK_TIMEOUT_TEXT[50] = "";
char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH] = "";  // 添加这一行

char FONT_FILE_NAME[100] = "Hack Nerd Font.ttf";

char FONT_INTERNAL_NAME[100];

typedef struct {
    int menuId;
    int resourceId;
    const char* fontName;
} FontResource;

FontResource fontResources[] = {
    {CLOCK_IDC_FONT_AGAVE, IDR_FONT_AGAVE, "Agave Nerd Font.ttf"},
    {CLOCK_IDC_FONT_ANONYMICE, IDR_FONT_ANONYMICE, "AnonymiceProNerdFont-Regular.ttf"},
    {CLOCK_IDC_FONT_AURULENT, IDR_FONT_AURULENT, "AurulentSansM Nerd Font.ttf"},
    {CLOCK_IDC_FONT_BIGBLUE, IDR_FONT_BIGBLUE, "BigBlueTermPlus Nerd Font.ttf"},
    {CLOCK_IDC_FONT_COMIC_SHANNS, IDR_FONT_COMIC_SHANNS, "ComicShannsMono Nerd Font.ttf"},
    {CLOCK_IDC_FONT_HACK, IDR_FONT_HACK, "Hack Nerd Font.ttf"},
    {CLOCK_IDC_FONT_HEAVYDATA, IDR_FONT_HEAVYDATA, "HeavyData Nerd Font.ttf"},
    {CLOCK_IDC_FONT_PROFONT, IDR_FONT_PROFONT, "ProFont IIx Nerd Font.ttf"},
    {CLOCK_IDC_FONT_ARIMO, IDR_FONT_ARIMO, "Arimo Nerd Font.ttf"},
    {CLOCK_IDC_FONT_COUSINE, IDR_FONT_COUSINE, "Cousine Nerd Font.ttf"},
    {CLOCK_IDC_FONT_DROIDSANS, IDR_FONT_DROIDSANS, "DroidSansM Nerd Font Mono.ttf"},
    {CLOCK_IDC_FONT_ROBOTOMONO, IDR_FONT_ROBOTOMONO, "RobotoMono Nerd Font Propo Md.ttf"},
    {CLOCK_IDC_FONT_0XPROTO, IDR_FONT_0XPROTO, "0xProto Nerd Font.ttf"},
    {CLOCK_IDC_FONT_PROGGYCLEAN, IDR_FONT_PROGGYCLEAN, "ProggyCleanSZ Nerd Font Propo.ttf"},
    {CLOCK_IDC_FONT_BLEX, IDR_FONT_BLEX, "BlexMono Nerd Font.ttf"},
    {CLOCK_IDC_FONT_CASKAYDIA_COVE, IDR_FONT_CASKAYDIA_COVE, "CaskaydiaCove NF SemiLight.ttf"},
    {CLOCK_IDC_FONT_CASKAYDIA_MONO, IDR_FONT_CASKAYDIA_MONO, "CaskaydiaMono NF SemiLight.ttf"},
    {CLOCK_IDC_FONT_CODENEWROMAN, IDR_FONT_CODENEWROMAN, "CodeNewRoman Nerd Font.ttf"},
    {CLOCK_IDC_FONT_D2CODING, IDR_FONT_D2CODING, "D2CodingLigature Nerd Font.ttf"},
    {CLOCK_IDC_FONT_DADDYTIME, IDR_FONT_DADDYTIME, "DaddyTimeMono Nerd Font Propo.ttf"},
    {CLOCK_IDC_FONT_DEPARTURE, IDR_FONT_DEPARTURE, "DepartureMono Nerd Font Propo.ttf"},
    {CLOCK_IDC_FONT_ENVYCODE, IDR_FONT_ENVYCODE, "EnvyCodeR Nerd Font.ttf"},
    {CLOCK_IDC_FONT_FANTASQUE, IDR_FONT_FANTASQUE, "FantasqueSansM Nerd Font.ttf"},
    {CLOCK_IDC_FONT_FIRACODE, IDR_FONT_FIRACODE, "FiraCode Nerd Font.ttf"},
    {CLOCK_IDC_FONT_FIRAMONO, IDR_FONT_FIRAMONO, "FiraMono Nerd Font Propo.ttf"},
    {CLOCK_IDC_FONT_GEIST, IDR_FONT_GEIST, "GeistMono NFM.ttf"},
    {CLOCK_IDC_FONT_HASKLUG, IDR_FONT_HASKLUG, "Hasklug Nerd Font.ttf"},
    {CLOCK_IDC_FONT_HURMIT, IDR_FONT_HURMIT, "Hurmit Nerd Font Propo.ttf"},
    {CLOCK_IDC_FONT_IMWRITING, IDR_FONT_IMWRITING, "iMWritingQuat Nerd Font.ttf"},
    {CLOCK_IDC_FONT_INCONSOLATA_GO, IDR_FONT_INCONSOLATA_GO, "InconsolataGo Nerd Font.ttf"},
    {CLOCK_IDC_FONT_INCONSOLATA_LGC, IDR_FONT_INCONSOLATA_LGC, "Inconsolata LGC Nerd Font.ttf"},
    {CLOCK_IDC_FONT_INCONSOLATA, IDR_FONT_INCONSOLATA, "Inconsolata Nerd Font.ttf"},
    {CLOCK_IDC_FONT_INTONE, IDR_FONT_INTONE, "IntoneMono NF.ttf"},
    {CLOCK_IDC_FONT_IOSEVKA, IDR_FONT_IOSEVKA, "Iosevka NF SemiBold.ttf"},
    {CLOCK_IDC_FONT_IOSEVKA_TERM, IDR_FONT_IOSEVKA_TERM, "IosevkaTermSlab NFP Medium.ttf"},
    {CLOCK_IDC_FONT_JETBRAINS, IDR_FONT_JETBRAINS, "JetBrainsMonoNL NFP SemiBold.ttf"},
    {CLOCK_IDC_FONT_LEKTON, IDR_FONT_LEKTON, "Lekton Nerd Font.ttf"},
    {CLOCK_IDC_FONT_LILEX, IDR_FONT_LILEX, "Lilex Nerd Font.ttf"},
    {CLOCK_IDC_FONT_LIBERATION, IDR_FONT_LIBERATION, "LiterationSerif Nerd Font.ttf"},
    {CLOCK_IDC_FONT_MARTIAN, IDR_FONT_MARTIAN, "MartianMono NFM Cond.ttf"},
    {CLOCK_IDC_FONT_MONONOKI, IDR_FONT_MONONOKI, "Mononoki Nerd Font.ttf"},
    {CLOCK_IDC_FONT_OVERPASS, IDR_FONT_OVERPASS, "OverpassM Nerd Font Mono SemBd.ttf"},
    {CLOCK_IDC_FONT_RECMONO, IDR_FONT_RECMONO, "RecMonoCasual Nerd Font Mono.ttf"},
    {CLOCK_IDC_FONT_TERMINESS, IDR_FONT_TERMINESS, "Terminess Nerd Font Propo.ttf"},
    {CLOCK_IDC_FONT_VICTORMONO, IDR_FONT_VICTORMONO, "VictorMono NFP Medium.ttf"},
    {CLOCK_IDC_FONT_ZEDMONO, IDR_FONT_ZEDMONO, "ZedMono NF.ttf"},
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
void WriteConfigTimeOptions(const char* options);  // Add this line
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

#define MAX_RECENT_FILES 3  // 最多保存3个最近文件
typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
} RecentFile;

RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];
int CLOCK_RECENT_FILES_COUNT = 0;

#define CLOCK_ABOUT_URL "https://github.com/vladelaina/Catime"
#define CLOCK_IDM_ABOUT 130  // 添加菜单ID

char PREVIEW_FONT_NAME[100] = "";  // 用于存储预览的字体名称
char PREVIEW_INTERNAL_NAME[100] = "";    // 用于存储预览的字体内部名称
BOOL IS_PREVIEWING = FALSE;        // 标记是否正在预览

char PREVIEW_COLOR[10] = "";  // 用于存储预览的颜色
BOOL IS_COLOR_PREVIEWING = FALSE;  // 标记是否正在预览颜色

#define WM_USER_SHELLICON WM_USER + 1

void ShowToastNotification(HWND hwnd, const char* message);

// 在全局变量区域添加
BOOL CLOCK_SHOW_CURRENT_TIME = FALSE;
time_t CLOCK_LAST_TIME_UPDATE = 0;
BOOL CLOCK_USE_24HOUR = TRUE;
BOOL CLOCK_SHOW_SECONDS = TRUE;

// 在文件开头的宏定义区域添加新的菜单ID
#define CLOCK_IDM_SHOW_CURRENT_TIME 150
#define CLOCK_IDM_24HOUR_FORMAT    151
#define CLOCK_IDM_SHOW_SECONDS     152

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
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
        return;  // Config file already exists
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
    fprintf(file, "CLOCK_WINDOW_POS_Y=-7\n");  // 修改这里，将默认值改为-7
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
    
    size_t buffer_size = 8192;  // 初始缓冲区大小
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
            
            if (path[0] == '"') path++;
            size_t len = strlen(path);
            if (len > 0 && path[len-1] == '"') path[len-1] = '\0';
            
            while (*path == '=') path++;
            
            strncpy(CLOCK_TIMEOUT_FILE_PATH, path, sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
            CLOCK_TIMEOUT_FILE_PATH[sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1] = '\0';
        }
    }

    fclose(file);
    last_config_time = time(NULL);

    HWND hwnd = FindWindow("CatimeWindow", "Catime");
    if (hwnd) {
        SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        InvalidateRect(hwnd, NULL, TRUE);
    }

    LoadRecentFiles();  // 添加这一行
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
    int timeout_action_written = 0;
    int timeout_file_written = 0;
    int success = 1;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 20) == 0) {
            if (fprintf(temp, "CLOCK_TIMEOUT_ACTION=%s\n", action) < 0) {
                success = 0;
                break;
            }
            timeout_action_written = 1;
        }
        else if (strncmp(line, "CLOCK_TIMEOUT_FILE=", 19) == 0) {
            if (strcmp(action, "OPEN_FILE") == 0) {
                if (fprintf(temp, "CLOCK_TIMEOUT_FILE=\"%s\"\n", CLOCK_TIMEOUT_FILE_PATH) < 0) {
                    success = 0;
                    break;
                }
            }
            timeout_file_written = 1;
        }
        else {
            if (fputs(line, temp) == EOF) {
                success = 0;
                break;
            }
        }
    }

    if (!timeout_action_written && success) {
        if (fprintf(temp, "CLOCK_TIMEOUT_ACTION=%s\n", action) < 0) {
            success = 0;
        }
    }
    
    if (!timeout_file_written && strcmp(action, "OPEN_FILE") == 0 && success) {
        if (fprintf(temp, "CLOCK_TIMEOUT_FILE=\"%s\"\n", CLOCK_TIMEOUT_FILE_PATH) < 0) {
            success = 0;
        }
    }

    fclose(file);
    fclose(temp);

    if (success) {
        if (remove(config_path) == 0) {
            if (rename(temp_path, config_path) != 0) {
                rename(temp_path, config_path);
            }
        }
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

    return hasDigit;  // 确保至少有一个数字
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
            tokens[0][strlen(tokens[0]) - 1] = '\0';  // Remove unit character
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
            tokens[1][strlen(tokens[1]) - 1] = '\0';  // Remove unit character
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

    if (hours < 0 || hours > 99 ||    // Limit hours to 0-99
        minutes < 0 || minutes > 59 || // Limit minutes to 0-59
        seconds < 0 || seconds > 59) { // Limit seconds to 0-59
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
            return FALSE; // Return FALSE to keep focus on edit control
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
                // 保持12
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
            sprintf(time_text, "    %d:%02d", minutes, seconds);  // 添加四个空格以对齐小时格式
        } else {
            sprintf(time_text, "    %d:%02d", minutes, seconds);  // 添加四个空格以对齐小时格式
        }
    } else {
        if (seconds < 10) {
            sprintf(time_text, "          %d", seconds);  // 10个空格（改为10个）
        } else {
            sprintf(time_text, "        %d", seconds);  // 8个空格
        }
    }
}

void ExitProgram(HWND hwnd) {
    Shell_NotifyIcon(NIM_DELETE, &nid);

    PostQuitMessage(0);
}

void ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, 101, "Set Time");
    
    // 创建时间显示子菜单
    HMENU hTimeMenu = CreatePopupMenu();
    AppendMenu(hTimeMenu, MF_STRING | (CLOCK_SHOW_CURRENT_TIME ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHOW_CURRENT_TIME, "Show Current Time");
    AppendMenu(hTimeMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hTimeMenu, MF_STRING | (CLOCK_USE_24HOUR ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_24HOUR_FORMAT, "24-Hour Format");
    AppendMenu(hTimeMenu, MF_STRING | (CLOCK_SHOW_SECONDS ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHOW_SECONDS, "Show Seconds");
    
    // 添加Time Display菜单项，并根据CLOCK_SHOW_CURRENT_TIME设置选中状态
    AppendMenu(hMenu, MF_POPUP | (CLOCK_SHOW_CURRENT_TIME ? MF_CHECKED : MF_UNCHECKED), 
               (UINT_PTR)hTimeMenu, "Time Display");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

    for (int i = 0; i < time_options_count; i++) {
        char menu_item[20];
        snprintf(menu_item, sizeof(menu_item), "%d", time_options[i]);
        AppendMenu(hMenu, MF_STRING, 102 + i, menu_item);
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

    AppendMenu(hMenu, MF_STRING | (CLOCK_EDIT_MODE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDC_EDIT_MODE, "Edit Mode");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

    HMENU hTimeoutMenu = CreatePopupMenu();
    AppendMenu(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHOW_MESSAGE, "Show Message");

    HMENU hOpenFileMenu = CreatePopupMenu();
    if (CLOCK_RECENT_FILES_COUNT > 0) {
        for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
            BOOL isCurrentFile = (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && 
                                strcmp(CLOCK_RECENT_FILES[i].path, CLOCK_TIMEOUT_FILE_PATH) == 0);
            AppendMenu(hOpenFileMenu, MF_STRING | (isCurrentFile ? MF_CHECKED : MF_UNCHECKED), 
                      CLOCK_IDM_RECENT_FILE_1 + i, 
                      CLOCK_RECENT_FILES[i].name);
        }
        AppendMenu(hOpenFileMenu, MF_SEPARATOR, 0, NULL);
    }
    AppendMenu(hOpenFileMenu, MF_STRING, CLOCK_IDM_BROWSE_FILE, "Browse...");

    char menuText[32] = "Open File";
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        char *filename = strrchr(CLOCK_TIMEOUT_FILE_PATH, '\\');
        if (filename) {
            filename++;
            snprintf(menuText, sizeof(menuText), "Open: %s", filename);
        }
    }

    AppendMenu(hTimeoutMenu, MF_POPUP | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE ? MF_CHECKED : MF_UNCHECKED),
               (UINT_PTR)hOpenFileMenu, menuText);
               
    AppendMenu(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_LOCK ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_LOCK_SCREEN, "Lock Screen");
    AppendMenu(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHUTDOWN, "Shutdown");
    AppendMenu(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_RESTART, "Restart");

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hTimeoutMenu, "Timeout Action");

    AppendMenu(hMenu, MF_STRING, CLOCK_IDC_MODIFY_OPTIONS, "Modify Time Options");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

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
    AppendMenu(hColorSubMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hColorSubMenu, MF_STRING, CLOCK_IDC_CUSTOMIZE_LEFT, "Customize");

    for (int i = 0; i < sizeof(fontResources) / sizeof(fontResources[0]); i++) {
        BOOL isCurrentFont = strcmp(FONT_FILE_NAME, fontResources[i].fontName) == 0;
        AppendMenu(hFontSubMenu, MF_STRING | (isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                  fontResources[i].menuId, fontResources[i].fontName);
    }

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hColorSubMenu, "Color");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu, "Font");

    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, CLOCK_IDM_ABOUT, "About");  // 移到Reset前面
    AppendMenu(hMenu, MF_STRING, 200, "Reset");
    AppendMenu(hMenu, MF_STRING, 109, "Exit");

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
                
                // 获取鼠标在屏幕上的位置
                POINT mousePos;
                GetCursorPos(&mousePos);
                
                // 获取窗口位置和大小
                RECT windowRect;
                GetWindowRect(hwnd, &windowRect);
                int oldWidth = windowRect.right - windowRect.left;
                int oldHeight = windowRect.bottom - windowRect.top;
                
                // 计算鼠标相对于窗口左上角的偏移比例
                float relativeX = (float)(mousePos.x - windowRect.left) / oldWidth;
                float relativeY = (float)(mousePos.y - windowRect.top) / oldHeight;
                
                // 使用更合适的缩放步长
                float scaleFactor = 1.1f;  // 改回更大的缩放比例
                if (delta > 0) {
                    CLOCK_FONT_SCALE_FACTOR *= scaleFactor;
                    CLOCK_WINDOW_SCALE = CLOCK_FONT_SCALE_FACTOR;
                } else {
                    CLOCK_FONT_SCALE_FACTOR /= scaleFactor;
                    CLOCK_WINDOW_SCALE = CLOCK_FONT_SCALE_FACTOR;
                }
                
                // 限制缩放范围
                if (CLOCK_FONT_SCALE_FACTOR < MIN_SCALE_FACTOR) {
                    CLOCK_FONT_SCALE_FACTOR = MIN_SCALE_FACTOR;
                    CLOCK_WINDOW_SCALE = MIN_SCALE_FACTOR;
                }
                if (CLOCK_FONT_SCALE_FACTOR > MAX_SCALE_FACTOR) {
                    CLOCK_FONT_SCALE_FACTOR = MAX_SCALE_FACTOR;
                    CLOCK_WINDOW_SCALE = MAX_SCALE_FACTOR;
                }
                
                if (old_scale != CLOCK_FONT_SCALE_FACTOR) {
                    // 直接使用缩放比例计算新的窗口大小
                    int newWidth = (int)(oldWidth * (CLOCK_FONT_SCALE_FACTOR / old_scale));
                    int newHeight = (int)(oldHeight * (CLOCK_FONT_SCALE_FACTOR / old_scale));
                    
                    // 计算新的窗口位置，保持鼠标位置相对不变
                    int newX = mousePos.x - (int)(relativeX * newWidth);
                    int newY = mousePos.y - (int)(relativeY * newHeight);
                    
                    // 使用 SWP_NOREDRAW 来避免闪烁
                    SetWindowPos(hwnd, NULL, 
                        newX, newY,
                        newWidth, newHeight,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
                    
                    // 使用累积计时器来延迟保存设置
                    static UINT_PTR timerId = 0;
                    if (timerId) {
                        KillTimer(hwnd, timerId);
                    }
                    timerId = SetTimer(hwnd, 3, 200, NULL);  // 增加延迟到200ms
                    
                    // 立即重绘窗口
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
                    windowRect.right - windowRect.left,  // 保持当前窗口大小
                    windowRect.bottom - windowRect.top,  // 保持当前窗口大小
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW  // 添加 SWP_NOREDRAW 标志
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
                CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,  // 改回 ANTIALIASED_QUALITY
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
            if (wp == 3) {  // 缩放更新计时器
                KillTimer(hwnd, 3);
                SaveWindowSettings(hwnd);
            } else if (wp == 1) {  // 使用定时器1来处理所有时间更新
                if (CLOCK_SHOW_CURRENT_TIME) {
                    static DWORD lastTick = 0;
                    DWORD currentTick = GetTickCount();
                    
                    // 确保大约每秒更新一次
                    if (currentTick - lastTick >= 1000) {
                        lastTick = currentTick;
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                } else if (elapsed_time < CLOCK_TOTAL_TIME) {
                    elapsed_time++;
                    InvalidateRect(hwnd, NULL, TRUE);
                } else if (elapsed_time == CLOCK_TOTAL_TIME) {
                    KillTimer(hwnd, 1);
                    InvalidateRect(hwnd, NULL, TRUE);

                    if (!message_shown) {
                        switch (CLOCK_TIMEOUT_ACTION) {
                            case TIMEOUT_ACTION_MESSAGE:
                                ShowToastNotification(hwnd, "Time's up!");
                                break;
                            case TIMEOUT_ACTION_LOCK:
                                PauseMediaPlayback();
                                Sleep(10);
                                LockWorkStation();
                                break;
                            case TIMEOUT_ACTION_SHUTDOWN:
                                system("shutdown /s /t 0");
                                break;
                            case TIMEOUT_ACTION_RESTART:
                                system("shutdown /r /t 0");
                                break;
                            case TIMEOUT_ACTION_OPEN_FILE:
                                if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
                                    STARTUPINFO si = {sizeof(si)};
                                    PROCESS_INFORMATION pi;
                                    char cmdLine[MAX_PATH + 2];  // 额外空间用于引号
                                    
                                    snprintf(cmdLine, sizeof(cmdLine), "\"%s\"", CLOCK_TIMEOUT_FILE_PATH);
                                    
                                    if (CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 
                                                     0, NULL, NULL, &si, &pi)) {
                                        CloseHandle(pi.hProcess);
                                        CloseHandle(pi.hThread);
                                    } else {
                                        ShellExecuteA(NULL, "open", CLOCK_TIMEOUT_FILE_PATH, 
                                                    NULL, NULL, SW_SHOWNORMAL);
                                    }
                                }
                                break;
                        }
                        message_shown = 1;
                    }
                    elapsed_time++;
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
            switch (LOWORD(wp)) {
                case 101: {  // Set Time case
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        CLOCK_LAST_TIME_UPDATE = 0;
                    }
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_DIALOG1), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            // 如果输入为空，不要开始倒计时，直接返回
                            break;
                        }

                        int total_seconds = 0;
                        if (ParseInput(inputText, &total_seconds)) {
                            CLOCK_TOTAL_TIME = total_seconds;
                            elapsed_time = 0;
                            message_shown = 0;
                            InvalidateRect(hwnd, NULL, TRUE);
                            SetTimer(hwnd, 1, 1000, NULL);
                            break;
                        } else {
                            MessageBox(hwnd, 
                                "25    = 25 minutes \n"
                                "25h   = 25 hours \n"
                                "25s   = 25 seconds \n"
                                "25 30 = 25 minutes 30 seconds \n"
                                "25 30m = 25 hours 30 minutes \n"
                                "1 30 20 = 1 hour 30 minutes 20 seconds",
                                "Input Format",
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
                            MessageBox(hwnd,
                                "Enter numbers separated by spaces\n"
                                "Example: 25 10 5",
                                "Invalid Input", MB_OK);  // 移除了 MB_ICONWARNING
                        }
                    }
                    break;
                }
                case 200: {  // Reset case
                    int current_elapsed = elapsed_time;
                    int current_total = CLOCK_TOTAL_TIME;
                    BOOL was_timing = (current_elapsed < current_total);
                    
                    CLOCK_EDIT_MODE = FALSE;
                    SetClickThrough(hwnd, TRUE);
                    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
                    
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    remove(config_path);  // 删除现有配置文件
                    CreateDefaultConfig(config_path);  // 创建新的默认配置文件
                    
                    ReadConfig();  // 这会加载默认的窗口位置
                    
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
                        -CLOCK_BASE_FONT_SIZE,  // 使用未缩放的基础字体大小
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
                case CLOCK_IDM_ABOUT: {
                    ShellExecuteA(NULL, "open", CLOCK_ABOUT_URL, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                default: {
                    int cmd = LOWORD(wp);
                    if (cmd >= 102 && cmd < 102 + time_options_count) {
                        // 如果当前是显示时间模式，先关闭它
                        if (CLOCK_SHOW_CURRENT_TIME) {
                            CLOCK_SHOW_CURRENT_TIME = FALSE;
                            CLOCK_LAST_TIME_UPDATE = 0;
                        }
                        
                        // 设置倒计时
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
                            strncpy(CLOCK_TIMEOUT_FILE_PATH, CLOCK_RECENT_FILES[index].path, 
                                    sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
                            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
                            WriteConfigTimeoutAction("OPEN_FILE");
                            SaveRecentFile(CLOCK_RECENT_FILES[index].path);
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
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_DIALOG1), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            break;
                        }

                        if (isValidColor(inputText)) {
                            char hex_color[10] = "#";  // 添加 # 作为前缀
                            if (strchr(inputText, ',') != NULL) {
                                int r, g, b;
                                sscanf(inputText, "%d,%d,%d", &r, &g, &b);
                                snprintf(hex_color + 1, sizeof(hex_color) - 1, "%02X%02X%02X", r, g, b);
                                WriteConfigColor(hex_color);
                            } else {
                                if (inputText[0] == '#') {
                                    WriteConfigColor(inputText);  // 已经包含 #
                                } else {
                                    snprintf(hex_color + 1, sizeof(hex_color) - 1, "%s", inputText);
                                    WriteConfigColor(hex_color);
                                }
                            }
                            InvalidateRect(hwnd, NULL, TRUE);
                            break;
                        } else {
                            MessageBox(hwnd, 
                                "HEX: FF5733 or #FF5733\n"
                                "RGB: 255,87,51",
                                "Input Format", MB_OK);  // 移除了 MB_ICONWARNING 标志
                        }
                    }
                    break;
                }
                case CLOCK_IDC_FONT_AGAVE:
                    WriteConfigFont("Agave Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Agave Nerd Font.ttf")) {
                        MessageBox(hwnd, "Failed to load font: Agave Nerd Font.ttf", "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_ANONYMICE:
                    WriteConfigFont("AnonymiceProNerdFont-Regular.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "AnonymiceProNerdFont-Regular.ttf")) {
                        MessageBox(hwnd, "Failed to load font: AnonymiceProNerdFont-Regular.ttf", "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_AURULENT:
                    WriteConfigFont("AurulentSansM Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "AurulentSansM Nerd Font.ttf")) {
                        MessageBox(hwnd, "Failed to load font: AurulentSansM Nerd Font.ttf", "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_BIGBLUE:
                    WriteConfigFont("BigBlueTermPlus Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "BigBlueTermPlus Nerd Font.ttf")) {
                        MessageBox(hwnd, "Failed to load font: BigBlueTermPlus Nerd Font.ttf", "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_COMIC_SHANNS:
                    WriteConfigFont("ComicShannsMono Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "ComicShannsMono Nerd Font.ttf")) {
                        MessageBox(hwnd, "Failed to load font: ComicShannsMono Nerd Font.ttf", "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_HACK:
                    WriteConfigFont("Hack Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Hack Nerd Font.ttf")) {
                        MessageBox(hwnd, "Failed to load font: Hack Nerd Font.ttf", "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_HEAVYDATA:
                    WriteConfigFont("HeavyData Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "HeavyData Nerd Font.ttf")) {
                        MessageBox(hwnd, "Failed to load font: HeavyData Nerd Font.ttf", "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_PROFONT:
                    WriteConfigFont("ProFont IIx Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "ProFont IIx Nerd Font.ttf")) {
                        MessageBox(hwnd, "Failed to load font: ProFont IIx Nerd Font.ttf", "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_ARIMO:
                    WriteConfigFont("Arimo Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Arimo Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: Arimo Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_COUSINE:
                    WriteConfigFont("Cousine Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Cousine Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: Cousine Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_DROIDSANS:
                    WriteConfigFont("DroidSansM Nerd Font Mono.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "DroidSansM Nerd Font Mono.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: DroidSansM Nerd Font Mono.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_ROBOTOMONO:
                    WriteConfigFont("RobotoMono Nerd Font Propo Md.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "RobotoMono Nerd Font Propo Md.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: RobotoMono Nerd Font Propo Md.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_0XPROTO:
                    WriteConfigFont("0xProto Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "0xProto Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: 0xProto Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_PROGGYCLEAN:
                    WriteConfigFont("ProggyCleanSZ Nerd Font Propo.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "ProggyCleanSZ Nerd Font Propo.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: ProggyCleanSZ Nerd Font Propo.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_BLEX:
                    WriteConfigFont("BlexMono Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "BlexMono Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: BlexMono Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_CASKAYDIA_COVE:
                    WriteConfigFont("CaskaydiaCove NF SemiLight.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "CaskaydiaCove NF SemiLight.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: CaskaydiaCove NF SemiLight.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_CASKAYDIA_MONO:
                    WriteConfigFont("CaskaydiaMono NF SemiLight.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "CaskaydiaMono NF SemiLight.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: CaskaydiaMono NF SemiLight.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_CODENEWROMAN:
                    WriteConfigFont("CodeNewRoman Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "CodeNewRoman Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: CodeNewRoman Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_D2CODING:
                    WriteConfigFont("D2CodingLigature Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "D2CodingLigature Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: D2CodingLigature Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_DADDYTIME:
                    WriteConfigFont("DaddyTimeMono Nerd Font Propo.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "DaddyTimeMono Nerd Font Propo.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: DaddyTimeMono Nerd Font Propo.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_DEPARTURE:
                    WriteConfigFont("DepartureMono Nerd Font Propo.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "DepartureMono Nerd Font Propo.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: DepartureMono Nerd Font Propo.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_ENVYCODE:
                    WriteConfigFont("EnvyCodeR Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "EnvyCodeR Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: EnvyCodeR Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_FANTASQUE:
                    WriteConfigFont("FantasqueSansM Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "FantasqueSansM Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: FantasqueSansM Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_FIRACODE:
                    WriteConfigFont("FiraCode Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "FiraCode Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: FiraCode Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_FIRAMONO:
                    WriteConfigFont("FiraMono Nerd Font Propo.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "FiraMono Nerd Font Propo.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: FiraMono Nerd Font Propo.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_GEIST:
                    WriteConfigFont("GeistMono NFM.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "GeistMono NFM.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: GeistMono NFM.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_HASKLUG:
                    WriteConfigFont("Hasklug Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Hasklug Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: Hasklug Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_HURMIT:
                    WriteConfigFont("Hurmit Nerd Font Propo.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Hurmit Nerd Font Propo.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: Hurmit Nerd Font Propo.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_IMWRITING:
                    WriteConfigFont("iMWritingQuat Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "iMWritingQuat Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: iMWritingQuat Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_INCONSOLATA_GO:
                    WriteConfigFont("InconsolataGo Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "InconsolataGo Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: InconsolataGo Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_INCONSOLATA_LGC:
                    WriteConfigFont("Inconsolata LGC Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Inconsolata LGC Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: Inconsolata LGC Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_INCONSOLATA:
                    WriteConfigFont("Inconsolata Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Inconsolata Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: Inconsolata Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_INTONE:
                    WriteConfigFont("IntoneMono NF.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "IntoneMono NF.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: IntoneMono NF.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_IOSEVKA:
                    WriteConfigFont("Iosevka NF SemiBold.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Iosevka NF SemiBold.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: Iosevka NF SemiBold.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_IOSEVKA_TERM:
                    WriteConfigFont("IosevkaTermSlab NFP Medium.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "IosevkaTermSlab NFP Medium.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: IosevkaTermSlab NFP Medium.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_JETBRAINS:
                    WriteConfigFont("JetBrainsMonoNL NFP SemiBold.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "JetBrainsMonoNL NFP SemiBold.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: JetBrainsMonoNL NFP SemiBold.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_LEKTON:
                    WriteConfigFont("Lekton Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Lekton Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: Lekton Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_LILEX:
                    WriteConfigFont("Lilex Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Lilex Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: Lilex Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_LIBERATION:
                    WriteConfigFont("LiterationSerif Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "LiterationSerif Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: LiterationSerif Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_MARTIAN:
                    WriteConfigFont("MartianMono NFM Cond.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "MartianMono NFM Cond.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: MartianMono NFM Cond.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_MONONOKI:
                    WriteConfigFont("Mononoki Nerd Font.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Mononoki Nerd Font.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: Mononoki Nerd Font.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_OVERPASS:
                    WriteConfigFont("OverpassM Nerd Font Mono SemBd.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "OverpassM Nerd Font Mono SemBd.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: OverpassM Nerd Font Mono SemBd.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_RECMONO:
                    WriteConfigFont("RecMonoCasual Nerd Font Mono.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "RecMonoCasual Nerd Font Mono.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: RecMonoCasual Nerd Font Mono.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_TERMINESS:
                    WriteConfigFont("Terminess Nerd Font Propo.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "Terminess Nerd Font Propo.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: Terminess Nerd Font Propo.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_VICTORMONO:
                    WriteConfigFont("VictorMono NFP Medium.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "VictorMono NFP Medium.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: VictorMono NFP Medium.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_ZEDMONO:
                    WriteConfigFont("ZedMono NF.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "ZedMono NF.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: ZedMono NF.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDC_FONT_GOHUFONT:
                    WriteConfigFont("GohuFont uni11 Nerd Font Mono.ttf");
                    if (!LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), "GohuFont uni11 Nerd Font Mono.ttf")) {
                        char errorMsg[256];
                        snprintf(errorMsg, sizeof(errorMsg), "Failed to load font: GohuFont uni11 Nerd Font Mono.ttf");
                        MessageBox(hwnd, errorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);
                    }
                    goto refresh_window;
                case CLOCK_IDM_SHOW_CURRENT_TIME: { // Show Current Time toggle
                    CLOCK_SHOW_CURRENT_TIME = !CLOCK_SHOW_CURRENT_TIME;
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        KillTimer(hwnd, 1);  // 停止倒计时定时器
                        elapsed_time = 0;
                        CLOCK_LAST_TIME_UPDATE = time(NULL);
                        SetTimer(hwnd, 1, 1000, NULL);  // 重新启动定时器
                    } else {
                        KillTimer(hwnd, 1);  // 停止当前时间定时器
                        elapsed_time = CLOCK_TOTAL_TIME;  // 设置为总时间，这样就不会显示任何内容
                        message_shown = 1;  // 防止触发超时动作
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_24HOUR_FORMAT: { // 24-Hour Format toggle
                    CLOCK_USE_24HOUR = !CLOCK_USE_24HOUR;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_SHOW_SECONDS: { // Show Seconds toggle
                    CLOCK_SHOW_SECONDS = !CLOCK_SHOW_SECONDS;
                    InvalidateRect(hwnd, NULL, TRUE);
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
    Sleep(2);  // 最小延迟
    keybd_event(VK_MEDIA_STOP, 0, KEYEVENTF_KEYUP, 0);
    Sleep(5);  // 减少命令间延迟

    for (int i = 0; i < 2; i++) {  // 改回2次循环
        keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
        Sleep(2);  // 最小延迟
        keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
        Sleep(5);  // 减少命令间延迟
    }
}

BOOL OpenFileDialog(HWND hwnd, char* filePath, DWORD maxPath) {
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "All Files\0*.*\0"
                      "Audio Files\0*.mp3;*.wav;*.m4a;*.wma\0"
                      "Video Files\0*.mp4;*.avi;*.mkv;*.wmv\0"
                      "Applications\0*.exe\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = maxPath;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    return GetOpenFileNameA(&ofn);
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
            
            strncpy(CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path, path, MAX_PATH - 1);
            
            char *filename = strrchr(path, '\\');
            if (filename) {
                filename++; // 跳过反斜杠
            } else {
                filename = path;
            }
            strncpy(CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].name, filename, MAX_PATH - 1);
            
            CLOCK_RECENT_FILES_COUNT++;
        }
    }
    
    fclose(file);
}

void SaveRecentFile(const char* filePath) {
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        if (strcmp(CLOCK_RECENT_FILES[i].path, filePath) == 0) {
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
    
    char *filename = strrchr(filePath, '\\');
    if (filename) {
        filename++; // 跳过反斜杠
    } else {
        filename = (char*)filePath;
    }
    strncpy(CLOCK_RECENT_FILES[0].name, filename, MAX_PATH - 1);
    CLOCK_RECENT_FILES[0].name[MAX_PATH - 1] = '\0';
    
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
        if (strncmp(line, "CLOCK_RECENT_FILE", 16) != 0 && 
            strncmp(line, "CLOCK_TIMEOUT_FILE", 17) != 0) {
            strcat(new_config, line);
            strcat(new_config, "\n");
        }
        line = strtok(NULL, "\n");
    }
    
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        char recent_file_line[MAX_PATH + 20];
        snprintf(recent_file_line, sizeof(recent_file_line), 
                "CLOCK_RECENT_FILE=%s\n", CLOCK_RECENT_FILES[i].path);
        strcat(new_config, recent_file_line);
    }
    
    if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        char timeout_file_line[MAX_PATH + 20];
        snprintf(timeout_file_line, sizeof(timeout_file_line),
                "CLOCK_TIMEOUT_FILE=%s\n", CLOCK_TIMEOUT_FILE_PATH);
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
    nid.dwInfoFlags = NIIF_NONE;  // 不显示任何图标
    strncpy(nid.szInfo, "Time is up!", sizeof(nid.szInfo) - 1);
    nid.szInfoTitle[0] = '\0';  // 不显示标题
    nid.uTimeout = 10000;  // 显示10秒

    Shell_NotifyIcon(NIM_MODIFY, &nid);

    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.szInfo[0] = '\0';
    nid.szInfoTitle[0] = '\0';
}
