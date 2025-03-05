/**
 * @file tray_menu.c
 * @brief 系统托盘菜单功能实现
 * 
 * 本文件实现了应用程序的系统托盘菜单，包括右键菜单、颜色选择菜单等功能。
 * 提供了丰富的应用程序设置选项和用户交互界面。
 */

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "../include/language.h"
#include "../include/tray_menu.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../resource/resource.h"

/// @name 外部变量声明
/// @{
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_USE_24HOUR;
extern BOOL CLOCK_SHOW_SECONDS;
extern BOOL CLOCK_COUNT_UP;
extern BOOL CLOCK_IS_PAUSED;
extern BOOL CLOCK_EDIT_MODE;
extern char CLOCK_STARTUP_MODE[20];
extern char CLOCK_TEXT_COLOR[10];
extern char FONT_FILE_NAME[];
extern char PREVIEW_FONT_NAME[];
extern char PREVIEW_INTERNAL_NAME[];
extern BOOL IS_PREVIEWING;
extern int time_options[];
extern int time_options_count;
extern int CLOCK_TOTAL_TIME;
extern int countdown_elapsed_time;
extern char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH];
extern char CLOCK_TIMEOUT_TEXT[50];
/// @}

/// @name 外部函数声明
/// @{
extern void GetConfigPath(char* path, size_t size);
extern BOOL IsAutoStartEnabled(void);
extern void WriteConfigStartupMode(const char* mode);
extern void ClearColorOptions(void);
extern void AddColorOption(const char* color);
/// @}

/**
 * @brief 超时动作类型枚举
 * 
 * 定义了时间结束后可以执行的不同操作类型
 */
typedef enum {
    TIMEOUT_ACTION_MESSAGE = 0,   ///< 显示消息
    TIMEOUT_ACTION_LOCK = 1,      ///< 锁定屏幕
    TIMEOUT_ACTION_SHUTDOWN = 2,  ///< 关机
    TIMEOUT_ACTION_RESTART = 3,   ///< 重启
    TIMEOUT_ACTION_OPEN_FILE = 4  ///< 打开文件
} TimeoutActionType;

extern TimeoutActionType CLOCK_TIMEOUT_ACTION;

/**
 * @brief 最近文件结构体
 * 
 * 存储最近使用过的文件路径和名称信息
 */
typedef struct {
    char path[MAX_PATH];  ///< 文件完整路径
    char name[MAX_PATH];  ///< 文件显示名称
} RecentFile;

extern RecentFile CLOCK_RECENT_FILES[];
extern int CLOCK_RECENT_FILES_COUNT;

/**
 * @brief 显示颜色和设置菜单
 * @param hwnd 窗口句柄
 * 
 * 创建并显示应用程序的主设置菜单，包括编辑模式、超时动作、
 * 预设管理、字体选择、颜色设置和关于信息等选项。
 */
void ShowColorMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    
    // 添加编辑模式选项
    AppendMenuW(hMenu, MF_STRING | (CLOCK_EDIT_MODE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDC_EDIT_MODE, 
               GetLocalizedString(L"编辑模式", L"Edit Mode"));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // 超时动作菜单
    HMENU hTimeoutMenu = CreatePopupMenu();
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHOW_MESSAGE, 
               GetLocalizedString(L"显示消息", L"Show Message"));

    // 创建打开文件子菜单
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

    // 设置打开文件菜单文本，显示当前选择的文件名
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

    // 将打开文件子菜单添加到超时动作菜单
    AppendMenuW(hTimeoutMenu, MF_POPUP | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE ? MF_CHECKED : MF_UNCHECKED),
               (UINT_PTR)hOpenFileMenu, menuText);
               
    // 添加其他超时动作选项
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_LOCK ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_LOCK_SCREEN, 
               GetLocalizedString(L"锁定屏幕", L"Lock Screen"));
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHUTDOWN, 
               GetLocalizedString(L"关机", L"Shutdown"));
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_RESTART, 
               GetLocalizedString(L"重启", L"Restart"));

    // 将超时动作菜单添加到主菜单
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeoutMenu, 
                GetLocalizedString(L"超时动作", L"Timeout Action"));

    // 预设管理菜单
    HMENU hTimeOptionsMenu = CreatePopupMenu();
    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDC_MODIFY_TIME_OPTIONS,
                GetLocalizedString(L"修改快捷时间选项", L"Modify Time Options"));
    
    // 启动设置子菜单
    HMENU hStartupSettingsMenu = CreatePopupMenu();

    // 读取当前启动模式
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
    
    // 添加启动模式选项
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

    // 添加开机自启动选项
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
            (IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED),
            CLOCK_IDC_AUTO_START,
            GetLocalizedString(L"开机自启动", L"Start with Windows"));

    // 将启动设置菜单添加到预设管理菜单
    AppendMenuW(hTimeOptionsMenu, MF_POPUP, (UINT_PTR)hStartupSettingsMenu,
                GetLocalizedString(L"启动设置", L"Startup Settings"));

    // 将预设管理菜单添加到主菜单
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeOptionsMenu,
                GetLocalizedString(L"预设管理", L"Preset Manager"));
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // 字体菜单
    HMENU hMoreFontsMenu = CreatePopupMenu();
    HMENU hFontSubMenu = CreatePopupMenu();
    
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

    // 颜色菜单
    HMENU hColorSubMenu = CreatePopupMenu();
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

    // 自定义颜色选项
    HMENU hCustomizeMenu = CreatePopupMenu();
    AppendMenuW(hCustomizeMenu, MF_STRING, CLOCK_IDC_COLOR_VALUE, 
                GetLocalizedString(L"颜色值", L"Color Value"));
    AppendMenuW(hCustomizeMenu, MF_STRING, CLOCK_IDC_COLOR_PANEL, 
                GetLocalizedString(L"颜色面板", L"Color Panel"));

    AppendMenuW(hColorSubMenu, MF_POPUP, (UINT_PTR)hCustomizeMenu, 
                GetLocalizedString(L"自定义", L"Customize"));

    // 将字体和颜色菜单添加到主菜单
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu, 
                GetLocalizedString(L"字体", L"Font"));
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hColorSubMenu, 
                GetLocalizedString(L"颜色", L"Color"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // 关于菜单
    HMENU hAboutMenu = CreatePopupMenu();
    wchar_t version_text[32];
    _snwprintf(version_text, sizeof(version_text)/sizeof(wchar_t), 
               GetLocalizedString(L"当前版本: %hs", L"Version: %hs"), 
               CATIME_VERSION);
    AppendMenuW(hAboutMenu, MF_STRING | MF_DISABLED, 0, version_text);

    // 反馈菜单
    HMENU hFeedbackMenu = CreatePopupMenu();
    AppendMenuW(hFeedbackMenu, MF_STRING, CLOCK_IDM_FEEDBACK_GITHUB, L"GitHub");
    AppendMenuW(hFeedbackMenu, MF_STRING, CLOCK_IDM_FEEDBACK_BILIBILI, L"BiliBili");
    AppendMenuW(hAboutMenu, MF_POPUP, (UINT_PTR)hFeedbackMenu, 
                GetLocalizedString(L"反馈", L"Feedback"));

    // 语言选择菜单
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

    // 更新菜单
    HMENU hUpdateMenu = CreatePopupMenu();
    AppendMenuW(hUpdateMenu, MF_STRING, CLOCK_IDM_UPDATE_GITHUB, L"GitHub");
    AppendMenuW(hUpdateMenu, MF_STRING, CLOCK_IDM_UPDATE_123PAN,
                GetLocalizedString(L"123云盘", L"123Pan"));
    AppendMenuW(hUpdateMenu, MF_STRING, CLOCK_IDM_UPDATE_LANZOU,
                GetLocalizedString(L"蓝奏云 (密码: 1234)", L"LanzouCloud (pwd: 1234)"));

    AppendMenuW(hAboutMenu, MF_POPUP, (UINT_PTR)hUpdateMenu,
                GetLocalizedString(L"检查更新", L"Check for Updates"));
    
    // 将关于菜单添加到主菜单
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAboutMenu,
                GetLocalizedString(L"帮助", L"Help"));

    // 添加重置和退出选项
    AppendMenuW(hMenu, MF_STRING, 200,
                GetLocalizedString(L"重置", L"Reset"));
    AppendMenuW(hMenu, MF_STRING, 109,
                GetLocalizedString(L"退出", L"Exit"));
    
    // 显示菜单
    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

