/**
 * @file tray_menu.c
 * @brief 系统托盘菜单功能实现
 * 
 * 本文件实现了应用程序的系统托盘菜单功能，包括：
 * - 右键菜单及其子菜单
 * - 颜色选择菜单
 * - 字体设置菜单
 * - 超时动作设置
 * - 番茄钟功能
 * - 预设时间管理
 * - 多语言界面支持
 */

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "../include/language.h"
#include "../include/tray_menu.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/drag_scale.h"
#include "../include/pomodoro.h"
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
extern BOOL CLOCK_WINDOW_TOPMOST;       ///< 窗口是否置顶

// 添加番茄钟相关变量声明
extern int POMODORO_WORK_TIME;      ///< 工作时间(秒)
extern int POMODORO_SHORT_BREAK;    ///< 短休息时间(秒)
extern int POMODORO_LONG_BREAK;     ///< 长休息时间(秒)
extern int POMODORO_LOOP_COUNT;     ///< 循环次数

// 番茄钟时间数组和计数变量
#define MAX_POMODORO_TIMES 10
extern int POMODORO_TIMES[MAX_POMODORO_TIMES]; // 存储所有番茄钟时间
extern int POMODORO_TIMES_COUNT;              // 实际的番茄钟时间数量

// 在外部变量声明部分添加
extern char CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];   ///< 超时打开网站的URL
extern int current_pomodoro_time_index; // 当前番茄钟时间索引
extern POMODORO_PHASE current_pomodoro_phase; // 番茄钟阶段
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
 * 定义了计时结束后可执行的不同操作类型
 */
typedef enum {
    TIMEOUT_ACTION_MESSAGE = 0,   ///< 显示消息提醒
    TIMEOUT_ACTION_LOCK = 1,      ///< 锁定屏幕
    TIMEOUT_ACTION_SHUTDOWN = 2,  ///< 关机
    TIMEOUT_ACTION_RESTART = 3,   ///< 重启系统
    TIMEOUT_ACTION_OPEN_FILE = 4, ///< 打开指定文件
    TIMEOUT_ACTION_SHOW_TIME = 5, ///< 显示当前时间
    TIMEOUT_ACTION_COUNT_UP = 6,   ///< 切换到正计时模式
    TIMEOUT_ACTION_OPEN_WEBSITE = 7, ///< 打开网站
    TIMEOUT_ACTION_SLEEP = 8        ///< 睡眠
} TimeoutActionType;

extern TimeoutActionType CLOCK_TIMEOUT_ACTION;

/**
 * @brief 从配置文件读取超时动作设置
 * 
 * 读取配置文件中保存的超时动作设置，并更新全局变量 CLOCK_TIMEOUT_ACTION
 */
void ReadTimeoutActionFromConfig() {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    FILE *configFile = fopen(configPath, "r");
    if (configFile) {
        char line[256];
        while (fgets(line, sizeof(line), configFile)) {
            if (strncmp(line, "TIMEOUT_ACTION=", 15) == 0) {
                int action = 0;
                sscanf(line, "TIMEOUT_ACTION=%d", &action);
                CLOCK_TIMEOUT_ACTION = (TimeoutActionType)action;
                break;
            }
        }
        fclose(configFile);
    }
}

/**
 * @brief 最近文件结构体
 * 
 * 存储最近使用过的文件信息，包括完整路径和显示名称
 */
typedef struct {
    char path[MAX_PATH];  ///< 文件完整路径
    char name[MAX_PATH];  ///< 文件显示名称（可能是截断后的）
} RecentFile;

extern RecentFile CLOCK_RECENT_FILES[];
extern int CLOCK_RECENT_FILES_COUNT;

/**
 * @brief 格式化番茄钟时间到宽字符串
 * @param seconds 秒数
 * @param buffer 输出缓冲区
 * @param bufferSize 缓冲区大小
 */
