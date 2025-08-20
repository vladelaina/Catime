/**
 * @file drag_scale.c
 * @brief Window dragging and scaling functionality for edit mode
 */

#include <windows.h>
#include "../include/window.h"
#include "../include/config.h"
#include "../include/drag_scale.h"

/** @brief Previous topmost state before entering edit mode */
BOOL PREVIOUS_TOPMOST_STATE = FALSE;

/**
 * @brief Initialize window dragging operation
 * @param hwnd Window handle to start dragging
 */
void StartDragWindow(HWND hwnd) {
    if (CLOCK_EDIT_MODE) {
        /** Enable dragging state and capture mouse */
        CLOCK_IS_DRAGGING = TRUE;
        SetCapture(hwnd);
        GetCursorPos(&CLOCK_LAST_MOUSE_POS);
    }
}

/**
 * @brief Enter edit mode for window positioning and scaling
 * @param hwnd Window handle to enable edit mode for
 */
void StartEditMode(HWND hwnd) {
    /** Save current topmost state for restoration */
    PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
    
    /** Ensure window is topmost during editing */
    if (!CLOCK_WINDOW_TOPMOST) {
        SetWindowTopmost(hwnd, TRUE);
    }
    
    /** Enable edit mode globally */
    CLOCK_EDIT_MODE = TRUE;
    
    /** Apply visual effects for edit mode */
    SetBlurBehind(hwnd, TRUE);
    
    /** Disable click-through to allow interaction */
    SetClickThrough(hwnd, FALSE);
    
    /** Set standard arrow cursor */
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
    
    /** Refresh window display */
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

/**
 * @brief Exit edit mode and restore normal window behavior
 * @param hwnd Window handle to disable edit mode for
 */
void EndEditMode(HWND hwnd) {
    if (CLOCK_EDIT_MODE) {
        /** Disable edit mode globally */
        CLOCK_EDIT_MODE = FALSE;
        
        /** Remove visual effects and restore transparency */
        SetBlurBehind(hwnd, FALSE);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
        
        /** Re-enable click-through behavior */
        SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
        
        /** Restore original topmost state */
        if (!PREVIOUS_TOPMOST_STATE) {
            SetWindowTopmost(hwnd, FALSE);
            
            /** Force complete redraw for non-topmost windows */
            InvalidateRect(hwnd, NULL, TRUE);
            RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
            KillTimer(hwnd, 1002);
            SetTimer(hwnd, 1002, 150, NULL);
        } else {
            /** Simple refresh for topmost windows */
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);
        }
    }
}

/**
 * @brief Finish window dragging operation
 * @param hwnd Window handle to stop dragging
 */
void EndDragWindow(HWND hwnd) {
    if (CLOCK_EDIT_MODE && CLOCK_IS_DRAGGING) {
        /** Disable dragging state and release mouse capture */
        CLOCK_IS_DRAGGING = FALSE;
        ReleaseCapture();
        /** Ensure window position is within screen bounds */
        AdjustWindowPosition(hwnd, FALSE);
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Process window dragging based on mouse movement
 * @param hwnd Window handle being dragged
 * @return TRUE if window was moved, FALSE otherwise
 */
BOOL HandleDragWindow(HWND hwnd) {
    if (CLOCK_EDIT_MODE && CLOCK_IS_DRAGGING) {
        /** Get current mouse position */
        POINT currentPos;
        GetCursorPos(&currentPos);
        
        /** Calculate movement delta from last position */
        int deltaX = currentPos.x - CLOCK_LAST_MOUSE_POS.x;
        int deltaY = currentPos.y - CLOCK_LAST_MOUSE_POS.y;
        
        /** Get current window rectangle */
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);
        
        /** Move window by delta amount */
        SetWindowPos(hwnd, NULL,
            windowRect.left + deltaX,
            windowRect.top + deltaY,
            windowRect.right - windowRect.left,   
            windowRect.bottom - windowRect.top,   
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW   
        );
        
        /** Update last mouse position for next iteration */
        CLOCK_LAST_MOUSE_POS = currentPos;
        
        UpdateWindow(hwnd);
        
        /** Save new position to global variables and config */
        CLOCK_WINDOW_POS_X = windowRect.left + deltaX;
        CLOCK_WINDOW_POS_Y = windowRect.top + deltaY;
        SaveWindowSettings(hwnd);
        
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Handle window scaling via mouse wheel in edit mode
 * @param hwnd Window handle to scale
 * @param delta Mouse wheel delta (positive for zoom in, negative for zoom out)
 * @return TRUE if scaling was applied, FALSE otherwise
 */
BOOL HandleScaleWindow(HWND hwnd, int delta) {
    if (CLOCK_EDIT_MODE) {
        /** Store original scale for comparison */
        float old_scale = CLOCK_FONT_SCALE_FACTOR;
        
        /** Get current window dimensions */
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);
        int oldWidth = windowRect.right - windowRect.left;
        int oldHeight = windowRect.bottom - windowRect.top;
        
        /** Apply scaling based on wheel direction */
        float scaleFactor = 1.1f;
        if (delta > 0) {
            /** Zoom in */
            CLOCK_FONT_SCALE_FACTOR *= scaleFactor;
            CLOCK_WINDOW_SCALE = CLOCK_FONT_SCALE_FACTOR;
        } else {
            /** Zoom out */
            CLOCK_FONT_SCALE_FACTOR /= scaleFactor;
            CLOCK_WINDOW_SCALE = CLOCK_FONT_SCALE_FACTOR;
        }
        
        /** Enforce scale limits */
        if (CLOCK_FONT_SCALE_FACTOR < MIN_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MIN_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MIN_SCALE_FACTOR;
        }
        if (CLOCK_FONT_SCALE_FACTOR > MAX_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MAX_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MAX_SCALE_FACTOR;
        }
        
        /** Apply new size if scale changed */
        if (old_scale != CLOCK_FONT_SCALE_FACTOR) {
            /** Calculate new dimensions proportionally */
            int newWidth = (int)(oldWidth * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            int newHeight = (int)(oldHeight * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            
            /** Center the scaled window on original position */
            int newX = windowRect.left + (oldWidth - newWidth)/2;
            int newY = windowRect.top + (oldHeight - newHeight)/2;
            
            /** Apply new position and size */
            SetWindowPos(hwnd, NULL, 
                newX, newY,
                newWidth, newHeight,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
            
            /** Refresh window display */
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            
            /** Save new settings to configuration */
            SaveWindowSettings(hwnd);
            return TRUE;
        }
    }
    return FALSE;
}