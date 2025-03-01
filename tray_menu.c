/**
 * @file tray_menu.c
 * @brief 托盘菜单模块的实现
 * 
 * 本文件实现了系统托盘图标的右键菜单和颜色选择菜单功能。
 */

#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include "include/tray_menu.h"
#include "include/language.h"
#include "include/font.h"
#include "include/color.h"
#include "resource/resource.h"

// 外部变量声明
extern char CLOCK_TEXT_COLOR[];
extern char FONT_FILE_NAME[];
extern float CLOCK_FONT_SCALE_FACTOR;
extern float CLOCK_WINDOW_SCALE;
extern BOOL CLOCK_EDIT_MODE;
extern int CLOCK_TIMEOUT_ACTION;
extern char CLOCK_TIMEOUT_FILE_PATH[];
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_USE_24HOUR;
extern BOOL CLOCK_SHOW_SECONDS;
extern BOOL CLOCK_COUNT_UP;
extern char CLOCK_STARTUP_MODE[];
extern BOOL IS_PREVIEWING;
extern BOOL IS_COLOR_PREVIEWING;
extern char PREVIEW_COLOR[];
extern char PREVIEW_FONT_NAME[];

// 相关函数声明
void GetConfigPath(char* path, size_t size);
void WriteConfigFont(const char* font_file_name);
void WriteConfigColor(const char* color_input);
void WriteConfigTimeoutAction(const char* action);
void WriteConfigEditMode(const char* mode);
void WriteConfigStartupMode(const char* mode);
BOOL IsAutoStartEnabled(void);
BOOL CreateShortcut(void);
BOOL RemoveShortcut(void);
void SaveRecentFile(const char* filePath);
void ExitProgram(HWND hwnd);

// 超时动作的枚举值
enum {
    TIMEOUT_ACTION_MESSAGE = 0,
    TIMEOUT_ACTION_OPEN_FILE = 1,
    TIMEOUT_ACTION_LOCK = 2,
    TIMEOUT_ACTION_SHUTDOWN = 3,
    TIMEOUT_ACTION_RESTART = 4
};

// 最近打开的文件结构体
typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
} RecentFile;

// 外部变量
extern RecentFile CLOCK_RECENT_FILES[];
extern int CLOCK_RECENT_FILES_COUNT;
extern int MAX_RECENT_FILES;

/**
 * @brief 显示系统托盘的右键菜单
 * @param hwnd 窗口句柄
 */
void ShowContextMenu(HWND hwnd)
{
    HMENU hMenu = CreatePopupMenu();
    
    // 添加程序名称（不可点击的标题项）
    AppendMenuW(hMenu, MF_STRING | MF_DISABLED, 0, L"Catime");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // 添加当前时间显示选项
    AppendMenuW(hMenu, MF_STRING | (CLOCK_SHOW_CURRENT_TIME ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHOW_CURRENT_TIME, 
               GetLocalizedString(L"显示当前时间", L"Show Current Time"));
    
    // 24小时制选项
    AppendMenuW(hMenu, MF_STRING | (CLOCK_USE_24HOUR ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_24HOUR_FORMAT, 
               GetLocalizedString(L"24小时制", L"24-Hour Format"));
    
    // 显示秒数选项               
    AppendMenuW(hMenu, MF_STRING | (CLOCK_SHOW_SECONDS ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHOW_SECONDS, 
               GetLocalizedString(L"显示秒数", L"Show Seconds"));
               
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // 计时器选项
    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_COUNTDOWN_START_PAUSE, 
               GetLocalizedString(L"开始/暂停 倒计时", L"Start/Pause Countdown"));
    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_COUNTDOWN_RESET, 
               GetLocalizedString(L"重置倒计时", L"Reset Countdown"));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // 正计时选项
    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_COUNT_UP_START, 
               GetLocalizedString(L"开始/暂停 正计时", L"Start/Pause Stopwatch"));
    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_COUNT_UP_RESET, 
               GetLocalizedString(L"重置正计时", L"Reset Stopwatch"));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // 编辑模式选项
    AppendMenuW(hMenu, MF_STRING | (CLOCK_EDIT_MODE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDC_EDIT_MODE, 
               GetLocalizedString(L"编辑模式", L"Edit Mode"));
               
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // 更多选项
    AppendMenuW(hMenu, MF_STRING, 0, 
               GetLocalizedString(L"更多选项...", L"More Options..."));
    
    // 退出选项
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 109, 
               GetLocalizedString(L"退出", L"Exit"));
    
    // 显示菜单
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

/**
 * @brief 显示颜色选择菜单
 * @param hwnd 窗口句柄
 */
