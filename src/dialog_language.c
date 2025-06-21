 /**
 * @file dialog_language.c
 * @brief 对话框多语言支持模块实现文件
 * 
 * 本文件实现了对话框多语言支持功能，管理对话框文本的本地化显示。
 * 新版本使用Windows API动态遍历对话框控件，而不是手动定义控件映射。
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include "../include/dialog_language.h"
#include "../include/language.h"
#include "../resource/resource.h"

// 定义对话框标题的字典项
typedef struct {
    int dialogID;      // 对话框资源ID
    wchar_t* titleKey; // 对话框标题的键名
} DialogTitleEntry;

// 特殊处理的控件定义
typedef struct {
    int dialogID;      // 对话框资源ID
    int controlID;     // 控件资源ID
    wchar_t* textKey;  // ini文件中的键名
    wchar_t* fallbackText; // 硬编码的备用文本
} SpecialControlEntry;

// 结构化列举回调函数的数据
typedef struct {
    HWND hwndDlg;      // 对话框句柄
    int dialogID;      // 对话框资源ID
} EnumChildWindowsData;

// 对话框标题映射表
static DialogTitleEntry g_dialogTitles[] = {
    {IDD_ABOUT_DIALOG, L"About"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, L"Notification Settings"},
    {CLOCK_IDD_POMODORO_LOOP_DIALOG, L"Set Pomodoro Loop Count"},
    {CLOCK_IDD_POMODORO_COMBO_DIALOG, L"Set Pomodoro Time Combination"},
    {CLOCK_IDD_POMODORO_TIME_DIALOG, L"Set Pomodoro Time"},
    {CLOCK_IDD_SHORTCUT_DIALOG, L"Countdown Presets"},
    {CLOCK_IDD_WEBSITE_DIALOG, L"Open Website"},
    {CLOCK_IDD_DIALOG1, L"Set Countdown"},
    {IDD_NO_UPDATE_DIALOG, L"Update Check"},
    {IDD_UPDATE_DIALOG, L"Update Available"}
};

// 特殊控件映射表（需要特殊处理的控件）
static SpecialControlEntry g_specialControls[] = {
    // 关于对话框
    {IDD_ABOUT_DIALOG, IDC_ABOUT_TITLE, L"关于", L"About"},
    {IDD_ABOUT_DIALOG, IDC_VERSION_TEXT, L"Version: %hs", L"Version: %hs"},
    {IDD_ABOUT_DIALOG, IDC_BUILD_DATE, L"构建日期:", L"Build Date:"},
    {IDD_ABOUT_DIALOG, IDC_COPYRIGHT, L"COPYRIGHT_TEXT", L"COPYRIGHT_TEXT"},
    {IDD_ABOUT_DIALOG, IDC_CREDITS, L"鸣谢", L"Credits"},
    
    // 无需更新对话框
    {IDD_NO_UPDATE_DIALOG, IDC_NO_UPDATE_TEXT, L"NoUpdateRequired", L"You are already using the latest version!"},
    
    // 更新提示对话框
    {IDD_UPDATE_DIALOG, IDC_UPDATE_TEXT, L"CurrentVersion: %s\nNewVersion: %s", L"Current Version: %s\nNew Version: %s"},
    {IDD_UPDATE_DIALOG, IDC_UPDATE_EXIT_TEXT, L"The application will exit now", L"The application will exit now"},
    
    // 通知设置对话框的控件 - 添加这部分来解决问题
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_CONTENT_GROUP, L"Notification Content", L"Notification Content"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_LABEL1, L"Countdown timeout message:", L"Countdown timeout message:"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_LABEL2, L"Pomodoro timeout message:", L"Pomodoro timeout message:"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_LABEL3, L"Pomodoro cycle complete message:", L"Pomodoro cycle complete message:"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_DISPLAY_GROUP, L"Notification Display", L"Notification Display"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TIME_LABEL, L"Notification display time:", L"Notification display time:"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_OPACITY_LABEL, L"Maximum notification opacity (1-100%):", L"Maximum notification opacity (1-100%):"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_DISABLE_NOTIFICATION_CHECK, L"Disable notifications", L"Disable notifications"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_METHOD_GROUP, L"Notification Method", L"Notification Method"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TYPE_CATIME, L"Catime notification window", L"Catime notification window"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TYPE_OS, L"System notification", L"System notification"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL, L"System modal window", L"System modal window"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_SOUND_LABEL, L"Sound (supports .mp3/.wav/.flac):", L"Sound (supports .mp3/.wav/.flac):"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_VOLUME_LABEL, L"Volume (0-100%):", L"Volume (0-100%):"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_VOLUME_TEXT, L"100%", L"100%"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_OPACITY_TEXT, L"100%", L"100%"},
    
    // 番茄钟时间设置对话框特殊处理的静态文本 - 使用实际的INI键名
    {CLOCK_IDD_POMODORO_TIME_DIALOG, CLOCK_IDC_STATIC, 
     L"25=25 minutes\\n25h=25 hours\\n25s=25 seconds\\n25 30=25 minutes 30 seconds\\n25 30m=25 hours 30 minutes\\n1 30 20=1 hour 30 minutes 20 seconds", 
     L"25=25 minutes\n25h=25 hours\n25s=25 seconds\n25 30=25 minutes 30 seconds\n25 30m=25 hours 30 minutes\n1 30 20=1 hour 30 minutes 20 seconds"},
    
    // 番茄钟组合对话框特殊处理的静态文本 - 使用实际的INI键名
    {CLOCK_IDD_POMODORO_COMBO_DIALOG, CLOCK_IDC_STATIC, 
     L"Enter pomodoro time sequence, separated by spaces:\\n\\n25m = 25 minutes\\n30s = 30 seconds\\n1h30m = 1 hour 30 minutes\\nExample: 25m 5m 25m 10m - work 25min, short break 5min, work 25min, long break 10min", 
     L"Enter pomodoro time sequence, separated by spaces:\n\n25m = 25 minutes\n30s = 30 seconds\n1h30m = 1 hour 30 minutes\nExample: 25m 5m 25m 10m - work 25min, short break 5min, work 25min, long break 10min"},
    
    // 网站URL输入对话框说明文本
    {CLOCK_IDD_WEBSITE_DIALOG, CLOCK_IDC_STATIC, 
     L"Enter the website URL to open when the countdown ends:\\nExample: https://github.com/vladelaina/Catime", 
     L"Enter the website URL to open when the countdown ends:\nExample: https://github.com/vladelaina/Catime"},
    
    // 快捷时间选项设置对话框说明文本
    {CLOCK_IDD_SHORTCUT_DIALOG, CLOCK_IDC_STATIC, 
     L"CountdownPresetDialogStaticText", 
     L"Enter numbers (minutes), separated by spaces\n\n25 10 5\n\nThis will create options for 25 minutes, 10 minutes, and 5 minutes"},
    
    // 主倒计时对话框 (CLOCK_IDD_DIALOG1) 的静态帮助文本
    {CLOCK_IDD_DIALOG1, CLOCK_IDC_STATIC,
     L"CountdownDialogStaticText",
     L"25=25 minutes\n25h=25 hours\n25s=25 seconds\n25 30=25 minutes 30 seconds\n25 30m=25 hours 30 minutes\n1 30 20=1 hour 30 minutes 20 seconds\n17 20t=Countdown to 17:20\n9 9 9t=Countdown to 09:09:09"}
};

// 特殊按钮文本
static SpecialControlEntry g_specialButtons[] = {
    // 更新对话框按钮
    {IDD_UPDATE_DIALOG, IDYES, L"Yes", L"Yes"},
    {IDD_UPDATE_DIALOG, IDNO, L"No", L"No"},
    {IDD_UPDATE_DIALOG, IDOK, L"OK", L"OK"},
    
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_TEST_SOUND_BUTTON, L"Test", L"Test"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_OPEN_SOUND_DIR_BUTTON, L"Audio folder", L"Audio folder"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDOK, L"OK", L"OK"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDCANCEL, L"Cancel", L"Cancel"},
    
    {CLOCK_IDD_POMODORO_LOOP_DIALOG, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"},
    {CLOCK_IDD_POMODORO_COMBO_DIALOG, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"},
    {CLOCK_IDD_POMODORO_TIME_DIALOG, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"},
    {CLOCK_IDD_WEBSITE_DIALOG, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"},
    {CLOCK_IDD_SHORTCUT_DIALOG, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"},
    {CLOCK_IDD_DIALOG1, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"}
};

// 对话框映射表的大小
#define DIALOG_TITLES_COUNT (sizeof(g_dialogTitles) / sizeof(g_dialogTitles[0]))
#define SPECIAL_CONTROLS_COUNT (sizeof(g_specialControls) / sizeof(g_specialControls[0]))
#define SPECIAL_BUTTONS_COUNT (sizeof(g_specialButtons) / sizeof(g_specialButtons[0]))

/**
 * @brief 查找特殊控件的对应本地化文本
 * @param dialogID 对话框ID
 * @param controlID 控件ID
 * @return const wchar_t* 找到的本地化文本，如果未找到则返回NULL
 */
