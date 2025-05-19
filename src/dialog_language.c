/**
 * @file dialog_language.c
 * @brief 对话框多语言支持模块实现文件
 * 
 * 本文件实现了对话框多语言支持功能，管理对话框文本的本地化显示。
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include "../include/dialog_language.h"
#include "../include/language.h"
#include "../resource/resource.h"

/**
 * @brief 对话框元素本地化字符串结构
 */
typedef struct {
    int dialogID;      // 对话框资源ID
    int controlID;     // 控件资源ID
    wchar_t* textKey;  // ini文件中的键名
} DialogLocalizedElement;

// 关于对话框元素本地化映射表
static DialogLocalizedElement g_aboutDialogElements[] = {
    // 对话框标题不包括在元素数组中，单独处理
    {IDD_ABOUT_DIALOG, IDC_VERSION_TEXT, L"Version: %hs"},
    {IDD_ABOUT_DIALOG, IDC_BUILD_DATE, L"Build Date:"},
    {IDD_ABOUT_DIALOG, IDC_COPYRIGHT, L"Copyright © 2023-2024 VladeLaina"},
    {IDD_ABOUT_DIALOG, IDC_CREDIT_LINK, L"Special thanks to Neko House Lili Official for the icon"},
    {IDD_ABOUT_DIALOG, IDC_CREDITS, L"Credits"},
    {IDD_ABOUT_DIALOG, IDC_BILIBILI_LINK, L"BiliBili"},
    {IDD_ABOUT_DIALOG, IDC_GITHUB_LINK, L"GitHub"},
    {IDD_ABOUT_DIALOG, IDC_COPYRIGHT_LINK, L"Copyright Notice"},
    {IDD_ABOUT_DIALOG, IDC_SUPPORT, L"Support"}
};

// 通知设置对话框元素本地化映射表
static DialogLocalizedElement g_notificationDialogElements[] = {
    // 对话框标题
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, -1, L"通知设置"},
    // 通知内容组
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_CONTENT_GROUP, L"通知内容"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_LABEL1, L"倒计时超时提示:"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_LABEL2, L"番茄钟超时提示:"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_LABEL3, L"番茄钟循环完成提示:"},
    // 通知显示组
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_DISPLAY_GROUP, L"通知显示"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TIME_LABEL, L"通知显示时间:"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_OPACITY_LABEL, L"通知最大透明度(1-100%):"},
    // 通知方式组
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_METHOD_GROUP, L"通知方式"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TYPE_CATIME, L"Catime通知窗口"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TYPE_OS, L"操作系统通知"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL, L"系统模态窗口"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_SOUND_LABEL, L"提示音(支持.mp3/.wav/.flac):"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_TEST_SOUND_BUTTON, L"测试"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_OPEN_SOUND_DIR_BUTTON, L"音频目录"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_VOLUME_LABEL, L"音量(0-100%):"},
    // 底部按钮
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDCANCEL, L"取消"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDOK, L"确定"}
};

// 番茄钟循环次数设置对话框元素本地化映射表
static DialogLocalizedElement g_pomodoroLoopDialogElements[] = {
    // 对话框标题
    {CLOCK_IDD_POMODORO_LOOP_DIALOG, -1, L"设置番茄钟循环次数"},
    // 提示文本
    {CLOCK_IDD_POMODORO_LOOP_DIALOG, CLOCK_IDC_STATIC, L"请输入循环次数（1-10）："},
    // 确定按钮
    {CLOCK_IDD_POMODORO_LOOP_DIALOG, CLOCK_IDC_BUTTON_OK, L"确定"}
};

// 番茄钟时间组合设置对话框元素本地化映射表
static DialogLocalizedElement g_pomodoroComboDialogElements[] = {
    // 对话框标题
    {CLOCK_IDD_POMODORO_COMBO_DIALOG, -1, L"设置番茄钟时间组合"},
    // 提示文本
    {CLOCK_IDD_POMODORO_COMBO_DIALOG, CLOCK_IDC_STATIC, L"输入番茄钟时间组合，用空格分隔：\\n\\n25m = 25分钟\\n30s = 30秒\\n1h30m = 1小时30分钟\\n例如: 25m 5m 25m 10m - 工作25分钟，短休息5分钟，工作25分钟，长休息10分钟"},
    // 确定按钮
    {CLOCK_IDD_POMODORO_COMBO_DIALOG, CLOCK_IDC_BUTTON_OK, L"确定"}
};

// 本地化元素计数
#define ABOUT_DIALOG_ELEMENTS_COUNT (sizeof(g_aboutDialogElements) / sizeof(g_aboutDialogElements[0]))
#define NOTIFICATION_DIALOG_ELEMENTS_COUNT (sizeof(g_notificationDialogElements) / sizeof(g_notificationDialogElements[0]))
#define POMODORO_LOOP_DIALOG_ELEMENTS_COUNT (sizeof(g_pomodoroLoopDialogElements) / sizeof(g_pomodoroLoopDialogElements[0]))
#define POMODORO_COMBO_DIALOG_ELEMENTS_COUNT (sizeof(g_pomodoroComboDialogElements) / sizeof(g_pomodoroComboDialogElements[0]))