static void FormatPomodoroTime(int seconds, wchar_t* buffer, size_t bufferSize) {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    int hours = minutes / 60;
    minutes %= 60;
    
    if (hours > 0) {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d:%02d", hours, minutes, secs);
    } else {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d", minutes, secs);
    }
}

/**
 * @brief 截断过长的文件名
 * 
 * @param fileName 原始文件名
 * @param truncated 截断后的文件名缓冲区
 * @param maxLen 最大显示长度（不包括结束符）
 * 
 * 如果文件名超过指定长度，采用"前12个字符...后12个字符.扩展名"的格式进行智能截断。
 * 此函数保留文件扩展名，确保用户能识别文件类型。
 */
void TruncateFileName(const wchar_t* fileName, wchar_t* truncated, size_t maxLen) {
    if (!fileName || !truncated || maxLen <= 7) return; // 至少需要显示"x...y"
    
    size_t nameLen = wcslen(fileName);
    if (nameLen <= maxLen) {
        // 文件名未超过限制长度，直接复制
        wcscpy(truncated, fileName);
        return;
    }
    
    // 查找最后一个点号位置(扩展名分隔符)
    const wchar_t* lastDot = wcsrchr(fileName, L'.');
    const wchar_t* fileNameNoExt = fileName;
    const wchar_t* ext = L"";
    size_t nameNoExtLen = nameLen;
    size_t extLen = 0;
    
    if (lastDot && lastDot != fileName) {
        // 有有效的扩展名
        ext = lastDot;  // 包含点号的扩展名
        extLen = wcslen(ext);
        nameNoExtLen = lastDot - fileName;  // 不含扩展名的文件名长度
    }
    
    // 如果纯文件名长度小于等于27字符(12+3+12)，使用旧方式截断
    if (nameNoExtLen <= 27) {
        // 简单截断主文件名，保留扩展名
        wcsncpy(truncated, fileName, maxLen - extLen - 3);
        truncated[maxLen - extLen - 3] = L'\0';
        wcscat(truncated, L"...");
        wcscat(truncated, ext);
        return;
    }
    
    // 使用新的截断方式：前12个字符 + ... + 后12个字符 + 扩展名
    wchar_t buffer[MAX_PATH];
    
    // 复制前12个字符
    wcsncpy(buffer, fileName, 12);
    buffer[12] = L'\0';
    
    // 添加省略号
    wcscat(buffer, L"...");
    
    // 复制后12个字符(不含扩展名部分)
    wcsncat(buffer, fileName + nameNoExtLen - 12, 12);
    
    // 添加扩展名
    wcscat(buffer, ext);
    
    // 复制结果到输出缓冲区
    wcscpy(truncated, buffer);
}

/**
 * @brief 显示颜色和设置菜单
 * 
 * @param hwnd 窗口句柄
 * 
 * 创建并显示应用程序的主设置菜单，包括：
 * - 编辑模式开关
 * - 超时动作设置
 * - 预设时间管理
 * - 启动模式设置
 * - 字体选择
 * - 颜色设置
 * - 语言选择
 * - 帮助和关于信息
 */
