/**
 * @file async_update_checker.h
 * @brief 极简的异步应用程序更新检查功能接口
 * 
 * 本文件定义了应用程序异步检查更新的功能接口，确保更新检查不会阻塞主线程。
 */

#ifndef ASYNC_UPDATE_CHECKER_H
#define ASYNC_UPDATE_CHECKER_H

#include <windows.h>

/**
 * @brief 异步检查应用程序更新
 * @param hwnd 窗口句柄
 * @param silentCheck 是否为静默检查(仅在有更新时显示提示)
 * 
 * 在单独的线程中连接到GitHub检查是否有新版本。
 * 此函数立即返回，不会阻塞主线程。
 */
void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck);

#endif // ASYNC_UPDATE_CHECKER_H