/**
 * @brief 初始化对话框多语言支持
 */
BOOL InitDialogLanguageSupport(void) {
    // 由于我们使用的是现有的语言系统，所以这里不需要额外初始化
    // 如果将来需要额外的初始化操作，可以在这里添加
    return TRUE;
}

/**
 * @brief 对对话框应用多语言支持
 */
BOOL ApplyDialogLanguage(HWND hwndDlg, int dialogID) {
    if (!hwndDlg) return FALSE;
    
    // 其他对话框的常规处理
    int i;
    const DialogLocalizedElement* elements = NULL;
    size_t elementsCount = 0;
    
    // 根据对话框ID选择相应的本地化元素数组
    if (dialogID == IDD_ABOUT_DIALOG) {
        elements = g_aboutDialogElements;
        elementsCount = ABOUT_DIALOG_ELEMENTS_COUNT;
    } else if (dialogID == CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG) {
        elements = g_notificationDialogElements;
        elementsCount = NOTIFICATION_DIALOG_ELEMENTS_COUNT;
    } else if (dialogID == CLOCK_IDD_POMODORO_LOOP_DIALOG) {
        elements = g_pomodoroLoopDialogElements;
        elementsCount = POMODORO_LOOP_DIALOG_ELEMENTS_COUNT;
    } else if (dialogID == CLOCK_IDD_POMODORO_COMBO_DIALOG) {
        elements = g_pomodoroComboDialogElements;
        elementsCount = POMODORO_COMBO_DIALOG_ELEMENTS_COUNT;
    } else {
        // 不支持的对话框ID
        return FALSE;
    }
    
    // 遍历所有元素并应用本地化文本
    for (i = 0; i < elementsCount; i++) {
        // 跳过不匹配当前对话框ID的元素
        if (elements[i].dialogID != dialogID) continue;
        
        // 对话框标题特殊处理
        if (elements[i].controlID == -1) {
            // 设置对话框标题
            const wchar_t* localizedText = GetLocalizedString(elements[i].textKey, elements[i].textKey);
            if (localizedText) {
                SetWindowTextW(hwndDlg, localizedText);
            }
            continue;
        }
        
        // 获取控件句柄
        HWND hwndControl = GetDlgItem(hwndDlg, elements[i].controlID);
        if (!hwndControl) continue;
        
        // 获取本地化文本
        const wchar_t* localizedText = GetLocalizedString(elements[i].textKey, elements[i].textKey);
        if (!localizedText) continue;
        
        // 特殊处理番茄钟组合对话框的静态文本换行
        if (dialogID == CLOCK_IDD_POMODORO_COMBO_DIALOG && elements[i].controlID == CLOCK_IDC_STATIC) {
            wchar_t processedText[1024]; // 假设文本不会超过1024个宽字符
            const wchar_t* src = localizedText;
            wchar_t* dst = processedText;
            while (*src) {
                if (src[0] == L'\\' && src[1] == L'n') {
                    *dst++ = L'\n';
                    src += 2;
                } else {
                    *dst++ = *src++;
                }
            }
            *dst = L'\0';
            SetWindowTextW(hwndControl, processedText);
        } else if (elements[i].controlID == IDC_VERSION_TEXT && dialogID == IDD_ABOUT_DIALOG) {
            wchar_t versionText[256];
            StringCbPrintfW(versionText, sizeof(versionText), localizedText, CATIME_VERSION);
            SetWindowTextW(hwndControl, versionText);
        } else {
            // 设置控件文本
            SetWindowTextW(hwndControl, localizedText);
        }
    }
    
    return TRUE;
}

/**
 * @brief 获取对话框元素的本地化文本
 */
const wchar_t* GetDialogLocalizedString(int dialogID, int controlID) {
    const DialogLocalizedElement* elements = NULL;
    size_t elementsCount = 0;
    
    // 根据对话框ID选择相应的本地化元素数组
    if (dialogID == IDD_ABOUT_DIALOG) {
        elements = g_aboutDialogElements;
        elementsCount = ABOUT_DIALOG_ELEMENTS_COUNT;
    } else if (dialogID == CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG) {
        elements = g_notificationDialogElements;
        elementsCount = NOTIFICATION_DIALOG_ELEMENTS_COUNT;
    } else if (dialogID == CLOCK_IDD_POMODORO_LOOP_DIALOG) {
        elements = g_pomodoroLoopDialogElements;
        elementsCount = POMODORO_LOOP_DIALOG_ELEMENTS_COUNT;
    } else if (dialogID == CLOCK_IDD_POMODORO_COMBO_DIALOG) {
        elements = g_pomodoroComboDialogElements;
        elementsCount = POMODORO_COMBO_DIALOG_ELEMENTS_COUNT;
    } else {
        // 不支持的对话框ID
        return NULL;
    }
    
    // 查找匹配的元素
    for (int i = 0; i < elementsCount; i++) {
        if (elements[i].dialogID == dialogID && elements[i].controlID == controlID) {
            // 返回本地化文本
            return GetLocalizedString(elements[i].textKey, elements[i].textKey);
        }
    }
    
    return NULL;
} 