void ShowColorMenu(HWND hwnd) {
    // 在创建菜单前先读取配置文件中的超时动作设置
    ReadTimeoutActionFromConfig();
    
    // 设置鼠标光标为默认箭头，防止显示等待光标
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    
    HMENU hMenu = CreatePopupMenu();
    
    // 添加编辑模式选项
    AppendMenuW(hMenu, MF_STRING | (CLOCK_EDIT_MODE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDC_EDIT_MODE, 
               GetLocalizedString(L"编辑模式", L"Edit Mode"));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // 超时动作菜单
    HMENU hTimeoutMenu = CreatePopupMenu();
    
    // 1. 显示消息
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHOW_MESSAGE, 
               GetLocalizedString(L"显示消息", L"Show Message"));

    // 2. 显示当前时间
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHOW_TIME ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_TIMEOUT_SHOW_TIME, 
               GetLocalizedString(L"显示当前时间", L"Show Current Time"));

    // 3. 正计时
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_COUNT_UP ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_TIMEOUT_COUNT_UP, 
               GetLocalizedString(L"正计时", L"Count Up"));

    // 4. 锁定屏幕
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_LOCK ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_LOCK_SCREEN,
               GetLocalizedString(L"锁定屏幕", L"Lock Screen"));

    // 第一个分隔线
    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);

    // 5. 打开文件（子菜单）
    HMENU hFileMenu = CreatePopupMenu();

    // 先添加最近文件列表
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        wchar_t wFileName[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[i].name, -1, wFileName, MAX_PATH);
        
        // 截断过长的文件名
        wchar_t truncatedName[MAX_PATH];
        TruncateFileName(wFileName, truncatedName, 25); // 限制为25个字符
        
        // 检查是否是当前选择的文件，且当前超时动作为"打开文件"
        BOOL isCurrentFile = (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && 
                             strlen(CLOCK_TIMEOUT_FILE_PATH) > 0 && 
                             strcmp(CLOCK_RECENT_FILES[i].path, CLOCK_TIMEOUT_FILE_PATH) == 0);
        
        // 使用菜单项的勾选状态表示选中
        AppendMenuW(hFileMenu, MF_STRING | (isCurrentFile ? MF_CHECKED : 0), 
                   CLOCK_IDM_RECENT_FILE_1 + i, truncatedName);
    }
               
    // 如果有最近文件，添加分隔线
    if (CLOCK_RECENT_FILES_COUNT > 0) {
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
    }

    // 最后添加"浏览..."选项
    AppendMenuW(hFileMenu, MF_STRING, CLOCK_IDM_BROWSE_FILE,
               GetLocalizedString(L"浏览...", L"Browse..."));

    // 将"打开文件"作为子菜单添加到超时动作菜单中
    AppendMenuW(hTimeoutMenu, MF_POPUP | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE ? MF_CHECKED : MF_UNCHECKED), 
               (UINT_PTR)hFileMenu, 
               GetLocalizedString(L"打开文件/软件", L"Open File/Software"));

    // 6. 打开网站
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_WEBSITE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_OPEN_WEBSITE,
               GetLocalizedString(L"打开网站", L"Open Website"));

    // 第二个分隔线
    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);

    // 添加一个不可选择的提示选项
    AppendMenuW(hTimeoutMenu, MF_STRING | MF_GRAYED | MF_DISABLED, 
               0,  // 使用ID为0表示不可选菜单项
               GetLocalizedString(L"以下超时动作为一次性", L"Following actions are one-time only"));

    // 7. 关机
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHUTDOWN,
               GetLocalizedString(L"关机", L"Shutdown"));

    // 8. 重启
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_RESTART,
               GetLocalizedString(L"重启", L"Restart"));

    // 9. 睡眠
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SLEEP ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SLEEP,
               GetLocalizedString(L"睡眠", L"Sleep"));

    // 将超时动作菜单添加到主菜单
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeoutMenu, 
                GetLocalizedString(L"超时动作", L"Timeout Action"));

    // 预设管理菜单
    HMENU hTimeOptionsMenu = CreatePopupMenu();
    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDC_MODIFY_TIME_OPTIONS,
                GetLocalizedString(L"倒计时预设", L"Modify Quick Countdown Options"));
    
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

    // 添加通知设置菜单 - 修改为直接菜单项，不再使用子菜单
    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDM_NOTIFICATION_SETTINGS,
                GetLocalizedString(L"通知设置", L"Notification Settings"));

    // 将预设管理菜单添加到主菜单
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeOptionsMenu,
                GetLocalizedString(L"预设管理", L"Preset Management"));
    
    AppendMenuW(hTimeOptionsMenu, MF_STRING | (CLOCK_WINDOW_TOPMOST ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TOPMOST,
                GetLocalizedString(L"置顶", L"Always on Top"));

    // 在预设管理菜单之后添加"热键设置"选项
    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_HOTKEY_SETTINGS,
                GetLocalizedString(L"热键设置", L"Hotkey Settings"));

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
            strcmp(fontResources[i].fontName, "Rubik Burned Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Glitch Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ProFont IIx Nerd Font Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Wallpoet Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Yesteryear Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Pinyon Script Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ZCOOL KuaiLe Essence.ttf") == 0) {
            
            BOOL isCurrentFont = strcmp(FONT_FILE_NAME, fontResources[i].fontName) == 0;
            wchar_t wDisplayName[100];
            MultiByteToWideChar(CP_UTF8, 0, fontResources[i].fontName, -1, wDisplayName, 100);
            wchar_t* dot = wcsstr(wDisplayName, L".ttf");
            if (dot) *dot = L'\0';
            
            AppendMenuW(hFontSubMenu, MF_STRING | (isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                      fontResources[i].menuId, wDisplayName);
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
            strcmp(fontResources[i].fontName, "Rubik Burned Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Glitch Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ProFont IIx Nerd Font Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Wallpoet Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Yesteryear Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Pinyon Script Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ZCOOL KuaiLe Essence.ttf") == 0) {
            continue;
        }

        BOOL isCurrentFont = strcmp(FONT_FILE_NAME, fontResources[i].fontName) == 0;
        wchar_t wDisplayNameMore[100];
        MultiByteToWideChar(CP_UTF8, 0, fontResources[i].fontName, -1, wDisplayNameMore, 100);
        wchar_t* dot = wcsstr(wDisplayNameMore, L".ttf");
        if (dot) *dot = L'\0';
        
        AppendMenuW(hMoreFontsMenu, MF_STRING | (isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                  fontResources[i].menuId, wDisplayNameMore);
    }

    // 将"更多"子菜单添加到主字体菜单
    AppendMenuW(hFontSubMenu, MF_POPUP, (UINT_PTR)hMoreFontsMenu, GetLocalizedString(L"更多", L"More"));

    // 颜色菜单
    HMENU hColorSubMenu = CreatePopupMenu();
    // 预设颜色选项的菜单ID从201开始,到201+COLOR_OPTIONS_COUNT-1
    for (int i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        const char* hexColor = COLOR_OPTIONS[i].hexColor;
        
        MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
        mii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE | MIIM_FTYPE;
        mii.fType = MFT_STRING | MFT_OWNERDRAW;
        mii.fState = strcmp(CLOCK_TEXT_COLOR, hexColor) == 0 ? MFS_CHECKED : MFS_UNCHECKED;
        mii.wID = 201 + i;  // 预设颜色菜单项ID从201开始
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

    // 在这里添加"关于"菜单项
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_ABOUT, GetLocalizedString(L"关于", L"About"));

    // 添加分隔线
    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);

    // 添加"支持"选项 - 打开赞助网页
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_SUPPORT, GetLocalizedString(L"支持", L"Support"));
    
    // 添加"反馈"选项 - 根据语言打开不同的反馈链接
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_FEEDBACK, GetLocalizedString(L"反馈", L"Feedback"));
    
    // 添加分隔线
    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);
    
    // 添加"帮助"选项 - 打开使用指南网页
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_HELP, GetLocalizedString(L"使用指南", L"User Guide"));

    // 添加"检查更新"选项
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_CHECK_UPDATE, 
               GetLocalizedString(L"检查更新", L"Check for Updates"));

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
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_PORTUGUESE ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_PORTUGUESE, L"Português");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_JAPANESE ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_JAPANESE, L"日本語");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_KOREAN ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_KOREAN, L"한국어");

    AppendMenuW(hAboutMenu, MF_POPUP, (UINT_PTR)hLangMenu, GetLocalizedString(L"语言", L"Language"));

    // 添加重置选项到帮助菜单的最后
    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hAboutMenu, MF_STRING, 200,
                GetLocalizedString(L"重置", L"Reset"));

    // 将关于菜单添加到主菜单
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAboutMenu,
                GetLocalizedString(L"帮助", L"Help"));

    // 只保留退出选项
    AppendMenuW(hMenu, MF_STRING, 109,
                GetLocalizedString(L"退出", L"Exit"));
    
    // 显示菜单
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0); // 这将允许菜单在点击外部区域时自动关闭
    DestroyMenu(hMenu);
}

