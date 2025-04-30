/**
 * @file tray_menu.h
 * @brief 系统托盘菜单功能接口
 * 
 * 本文件定义了系统托盘菜单相关的常量和函数接口，
 * 包括菜单项ID定义和菜单显示函数声明。
 */

#ifndef CLOCK_TRAY_MENU_H
#define CLOCK_TRAY_MENU_H

#include <windows.h>

/// @name 时间显示相关菜单项
/// @{
#define CLOCK_IDM_SHOW_CURRENT_TIME 150  ///< 显示当前时间选项
#define CLOCK_IDM_24HOUR_FORMAT 151      ///< 24小时制显示格式
#define CLOCK_IDM_SHOW_SECONDS 152       ///< 显示秒数选项
/// @}

/// @name 计时功能菜单项
/// @{
#define CLOCK_IDM_TIMER_MANAGEMENT 159   ///< 计时管理主菜单
#define CLOCK_IDM_TIMER_PAUSE_RESUME 158 ///< 暂停/继续计时
#define CLOCK_IDM_TIMER_RESTART 177      ///< 重新开始计时
#define CLOCK_IDM_COUNT_UP_START 171     ///< 开始/暂停正计时
#define CLOCK_IDM_COUNT_UP_RESET 172     ///< 重置正计时
#define CLOCK_IDM_COUNTDOWN_START_PAUSE 154  ///< 开始/暂停倒计时
#define CLOCK_IDM_COUNTDOWN_RESET 155    ///< 重置倒计时
/// @}

/// @name 编辑与设置菜单项
/// @{
#define CLOCK_IDC_EDIT_MODE 113          ///< 编辑模式选项
/// @}

/// @name 超时动作菜单项
/// @{
#define CLOCK_IDM_SHOW_MESSAGE 121       ///< 显示消息动作
#define CLOCK_IDM_LOCK_SCREEN 122        ///< 锁定屏幕动作
#define CLOCK_IDM_SHUTDOWN 123           ///< 关机动作
#define CLOCK_IDM_RESTART 124            ///< 重启动作
#define CLOCK_IDM_SLEEP 125              ///< 睡眠动作
#define CLOCK_IDM_BROWSE_FILE 131        ///< 浏览文件选项
#define CLOCK_IDM_RECENT_FILE_1 126      ///< 最近文件起始ID
#define CLOCK_IDM_TIMEOUT_SHOW_TIME 135  ///< 显示当前时间动作
#define CLOCK_IDM_TIMEOUT_COUNT_UP 136   ///< 正计时动作
/// @}

/// @name 时间选项管理菜单项
/// @{
#define CLOCK_IDC_MODIFY_TIME_OPTIONS 156  ///< 修改时间选项
/// @}

/// @name 启动设置菜单项
/// @{
#define CLOCK_IDC_SET_COUNTDOWN_TIME 173   ///< 启动时设为倒计时
#define CLOCK_IDC_START_COUNT_UP 175       ///< 启动时设为正计时
#define CLOCK_IDC_START_SHOW_TIME 176      ///< 启动时显示当前时间
#define CLOCK_IDC_START_NO_DISPLAY 174     ///< 启动时不显示
#define CLOCK_IDC_AUTO_START 160           ///< 开机自启动
/// @}

/// @name 通知设置菜单项
/// @{
#define CLOCK_IDM_NOTIFICATION_SETTINGS 193  ///< 通知设置主菜单
#define CLOCK_IDM_NOTIFICATION_CONTENT 191   ///< 通知内容设置
#define CLOCK_IDM_NOTIFICATION_DISPLAY 192   ///< 通知显示设置
/// @}

/// @name 番茄钟菜单项
/// @{
#define CLOCK_IDM_POMODORO_START 181   ///< 开始番茄钟
#define CLOCK_IDM_POMODORO_WORK 182    ///< 设置工作时间
#define CLOCK_IDM_POMODORO_BREAK 183   ///< 设置短休息时间
#define CLOCK_IDM_POMODORO_LBREAK 184  ///< 设置长休息时间
#define CLOCK_IDM_POMODORO_LOOP_COUNT 185 ///< 设置循环次数
#define CLOCK_IDM_POMODORO_RESET 186  ///< 重置番茄钟
/// @}

/// @name 颜色选择菜单项
/// @{
#define CLOCK_IDC_COLOR_VALUE 1301       ///< 颜色值编辑项
#define CLOCK_IDC_COLOR_PANEL 1302       ///< 颜色面板项
/// @}

/// @name 语言选择菜单项
/// @{
#define CLOCK_IDM_LANG_CHINESE 161       ///< 简体中文选项
#define CLOCK_IDM_LANG_CHINESE_TRAD 163  ///< 繁体中文选项
#define CLOCK_IDM_LANG_ENGLISH 162       ///< 英语选项
#define CLOCK_IDM_LANG_SPANISH 164       ///< 西班牙语选项
#define CLOCK_IDM_LANG_FRENCH 165        ///< 法语选项
#define CLOCK_IDM_LANG_GERMAN 166        ///< 德语选项
#define CLOCK_IDM_LANG_RUSSIAN 167       ///< 俄语选项
#define CLOCK_IDM_LANG_KOREAN 170        ///< 韩语选项
/// @}

/**
 * @brief 显示托盘右键菜单
 * @param hwnd 窗口句柄
 * 
 * 创建并显示系统托盘右键菜单，包含时间设置、显示模式切换和快捷时间选项。
 * 根据当前应用程序状态动态调整菜单项。
 */
void ShowContextMenu(HWND hwnd);

/**
 * @brief 显示颜色和设置菜单
 * @param hwnd 窗口句柄
 * 
 * 创建并显示应用程序的主设置菜单，包括编辑模式、超时动作、
 * 预设管理、字体选择、颜色设置和关于信息等选项。
 */
void ShowColorMenu(HWND hwnd);

#endif // CLOCK_TRAY_MENU_H