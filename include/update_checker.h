/**
 * @file update_checker.h
 * @brief 极简的应用程序更新检查功能接口
 * 
 * 本文件定义了应用程序检查更新、打开浏览器下载和删除配置文件的功能接口。
 */

#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <windows.h>

/**
 * @brief 检查应用程序更新
 * @param hwnd 窗口句柄
 * 
 * 连接到GitHub检查是否有新版本。如果有，会提示用户是否通过浏览器下载。
 * 如果用户确认，将打开浏览器到下载页面，删除配置文件，并退出程序。
 */
void CheckForUpdate(HWND hwnd);

/**
 * @brief 静默检查应用程序更新
 * @param hwnd 窗口句柄
 * @param silentCheck 是否仅在有更新时显示提示
 * 
 * 连接到GitHub检查是否有新版本。
 * 如果silentCheck为TRUE，仅在有更新时才显示提示；
 * 如果为FALSE，则无论是否有更新都会显示结果。
 */
void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck);

/**
 * @brief 比较版本号
 * @param version1 第一个版本号字符串
 * @param version2 第二个版本号字符串
 * @return 如果version1 > version2返回1，如果相等返回0，如果version1 < version2返回-1
 */
int CompareVersions(const char* version1, const char* version2);

#endif // UPDATE_CHECKER_H 