/**
 * @brief 显示托盘右键菜单
 * 
 * @param hwnd 窗口句柄
 * 
 * 创建并显示系统托盘右键菜单，根据当前应用状态动态调整菜单项。包含：
 * - 计时控制（暂停/继续、重新开始）
 * - 时间显示设置（24小时制、显示秒数）
 * - 番茄时钟设置
 * - 正计时和倒计时模式切换
 * - 快捷时间预设选项
 */
void ShowContextMenu(HWND hwnd) {
    // 在创建菜单前先读取配置文件中的超时动作设置
    ReadTimeoutActionFromConfig();
    
    // 设置鼠标光标为默认箭头，防止显示等待光标
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    
    HMENU hMenu = CreatePopupMenu();
    
    // 计时管理菜单 - 添加在最顶部
    HMENU hTimerManageMenu = CreatePopupMenu();
    
    // 设置是否应该启用子菜单项的条件
    // 当满足以下条件时计时器选项应该可用:
    // 1. 不处于显示当前时间模式
    // 2. 且正在进行倒计时或正计时
    // 3. 如果是倒计时，则还没有结束（倒计时已用时间小于总时间）
    BOOL timerRunning = (!CLOCK_SHOW_CURRENT_TIME && 
                         (CLOCK_COUNT_UP || 
                          (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME)));
    
    // 暂停/继续文本根据当前状态变化
    const wchar_t* pauseResumeText = CLOCK_IS_PAUSED ? 
                                    GetLocalizedString(L"继续", L"Resume") : 
                                    GetLocalizedString(L"暂停", L"Pause");
    
    // 子菜单项根据条件禁用，但保持父菜单项可选
    AppendMenuW(hTimerManageMenu, MF_STRING | (timerRunning ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_PAUSE_RESUME, pauseResumeText);
    
    // 重新开始选项应该在以下情况下可用：
    // 1. 不处于显示当前时间模式
    // 2. 且正在进行倒计时或正计时（不考虑倒计时是否结束）
    BOOL canRestart = (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || 
                      (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0)));
    
    AppendMenuW(hTimerManageMenu, MF_STRING | (canRestart ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_RESTART, 
               GetLocalizedString(L"重新开始", L"Start Over"));
    
    // 将计时管理菜单添加到主菜单 - 这里总是启用父菜单项
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimerManageMenu,
               GetLocalizedString(L"计时管理", L"Timer Control"));
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // 时间显示菜单
    HMENU hTimeMenu = CreatePopupMenu();
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_SHOW_CURRENT_TIME ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHOW_CURRENT_TIME,
               GetLocalizedString(L"显示当前时间", L"Show Current Time"));
    
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_USE_24HOUR ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_24HOUR_FORMAT,
               GetLocalizedString(L"24小时制", L"24-Hour Format"));
    
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_SHOW_SECONDS ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHOW_SECONDS,
               GetLocalizedString(L"显示秒数", L"Show Seconds"));
    
    AppendMenuW(hMenu, MF_POPUP,
               (UINT_PTR)hTimeMenu,
               GetLocalizedString(L"时间显示", L"Time Display"));

    // 番茄钟菜单之前，先读取最新的配置值
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    FILE *configFile = fopen(configPath, "r");
    POMODORO_TIMES_COUNT = 0;  // 初始化为0
    
    if (configFile) {
        char line[256];
        while (fgets(line, sizeof(line), configFile)) {
            if (strncmp(line, "POMODORO_TIME_OPTIONS=", 22) == 0) {
                char* options = line + 22;
                char* token;
                int index = 0;
                
                token = strtok(options, ",");
                while (token && index < MAX_POMODORO_TIMES) {
                    POMODORO_TIMES[index++] = atoi(token);
                    token = strtok(NULL, ",");
                }
                
                // 设置实际的时间选项数量
                POMODORO_TIMES_COUNT = index;
                
                // 确保至少有一个有效值
                if (index > 0) {
                    POMODORO_WORK_TIME = POMODORO_TIMES[0];
                    if (index > 1) POMODORO_SHORT_BREAK = POMODORO_TIMES[1];
                    if (index > 2) POMODORO_LONG_BREAK = POMODORO_TIMES[2];
                }
            }
            else if (strncmp(line, "POMODORO_LOOP_COUNT=", 20) == 0) {
                sscanf(line, "POMODORO_LOOP_COUNT=%d", &POMODORO_LOOP_COUNT);
                // 确保循环次数至少为1
                if (POMODORO_LOOP_COUNT < 1) POMODORO_LOOP_COUNT = 1;
            }
        }
        fclose(configFile);
    }

    // 番茄钟菜单
    HMENU hPomodoroMenu = CreatePopupMenu();
    
    // 添加timeBuffer的声明
    wchar_t timeBuffer[64]; // 用于存储格式化后的时间字符串
    
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_START,
                GetLocalizedString(L"开始", L"Start"));
    AppendMenuW(hPomodoroMenu, MF_SEPARATOR, 0, NULL);

    // 为每个番茄钟时间创建菜单项
    for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
        FormatPomodoroTime(POMODORO_TIMES[i], timeBuffer, sizeof(timeBuffer)/sizeof(wchar_t));
        
        // 同时兼容旧的ID和新的ID体系
        UINT menuId;
        if (i == 0) menuId = CLOCK_IDM_POMODORO_WORK;
        else if (i == 1) menuId = CLOCK_IDM_POMODORO_BREAK;
        else if (i == 2) menuId = CLOCK_IDM_POMODORO_LBREAK;
        else menuId = CLOCK_IDM_POMODORO_TIME_BASE + i;
        
        // 检查当前是否是活跃的番茄钟阶段
        BOOL isCurrentPhase = (current_pomodoro_phase != POMODORO_PHASE_IDLE &&
                              current_pomodoro_time_index == i &&
                              !CLOCK_SHOW_CURRENT_TIME &&
                              !CLOCK_COUNT_UP &&  // 添加检查是否不是正计时模式
                              CLOCK_TOTAL_TIME == POMODORO_TIMES[i]);
        
        // 如果是当前阶段，添加勾选状态
        AppendMenuW(hPomodoroMenu, MF_STRING | (isCurrentPhase ? MF_CHECKED : MF_UNCHECKED), 
                    menuId, timeBuffer);
    }

    // 添加循环次数选项
    wchar_t menuText[64];
    _snwprintf(menuText, sizeof(menuText)/sizeof(wchar_t),
              GetLocalizedString(L"循环次数: %d", L"Loop Count: %d"),
              POMODORO_LOOP_COUNT);
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_LOOP_COUNT, menuText);


    // 添加分隔线
    AppendMenuW(hPomodoroMenu, MF_SEPARATOR, 0, NULL);

        // 添加组合选项
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_COMBINATION,
              GetLocalizedString(L"组合", L"Combination"));
    
    // 将番茄钟菜单添加到主菜单
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPomodoroMenu,
                GetLocalizedString(L"番茄时钟", L"Pomodoro"));

    // 正计时菜单 - 改为直接点击启动
    AppendMenuW(hMenu, MF_STRING | (CLOCK_COUNT_UP ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_COUNT_UP_START,
               GetLocalizedString(L"正计时", L"Count Up"));

    // 将"设置倒计时"选项添加在正计时的下方
    AppendMenuW(hMenu, MF_STRING, 101, 
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
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0); // 这将允许菜单在点击外部区域时自动关闭
    DestroyMenu(hMenu);
}