/**
 * @brief 显示托盘右键菜单
 * @param hwnd 窗口句柄
 * 
 * 创建并显示系统托盘右键菜单，包含时间设置、显示模式切换和快捷时间选项。
 * 根据当前应用程序状态动态调整菜单项。
 */
void ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    
    // 添加设置时间选项
    AppendMenuW(hMenu, MF_STRING, 101, 
                GetLocalizedString(L"设置时间", L"Set Time"));
    
    // 添加分隔线
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // 时间显示菜单
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
    
    // 将时间显示菜单添加到主菜单
    AppendMenuW(hMenu, MF_POPUP | (CLOCK_SHOW_CURRENT_TIME ? MF_CHECKED : MF_UNCHECKED), 
               (UINT_PTR)hTimeMenu,
               GetLocalizedString(L"时间显示", L"Time Display"));

    // 正计时菜单
    HMENU hCountUpMenu = CreatePopupMenu();
    AppendMenuW(hCountUpMenu, MF_STRING, CLOCK_IDM_COUNT_UP_START,
        CLOCK_COUNT_UP ? 
            (CLOCK_IS_PAUSED ? 
                GetLocalizedString(L"继续", L"Resume") : 
                GetLocalizedString(L"暂停", L"Pause")) :
            GetLocalizedString(L"开始", L"Start"));
            
    if (CLOCK_COUNT_UP) {
        AppendMenuW(hCountUpMenu, MF_STRING, CLOCK_IDM_COUNT_UP_RESET,
            GetLocalizedString(L"重新开始", L"Restart"));
    }
               
    // 将正计时菜单添加到主菜单
    AppendMenuW(hMenu, MF_POPUP | (CLOCK_COUNT_UP ? MF_CHECKED : MF_UNCHECKED),
               (UINT_PTR)hCountUpMenu,
               GetLocalizedString(L"正计时", L"Count Up"));

    // 倒计时菜单
    HMENU hCountdownMenu = CreatePopupMenu();
    // 判断是否需要显示完整倒计时菜单（非正计时、非显示时间、倒计时进行中）
    BOOL isWindowVisible = IsWindowVisible(hwnd);
    if (!CLOCK_COUNT_UP && !CLOCK_SHOW_CURRENT_TIME && 
        CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME && isWindowVisible) 
    {
        // 显示暂停/继续和重新开始
        AppendMenuW(hCountdownMenu, MF_STRING,
            CLOCK_IDM_COUNTDOWN_START_PAUSE,
            CLOCK_IS_PAUSED ? 
                GetLocalizedString(L"继续", L"Resume") :
                GetLocalizedString(L"暂停", L"Pause"));
        
        AppendMenuW(hCountdownMenu, MF_STRING,
            CLOCK_IDM_COUNTDOWN_RESET,
            GetLocalizedString(L"重新开始", L"Restart"));
    } else {
        // 其他情况（显示当前时间/正计时/倒计时未开始/已结束/窗口隐藏）显示开始
        AppendMenuW(hCountdownMenu, MF_STRING,
            CLOCK_IDM_COUNTDOWN_START_PAUSE,
            GetLocalizedString(L"开始", L"Start"));
    }

    // 将倒计时菜单添加到主菜单，并根据状态设置勾选
    AppendMenuW(hMenu, MF_POPUP | 
        ((!CLOCK_COUNT_UP && !CLOCK_SHOW_CURRENT_TIME && 
          CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME && isWindowVisible) ? 
            MF_CHECKED : MF_UNCHECKED), 
        (UINT_PTR)hCountdownMenu,
        GetLocalizedString(L"倒计时", L"Countdown"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // 添加快捷时间选项
    for (int i = 0; i < time_options_count; i++) {
        wchar_t menu_item[20];
        _snwprintf(menu_item, sizeof(menu_item)/sizeof(wchar_t), L"%d", time_options[i]);
        AppendMenuW(hMenu, MF_STRING, 102 + i, menu_item);
    }

    // 显示菜单
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}