static const wchar_t* FindSpecialControlText(int dialogID, int controlID) {
    for (int i = 0; i < SPECIAL_CONTROLS_COUNT; i++) {
        if (g_specialControls[i].dialogID == dialogID && 
            g_specialControls[i].controlID == controlID) {
            // 将textKey作为查找本地化字符串的键名
            const wchar_t* localizedText = GetLocalizedString(NULL, g_specialControls[i].textKey);
            if (localizedText) {
                return localizedText;
            } else {
                // 如果找不到本地化文本，返回fallbackText
                return g_specialControls[i].fallbackText;
            }
        }
    }
    return NULL;
}

/**
 * @brief 查找特殊按钮的对应本地化文本
 * @param dialogID 对话框ID
 * @param controlID 控件ID
 * @return const wchar_t* 找到的本地化文本，如果未找到则返回NULL
 */
static const wchar_t* FindSpecialButtonText(int dialogID, int controlID) {
    for (int i = 0; i < SPECIAL_BUTTONS_COUNT; i++) {
        if (g_specialButtons[i].dialogID == dialogID && 
            g_specialButtons[i].controlID == controlID) {
            return GetLocalizedString(NULL, g_specialButtons[i].textKey);
        }
    }
    return NULL;
}

/**
 * @brief 获取对话框标题的本地化文本
 * @param dialogID 对话框ID
 * @return const wchar_t* 找到的本地化文本，如果未找到则返回NULL
 */
