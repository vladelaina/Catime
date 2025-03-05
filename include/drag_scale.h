/**
 * @file drag_scale.h
 * @brief 窗口拖动和缩放功能接口
 * 
 * 本文件定义了应用程序窗口的拖动和缩放功能接口，
 * 包括鼠标拖动窗口和滚轮缩放窗口的功能。
 */

#ifndef DRAG_SCALE_H
#define DRAG_SCALE_H

#include <windows.h>

/**
 * @brief 处理窗口拖动事件
 * @param hwnd 窗口句柄
 * @return BOOL 是否处理了事件
 * 
 * 在编辑模式下，处理鼠标拖动窗口的事件。
 * 根据鼠标移动距离更新窗口位置。
 */
BOOL HandleDragWindow(HWND hwnd);

/**
 * @brief 处理窗口缩放事件
 * @param hwnd 窗口句柄
 * @param delta 鼠标滚轮增量
 * @return BOOL 是否处理了事件
 * 
 * 在编辑模式下，处理鼠标滚轮缩放窗口的事件。
 * 根据滚轮方向调整窗口和字体大小。
 */
BOOL HandleScaleWindow(HWND hwnd, int delta);

/**
 * @brief 开始拖动窗口
 * @param hwnd 窗口句柄
 * 
 * 在编辑模式下，开始拖动窗口操作。
 * 记录初始鼠标位置并设置捕获。
 */
void StartDragWindow(HWND hwnd);

/**
 * @brief 结束拖动窗口
 * @param hwnd 窗口句柄
 * 
 * 结束拖动窗口操作。
 * 释放鼠标捕获并调整窗口位置。
 */
void EndDragWindow(HWND hwnd);

#endif // DRAG_SCALE_H