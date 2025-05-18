/**
 * @file dialog_language.h
 * @brief 对话框多语言支持模块头文件
 * 
 * 本文件定义了用于对话框多语言支持的函数接口，使对话框文本可以根据应用程序语言设置进行本地化。
 */

#ifndef DIALOG_LANGUAGE_H
#define DIALOG_LANGUAGE_H

#include <windows.h>

/**
 * @brief 初始化对话框多语言支持
 * 
 * 在应用程序启动时调用此函数以初始化对话框多语言支持系统。
 * 
 * @return BOOL 初始化是否成功
 */
BOOL InitDialogLanguageSupport(void);

/**
 * @brief 对对话框应用多语言支持
 * 
 * 根据当前应用程序语言设置，更新对话框中的文本元素。
 * 此函数应在对话框 WM_INITDIALOG 消息处理期间调用。
 * 
 * @param hwndDlg 对话框句柄
 * @param dialogID 对话框资源ID
 * @return BOOL 操作是否成功
 */
BOOL ApplyDialogLanguage(HWND hwndDlg, int dialogID);

/**
 * @brief 获取对话框元素的本地化文本
 * 
 * 根据当前应用程序语言设置，获取对话框元素的本地化文本。
 * 
 * @param dialogID 对话框资源ID
 * @param controlID 控件资源ID
 * @return const wchar_t* 本地化文本，如果找不到则返回NULL
 */
const wchar_t* GetDialogLocalizedString(int dialogID, int controlID);

#endif /* DIALOG_LANGUAGE_H */ 