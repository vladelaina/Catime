/**
 * @file tray_events.h
 * @brief 系统托盘事件处理接口
 * 
 * 本文件定义了应用程序系统托盘事件处理相关的函数接口，
 * 包括托盘图标的点击事件处理等功能。
 */

#ifndef CLOCK_TRAY_EVENTS_H
#define CLOCK_TRAY_EVENTS_H

#include <windows.h>

/**
 * @brief 处理系统托盘消息
 * @param hwnd 窗口句柄
 * @param uID 托盘图标ID
 * @param uMouseMsg 鼠标消息类型
 * 
 * 处理系统托盘的鼠标事件，包括：
 * - 左键点击：显示上下文菜单
 * - 右键点击：显示颜色菜单
 */
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg);

/**
 * @brief 暂停或继续计时器
 * @param hwnd 窗口句柄
 * 
 * 根据当前状态暂停或继续计时器，并更新相关状态变量
 */
void PauseResumeTimer(HWND hwnd);

/**
 * @brief 重新开始计时器
 * @param hwnd 窗口句柄
 * 
 * 重置计时器到初始状态并继续运行
 */
void RestartTimer(HWND hwnd);

/**
 * @brief 设置启动模式
 * @param hwnd 窗口句柄
 * @param mode 启动模式字符串
 * 
 * 根据给定的模式设置启动模式，并更新相关状态变量
 */
void SetStartupMode(HWND hwnd, const char* mode);

/**
 * @brief 打开用户指南
 * 
 * 打开用户指南，提供应用程序的使用说明和帮助
 */
void OpenUserGuide(void);

/**
 * @brief 打开支持页面
 * 
 * 打开支持页面，提供支持开发者的渠道
 */
void OpenSupportPage(void);

/**
 * @brief 打开反馈页面
 * 
 * 根据当前语言设置打开不同的反馈渠道：
 * - 简体中文：打开bilibili私信页面
 * - 其他语言：打开GitHub Issues页面
 */
void OpenFeedbackPage(void);

#endif // CLOCK_TRAY_EVENTS_H