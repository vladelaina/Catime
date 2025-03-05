/**
 * @file window_events.c
 * @brief 基本窗口事件处理实现
 * 
 * 本文件实现了应用程序窗口的基本事件处理功能，
 * 包括窗口创建、销毁、大小调整和位置调整等基本功能。
 */

#include <windows.h>
#include "../include/window.h"
#include "../include/tray.h"
#include "../include/config.h"
#include "../include/window_events.h"

/**
 * @brief 处理窗口创建事件
 * @param hwnd 窗口句柄
 * @return BOOL 处理结果
 */
BOOL HandleWindowCreate(HWND hwnd) {
    HWND hwndParent = GetParent(hwnd);
    if (hwndParent != NULL) {
        EnableWindow(hwndParent, TRUE);
    }
    
    // 加载窗口设置
    LoadWindowSettings(hwnd);
    
    // 设置点击穿透
    SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
    
    return TRUE;
}

/**
 * @brief 处理窗口销毁事件
 * @param hwnd 窗口句柄
 */
void HandleWindowDestroy(HWND hwnd) {
    SaveWindowSettings(hwnd);  // 保存窗口设置
    KillTimer(hwnd, 1);
    RemoveTrayIcon();
    PostQuitMessage(0);
}

// 该函数已移至drag_scale.c
BOOL HandleWindowResize(HWND hwnd, int delta) {
    return HandleScaleWindow(hwnd, delta);
}

// 该函数已移至drag_scale.c
BOOL HandleWindowMove(HWND hwnd) {
    return HandleDragWindow(hwnd);
}
