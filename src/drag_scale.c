/**
 * @file drag_scale.c
 * @brief 窗口拖动和缩放功能实现
 * 
 * 本文件实现了应用程序窗口的拖动和缩放功能，
 * 包括鼠标拖动窗口和滚轮缩放窗口的功能。
 */

#include <windows.h>
#include "../include/window.h"
#include "../include/config.h"
#include "../include/drag_scale.h"

// 添加变量来记录编辑模式前的置顶状态
BOOL PREVIOUS_TOPMOST_STATE = FALSE;

/**
 * @brief 开始拖动窗口
 * @param hwnd 窗口句柄
 * 
 * 在编辑模式下，开始拖动窗口操作。
 * 记录初始鼠标位置并设置捕获。
 */
void StartDragWindow(HWND hwnd) {
    if (CLOCK_EDIT_MODE) {
        CLOCK_IS_DRAGGING = TRUE;
        SetCapture(hwnd);
        GetCursorPos(&CLOCK_LAST_MOUSE_POS);
    }
}

/**
 * @brief 开始编辑模式
 * @param hwnd 窗口句柄
 * 
 * 启用编辑模式前，确保窗口为置顶状态，
 * 记录原始置顶状态以便退出编辑模式时恢复。
 */
void StartEditMode(HWND hwnd) {
    // 记录当前的置顶状态
    PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
    
    // 如果当前不是置顶状态，先设为置顶
    if (!CLOCK_WINDOW_TOPMOST) {
        SetWindowTopmost(hwnd, TRUE);
    }
    
    // 然后启用编辑模式
    CLOCK_EDIT_MODE = TRUE;
    
    // 应用模糊效果
    SetBlurBehind(hwnd, TRUE);
    
    // 禁用点击穿透
    SetClickThrough(hwnd, FALSE);
    
    // 确保鼠标光标为默认箭头
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    
    // 刷新窗口，添加立即更新
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);  // 确保立即刷新
}

/**
 * @brief 结束编辑模式
 * @param hwnd 窗口句柄
 * 
 * 退出编辑模式，恢复窗口原始置顶状态，
 * 清除模糊效果并更新相关设置。
 */
void EndEditMode(HWND hwnd) {
    if (CLOCK_EDIT_MODE) {
        CLOCK_EDIT_MODE = FALSE;
        
        // 移除模糊效果
        SetBlurBehind(hwnd, FALSE);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
        
        // 恢复点击穿透
        SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
        
        // 如果之前不是置顶状态，恢复为非置顶
        if (!PREVIOUS_TOPMOST_STATE) {
            SetWindowTopmost(hwnd, FALSE);
        }
        
        // 刷新窗口，添加立即更新
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);  // 确保立即刷新
    }
}

/**
 * @brief 结束拖动窗口
 * @param hwnd 窗口句柄
 * 
 * 结束拖动窗口操作。
 * 释放鼠标捕获并调整窗口位置。
 */
void EndDragWindow(HWND hwnd) {
    if (CLOCK_EDIT_MODE && CLOCK_IS_DRAGGING) {
        CLOCK_IS_DRAGGING = FALSE;
        ReleaseCapture();
        // 编辑模式下不强制窗口在屏幕内，允许拖出
        AdjustWindowPosition(hwnd, FALSE);
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief 处理窗口拖动事件
 * @param hwnd 窗口句柄
 * @return BOOL 是否处理了事件
 * 
 * 在编辑模式下，处理鼠标拖动窗口的事件。
 * 根据鼠标移动距离更新窗口位置。
 */
BOOL HandleDragWindow(HWND hwnd) {
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

/**
 * @brief 处理窗口缩放事件
 * @param hwnd 窗口句柄
 * @param delta 鼠标滚轮增量
 * @return BOOL 是否处理了事件
 * 
 * 在编辑模式下，处理鼠标滚轮缩放窗口的事件。
 * 根据滚轮方向调整窗口和字体大小。
 */
BOOL HandleScaleWindow(HWND hwnd, int delta) {
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