void ShowColorMenu(HWND hwnd) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *file = fopen(config_path, "r");
    if (file) {
        char line[1024];
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
                break;
            }
        }
        fclose(file);
    }

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
            
            wchar_t wFileName[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[i].name, -1, wFileName, MAX_PATH);
            
            AppendMenuW(hOpenFileMenu, MF_STRING | (isCurrentFile ? MF_CHECKED : MF_UNCHECKED), 
                      CLOCK_IDM_RECENT_FILE_1 + i, 
                      wFileName);
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
            wchar_t wFileName[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, filename, -1, wFileName, MAX_PATH);
            
            _snwprintf(displayText, MAX_PATH, 
                      GetLocalizedString(L"打开: %ls", L"Open: %ls"), 
                      wFileName);
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

    HMENU hTimeOptionsMenu = CreatePopupMenu();
    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDC_MODIFY_TIME_OPTIONS,
                GetLocalizedString(L"修改快捷时间选项", L"Modify Time Options"));
    
    HMENU hStartupSettingsMenu = CreatePopupMenu();

    char currentStartupMode[20] = "COUNTDOWN";
    char configPath[MAX_PATH];  
    GetConfigPath(configPath, MAX_PATH);
    FILE *configFile = fopen(configPath, "r");  
    if (configFile) {
        char line[256];
        while (fgets(line, sizeof(line), configFile)) {
            if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
                sscanf(line, "STARTUP_MODE=%19s", currentStartupMode);
                break;
            }
        }
        fclose(configFile);
    }
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "COUNTDOWN") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_SET_COUNTDOWN_TIME,
                GetLocalizedString(L"倒计时", L"Countdown"));
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "COUNT_UP") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_START_COUNT_UP,
                GetLocalizedString(L"正计时", L"Stopwatch"));
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "SHOW_TIME") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_START_SHOW_TIME,
                GetLocalizedString(L"显示当前时间", L"Show Current Time"));
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "NO_DISPLAY") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_START_NO_DISPLAY,
                GetLocalizedString(L"不显示", L"No Display"));
    
    AppendMenuW(hStartupSettingsMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
            (IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED),
            CLOCK_IDC_AUTO_START,
            GetLocalizedString(L"开机自启动", L"Start with Windows"));

    AppendMenuW(hTimeOptionsMenu, MF_POPUP, (UINT_PTR)hStartupSettingsMenu,
                GetLocalizedString(L"启动设置", L"Startup Settings"));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeOptionsMenu,
                GetLocalizedString(L"预设管理", L"Preset Manager"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // 创建"更多"子菜单
    HMENU hMoreFontsMenu = CreatePopupMenu();

    // 先添加常用字体到主菜单
    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
        // 这些字体保留在主菜单
        if (strcmp(fontResources[i].fontName, "Terminess Nerd Font Propo Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "DaddyTimeMono Nerd Font Propo Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Foldit SemiBold Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Jacquarda Bastarda 9 Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Moirai One Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Silkscreen Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Pixelify Sans Medium Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Glitch Pop Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Burned Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Glitch Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ProFont IIx Nerd Font.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Wallpoet Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Yesteryear Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ZCOOL KuaiLe Essence.ttf") == 0) {
            
            BOOL isCurrentFont = strcmp(FONT_FILE_NAME, fontResources[i].fontName) == 0;
            char displayName[100];
            strncpy(displayName, fontResources[i].fontName, sizeof(displayName) - 1);
            displayName[sizeof(displayName) - 1] = '\0';
            char* dot = strstr(displayName, ".ttf");
            if (dot) *dot = '\0';
            
            AppendMenu(hFontSubMenu, MF_STRING | (isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                      fontResources[i].menuId, displayName);
        }
    }

    AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);

    // 将其他字体添加到"更多"子菜单
    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
        // 排除已经添加到主菜单的字体
        if (strcmp(fontResources[i].fontName, "Terminess Nerd Font Propo Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "DaddyTimeMono Nerd Font Propo Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Foldit SemiBold Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Jacquarda Bastarda 9 Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Moirai One Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Silkscreen Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Pixelify Sans Medium Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Glitch Pop Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Burned Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Glitch Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ProFont IIx Nerd Font.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Wallpoet Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Yesteryear Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ZCOOL KuaiLe Essence.ttf") == 0) {
            continue;
        }

        BOOL isCurrentFont = strcmp(FONT_FILE_NAME, fontResources[i].fontName) == 0;
        char displayName[100];
        strncpy(displayName, fontResources[i].fontName, sizeof(displayName) - 1);
        displayName[sizeof(displayName) - 1] = '\0';
        char* dot = strstr(displayName, ".ttf");
        if (dot) *dot = '\0';
        
        AppendMenu(hMoreFontsMenu, MF_STRING | (isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                  fontResources[i].menuId, displayName);
    }

    // 将"更多"子菜单添加到主字体菜单
    AppendMenuW(hFontSubMenu, MF_POPUP, (UINT_PTR)hMoreFontsMenu, GetLocalizedString(L"更多", L"More"));

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

    HMENU hCustomizeMenu = CreatePopupMenu();
    AppendMenuW(hCustomizeMenu, MF_STRING, CLOCK_IDC_COLOR_VALUE, 
                GetLocalizedString(L"颜色值", L"Color Value"));
    AppendMenuW(hCustomizeMenu, MF_STRING, CLOCK_IDC_COLOR_PANEL, 
                GetLocalizedString(L"颜色面板", L"Color Panel"));

    AppendMenuW(hColorSubMenu, MF_POPUP, (UINT_PTR)hCustomizeMenu, 
                GetLocalizedString(L"自定义", L"Customize"));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu, 
                GetLocalizedString(L"字体", L"Font"));
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hColorSubMenu, 
                GetLocalizedString(L"颜色", L"Color"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    HMENU hAboutMenu = CreatePopupMenu();
    wchar_t version_text[32];
    _snwprintf(version_text, sizeof(version_text)/sizeof(wchar_t), 
               GetLocalizedString(L"当前版本: %hs", L"Version: %hs"), 
               CATIME_VERSION);
    AppendMenuW(hAboutMenu, MF_STRING | MF_DISABLED, 0, version_text);

    HMENU hFeedbackMenu = CreatePopupMenu();
    AppendMenuW(hFeedbackMenu, MF_STRING, CLOCK_IDM_FEEDBACK_GITHUB, L"GitHub");
    AppendMenuW(hFeedbackMenu, MF_STRING, CLOCK_IDM_FEEDBACK_BILIBILI, L"BiliBili");
    AppendMenuW(hAboutMenu, MF_POPUP, (UINT_PTR)hFeedbackMenu, 
                GetLocalizedString(L"反馈", L"Feedback"));

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