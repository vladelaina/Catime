/**
 * @file startup.h
 * @brief 开机自启动功能接口
 * 
 * 本文件定义了应用程序开机自启动相关的函数接口，
 * 包括检查是否已启用自启动、创建和删除自启动快捷方式的功能。
 */

#ifndef STARTUP_H
#define STARTUP_H

#include <windows.h>
#include <shlobj.h>

/**
 * @brief 检查应用程序是否已设置为开机自启动
 * @return BOOL 如果已启用开机自启动则返回TRUE，否则返回FALSE
 * 
 * 通过检查启动文件夹中是否存在应用程序的快捷方式来判断是否已启用自启动。
 */
BOOL IsAutoStartEnabled(void);

/**
 * @brief 创建开机自启动快捷方式
 * @return BOOL 如果创建成功则返回TRUE，否则返回FALSE
 * 
 * 在系统启动文件夹中创建应用程序的快捷方式，使其能够在系统启动时自动运行。
 */
BOOL CreateShortcut(void);

/**
 * @brief 删除开机自启动快捷方式
 * @return BOOL 如果删除成功则返回TRUE，否则返回FALSE
 * 
 * 从系统启动文件夹中删除应用程序的快捷方式，禁用其开机自启动功能。
 */
BOOL RemoveShortcut(void);

/**
 * @brief 更新开机自启动快捷方式
 * 
 * 检查是否已启用自启动，如果已启用，则删除旧的快捷方式并创建新的，
 * 确保即使应用程序位置发生变化，自启动功能也能正常工作。
 * 
 * @return BOOL 如果更新成功则返回TRUE，否则返回FALSE
 */
BOOL UpdateStartupShortcut(void);

#endif // STARTUP_H