static const wchar_t* GetDialogTitleText(int dialogID) {
    for (int i = 0; i < DIALOG_TITLES_COUNT; i++) {
        if (g_dialogTitles[i].dialogID == dialogID) {
            return GetLocalizedString(NULL, g_dialogTitles[i].titleKey);
        }
    }
    return NULL;
}

/**
 * @brief 获取控件的原始文本，用于翻译查找
 * @param hwndCtl 控件句柄
 * @param buffer 存储文本的缓冲区
 * @param bufferSize 缓冲区大小（字符数，不是字节数）
 * @return BOOL 是否成功获取文本
 */
static BOOL GetControlOriginalText(HWND hwndCtl, wchar_t* buffer, int bufferSize) {
    // 获取控件类名
    wchar_t className[256];
    GetClassNameW(hwndCtl, className, 256);
    
    // 按钮类控件需要获取其文本
    if (wcscmp(className, L"Button") == 0 || 
        wcscmp(className, L"Static") == 0 ||
        wcscmp(className, L"ComboBox") == 0 ||
        wcscmp(className, L"Edit") == 0) {
        return GetWindowTextW(hwndCtl, buffer, bufferSize) > 0;
    }
    
    return FALSE;
}

/**
 * @brief 处理特殊控件的文本设置，如换行符等
 * @param hwndCtl 控件句柄
 * @param localizedText 本地化后的文本
 * @param dialogID 对话框ID
 * @param controlID 控件ID
 * @return BOOL 是否成功处理
 */
