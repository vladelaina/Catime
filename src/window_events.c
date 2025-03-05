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

/**
 * @brief 处理窗口大小调整事件
 * @param hwnd 窗口句柄
 * @param delta 鼠标滚轮增量
 * @return BOOL 是否处理了事件
 */
BOOL HandleWindowResize(HWND hwnd, int delta) {
    if (CLOCK_EDIT_MODE) {
        float old_scale = CLOCK_FONT_SCALE_FACTOR;
        
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);
        int oldWidth = windowRect.right - windowRect.left;
        int oldHeight = windowRect.bottom - windowRect.top;
        
        float scaleFactor = 1.1f;
        if (delta > 0) {
            CLOCK_FONT_SCALE_FACTOR *= scaleFactor;
            CLOCK_WINDOW_SCALE = CLOCK_FONT_SCALE_FACTOR;
        } else {
            CLOCK_FONT_SCALE_FACTOR /= scaleFactor;
            CLOCK_WINDOW_SCALE = CLOCK_FONT_SCALE_FACTOR;
        }
        
        // 保持缩放范围限制
        if (CLOCK_FONT_SCALE_FACTOR < MIN_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MIN_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MIN_SCALE_FACTOR;
        }
        if (CLOCK_FONT_SCALE_FACTOR > MAX_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MAX_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MAX_SCALE_FACTOR;
        }
        
        if (old_scale != CLOCK_FONT_SCALE_FACTOR) {
            // 计算新尺寸
            int newWidth = (int)(oldWidth * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            int newHeight = (int)(oldHeight * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            
            // 保持窗口中心位置不变
            int newX = windowRect.left + (oldWidth - newWidth)/2;
            int newY = windowRect.top + (oldHeight - newHeight)/2;
            
            SetWindowPos(hwnd, NULL, 
                newX, newY,
                newWidth, newHeight,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
            
            // 触发重绘
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            
            // 保存设置
            SaveWindowSettings(hwnd);
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief 处理窗口位置调整事件
 * @param hwnd 窗口句柄
 * @return BOOL 是否处理了事件
 */
BOOL HandleWindowMove(HWND hwnd) {
    if (CLOCK_EDIT_MODE && CLOCK_IS_DRAGGING) {
        POINT currentPos;
        GetCursorPos(&currentPos);
        
        int deltaX = currentPos.x - CLOCK_LAST_MOUSE_POS.x;
        int deltaY = currentPos.y - CLOCK_LAST_MOUSE_POS.y;
        
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);
        
        SetWindowPos(hwnd, NULL,
            windowRect.left + deltaX,
            windowRect.top + deltaY,
            windowRect.right - windowRect.left,   
            windowRect.bottom - windowRect.top,   
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW   
        );
        
        CLOCK_LAST_MOUSE_POS = currentPos;
        
        UpdateWindow(hwnd);
        
        // 更新位置变量并保存设置
        CLOCK_WINDOW_POS_X = windowRect.left + deltaX;
        CLOCK_WINDOW_POS_Y = windowRect.top + deltaY;
        SaveWindowSettings(hwnd);
        
        return TRUE;
    }
    return FALSE;
}