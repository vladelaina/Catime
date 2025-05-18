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

// 本地化元素计数
#define ABOUT_DIALOG_ELEMENTS_COUNT (sizeof(g_aboutDialogElements) / sizeof(g_aboutDialogElements[0]))

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
    
    int i;
    const DialogLocalizedElement* elements = NULL;
    size_t elementsCount = 0;
    
    // 根据对话框ID选择相应的本地化元素数组
    if (dialogID == IDD_ABOUT_DIALOG) {
        elements = g_aboutDialogElements;
        elementsCount = ABOUT_DIALOG_ELEMENTS_COUNT;
    } else {
        // 不支持的对话框ID
        return FALSE;
    }
    
    // 遍历所有元素并应用本地化文本
    for (i = 0; i < elementsCount; i++) {
        // 跳过不匹配当前对话框ID的元素
        if (elements[i].dialogID != dialogID) continue;
        
        // 获取控件句柄
        HWND hwndControl = GetDlgItem(hwndDlg, elements[i].controlID);
        if (!hwndControl) continue;
        
        // 获取本地化文本
        const wchar_t* localizedText = GetLocalizedString(NULL, elements[i].textKey);
        if (!localizedText) continue;
        
        // 特殊处理版本信息（包含格式化参数）
        if (elements[i].controlID == IDC_VERSION_TEXT) {
            wchar_t versionText[256];
            swprintf(versionText, 256, localizedText, CATIME_VERSION);
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
    } else {
        // 不支持的对话框ID
        return NULL;
    }
    
    // 查找匹配的元素
    for (int i = 0; i < elementsCount; i++) {
        if (elements[i].dialogID == dialogID && elements[i].controlID == controlID) {
            // 返回本地化文本
            return GetLocalizedString(NULL, elements[i].textKey);
        }
    }
    
    return NULL;
} 