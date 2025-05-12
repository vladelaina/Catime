/**
 * @file hotkey.h
 * @brief 热键管理接口
 * 
 * 本文件定义了应用程序的热键管理接口，
 * 处理全局热键的设置、对话框交互和配置保存。
 */

#ifndef HOTKEY_H
#define HOTKEY_H

#include <windows.h>

/**
 * @brief 显示热键设置对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示热键设置对话框，用于设置全局热键。
 */
void ShowHotkeySettingsDialog(HWND hwndParent);

/**
 * @brief 热键设置对话框消息处理过程
 * @param hwndDlg 对话框句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 * 
 * 处理热键设置对话框的所有消息事件，包括初始化、背景颜色和按钮点击。
 */
INT_PTR CALLBACK HotkeySettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief 热键控件子类化处理函数
 * @param hwnd 热键控件窗口句柄
 * @param uMsg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @param uIdSubclass 子类ID
 * @param dwRefData 引用数据
 * @return LRESULT 消息处理结果
 * 
 * 处理热键控件的消息，特别是拦截Alt键和Alt+Shift组合键
 * 防止Windows系统发出提示音
 */
LRESULT CALLBACK HotkeyControlSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, 
                                         LPARAM lParam, UINT_PTR uIdSubclass, 
                                         DWORD_PTR dwRefData);

#endif // HOTKEY_H 