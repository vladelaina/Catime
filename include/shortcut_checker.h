/**
 * @file shortcut_checker.h
 * @brief 检测和创建桌面快捷方式的功能
 *
 * 提供检测程序是否从应用商店或WinGet安装，
 * 并在需要时在桌面创建快捷方式的功能。
 */

#ifndef SHORTCUT_CHECKER_H
#define SHORTCUT_CHECKER_H

#include <windows.h>

/**
 * @brief 检查并创建桌面快捷方式
 * 
 * 检查程序是否从Windows应用商店或WinGet安装，
 * 如果是且配置中没有标记为已检查过，则检查桌面
 * 是否有程序的快捷方式，如果没有则创建一个。
 * 
 * @return int 0表示无需创建或创建成功，1表示创建失败
 */
int CheckAndCreateShortcut(void);

#endif /* SHORTCUT_CHECKER_H */ 