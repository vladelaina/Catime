/**
 * @file dialog_procedure.h
 * @brief 对话框消息处理过程接口
 * 
 * 本文件定义了应用程序的对话框消息处理回调函数接口，
 * 处理对话框的所有消息事件包括初始化、颜色管理、按钮点击和键盘事件。
 */

#ifndef DIALOG_PROCEDURE_H
#define DIALOG_PROCEDURE_H

#include <windows.h>

/**
 * @brief 输入对话框过程
 * @param hwndDlg 对话框句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 * 
 * 处理倒计时输入对话框的：
 * 1. 控件初始化与焦点设置
 * 2. 背景/控件颜色管理
 * 3. 确定按钮点击处理
 * 4. 回车键响应
 * 5. 资源清理
 */
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief 显示关于对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示包含程序版本、作者和第三方库信息的关于对话框。
 * 包含以下链接按钮：
 * - 鸣谢 (IDC_CREDITS)
 * - 反馈 (IDC_FEEDBACK)
 * - GitHub (IDC_GITHUB)
 * - 开源协议 (IDC_LICENSE)
 * - 支持 (IDC_SUPPORT)
 */
void ShowAboutDialog(HWND hwndParent);

INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief 显示鸣谢对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示包含项目贡献者名单的鸣谢对话框。
 */
void ShowCreditsDialog(HWND hwndParent);

#endif // DIALOG_PROCEDURE_H