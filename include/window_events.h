/**
 * @file window_events.h
 * @brief 基本窗口事件处理接口
 * 
 * 本文件定义了应用程序窗口的基本事件处理功能接口，
 * 包括窗口创建、销毁、大小调整和位置调整等基本功能。
 */

#ifndef WINDOW_EVENTS_H
#define WINDOW_EVENTS_H

#include <windows.h>

/**
 * @brief 处理窗口创建事件
 * @param hwnd 窗口句柄
 * @return BOOL 处理结果
 */
BOOL HandleWindowCreate(HWND hwnd);

/**
 * @brief 处理窗口销毁事件
 * @param hwnd 窗口句柄
 */
void HandleWindowDestroy(HWND hwnd);

/**
 * @brief 处理窗口重置事件
 * @param hwnd 窗口句柄
 */
void HandleWindowReset(HWND hwnd);

/**
 * @brief 处理窗口大小调整事件
 * @param hwnd 窗口句柄
 * @param delta 鼠标滚轮增量
 * @return BOOL 是否处理了事件
 */
BOOL HandleWindowResize(HWND hwnd, int delta);

/**
 * @brief 处理窗口位置调整事件
 * @param hwnd 窗口句柄
 * @return BOOL 是否处理了事件
 */
BOOL HandleWindowMove(HWND hwnd);

#endif // WINDOW_EVENTS_H