/**
 * @file notification.h
 * @brief 应用通知系统接口
 * 
 * 本文件定义了应用程序的通知系统接口，包括自定义样式的弹出通知和系统托盘通知。
 */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <windows.h>
#include "config.h"  // 引入通知类型枚举

// 全局变量：通知显示持续时间(毫秒)
extern int NOTIFICATION_TIMEOUT_MS;  // 新增：通知显示时间变量

/**
 * @brief 显示通知（根据配置的通知类型）
 * @param hwnd 父窗口句柄，用于获取应用实例和计算位置
 * @param message 要显示的通知消息文本(UTF-8编码)
 * 
 * 根据配置的通知类型显示不同风格的通知：
 * - NOTIFICATION_TYPE_CATIME: 使用Catime自定义通知窗口
 * - NOTIFICATION_TYPE_SYSTEM_MODAL: 使用系统模态对话框
 * - NOTIFICATION_TYPE_OS: 使用系统托盘通知
 */
void ShowNotification(HWND hwnd, const char* message);

/**
 * @brief 显示自定义样式的提示通知
 * @param hwnd 父窗口句柄，用于获取应用实例和计算位置
 * @param message 要显示的通知消息文本(UTF-8编码)
 * 
 * 在屏幕右下角显示一个带动画效果的自定义通知窗口
 */
void ShowToastNotification(HWND hwnd, const char* message);

/**
 * @brief 显示系统模态对话框通知
 * @param hwnd 父窗口句柄
 * @param message 要显示的通知消息文本(UTF-8编码)
 * 
 * 显示一个阻塞的系统消息框通知
 */
void ShowModalNotification(HWND hwnd, const char* message);

/**
 * @brief 关闭所有当前显示的Catime通知窗口
 * 
 * 查找并关闭所有由Catime创建的通知窗口，无视它们当前的显示时间设置，
 * 直接开始淡出动画。通常在切换计时器模式时调用，确保通知不会继续显示。
 */
void CloseAllNotifications(void);

// 系统托盘通知已在tray.h中定义：void ShowTrayNotification(HWND hwnd, const char* message);

#endif // NOTIFICATION_H