static BOOL ProcessSpecialControlText(HWND hwndCtl, const wchar_t* localizedText, int dialogID, int controlID) {
    // 特殊处理番茄钟相关对话框和网站URL对话框的静态文本换行
    if ((dialogID == CLOCK_IDD_POMODORO_COMBO_DIALOG || 
         dialogID == CLOCK_IDD_POMODORO_TIME_DIALOG ||
         dialogID == CLOCK_IDD_WEBSITE_DIALOG ||
         dialogID == CLOCK_IDD_SHORTCUT_DIALOG ||
         dialogID == CLOCK_IDD_DIALOG1) && 
        controlID == CLOCK_IDC_STATIC) {
        wchar_t processedText[1024]; // 假设文本不会超过1024个宽字符
        const wchar_t* src = localizedText;
        wchar_t* dst = processedText;
        
        while (*src) {
            // 处理INI文件中的转义换行符 \n
            if (src[0] == L'\\' && src[1] == L'n') {
                *dst++ = L'\n'; // 转换为实际的换行符
                src += 2;
            } 
            // 处理文本中可能存在的字面换行符
            else if (src[0] == L'\n') {
                *dst++ = L'\n';
                src++;
            }
            else {
                *dst++ = *src++;
            }
        }
        *dst = L'\0';
        
        SetWindowTextW(hwndCtl, processedText);
        return TRUE;
    }
    
    // 特殊处理版本信息文本
    if (controlID == IDC_VERSION_TEXT && dialogID == IDD_ABOUT_DIALOG) {
        wchar_t versionText[256];
        // 从语言文件获取本地化的版本字符串格式
        const wchar_t* localizedVersionFormat = GetLocalizedString(NULL, L"Version: %hs");
        if (localizedVersionFormat) {
            StringCbPrintfW(versionText, sizeof(versionText), localizedVersionFormat, CATIME_VERSION);
        } else {
            // 如果找不到本地化版本，使用传入的格式
            StringCbPrintfW(versionText, sizeof(versionText), localizedText, CATIME_VERSION);
        }
        SetWindowTextW(hwndCtl, versionText);
        return TRUE;
    }
    
    return FALSE;
}

/**
 * @brief 对话框子窗口遍历回调函数
 * @param hwndCtl 子窗口句柄
 * @param lParam 回调参数，包含父对话框句柄和对话框ID
 * @return BOOL 是否继续遍历
 */
static BOOL CALLBACK EnumChildProc(HWND hwndCtl, LPARAM lParam) {
    EnumChildWindowsData* data = (EnumChildWindowsData*)lParam;
    HWND hwndDlg = data->hwndDlg;
    int dialogID = data->dialogID;
    
    // 获取控件ID
    int controlID = GetDlgCtrlID(hwndCtl);
    if (controlID == 0) {
        return TRUE; // 继续遍历
    }

    // 根据对话框ID和控件ID特殊处理一些控件
    const wchar_t* specialText = FindSpecialControlText(dialogID, controlID);
    if (specialText) {
        if (ProcessSpecialControlText(hwndCtl, specialText, dialogID, controlID)) {
            return TRUE; // 已处理特殊控件
        }
        SetWindowTextW(hwndCtl, specialText);
        return TRUE;
    }
    
    // 检查按钮特殊文本
    const wchar_t* buttonText = FindSpecialButtonText(dialogID, controlID);
    if (buttonText) {
        SetWindowTextW(hwndCtl, buttonText);
        return TRUE;
    }

    // 获取控件当前文本
    wchar_t originalText[512] = {0};
    if (GetControlOriginalText(hwndCtl, originalText, 512) && originalText[0] != L'\0') {
        // 查找本地化文本
        const wchar_t* localizedText = GetLocalizedString(NULL, originalText);
        if (localizedText && wcscmp(localizedText, originalText) != 0) {
            // 设置本地化文本
            SetWindowTextW(hwndCtl, localizedText);
        }
    }
    
    return TRUE; // 继续遍历
}

/**
 * @brief 初始化对话框多语言支持
 */
BOOL InitDialogLanguageSupport(void) {
    // 这里不需要额外初始化操作
    return TRUE;
}

/**
 * @brief 对对话框应用多语言支持
 */
BOOL ApplyDialogLanguage(HWND hwndDlg, int dialogID) {
    if (!hwndDlg) return FALSE;
    
    // 设置对话框标题
    const wchar_t* titleText = GetDialogTitleText(dialogID);
    if (titleText) {
        SetWindowTextW(hwndDlg, titleText);
    }
    
    // 设置遍历数据
    EnumChildWindowsData data = {
        .hwndDlg = hwndDlg,
        .dialogID = dialogID
    };
    
    // 遍历所有子窗口并应用本地化文本
    EnumChildWindows(hwndDlg, EnumChildProc, (LPARAM)&data);
    
    return TRUE;
}

/**
 * @brief 获取对话框元素的本地化文本
 */
const wchar_t* GetDialogLocalizedString(int dialogID, int controlID) {
    // 检查是否是特殊控件
    const wchar_t* specialText = FindSpecialControlText(dialogID, controlID);
    if (specialText) {
        return specialText;
    }
    
    // 检查是否是特殊按钮
    const wchar_t* buttonText = FindSpecialButtonText(dialogID, controlID);
    if (buttonText) {
        return buttonText;
    }
    
    // 如果是对话框标题
    if (controlID == -1) {
        return GetDialogTitleText(dialogID);
    }
    
    return NULL;
} 