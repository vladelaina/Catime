/**
 * @file drag_scale.c
 * @brief Window dragging and scaling functionality implementation
 * 
 * This file implements the dragging and scaling functionality of the application window,
 * including mouse dragging of the window and mouse wheel scaling of the window.
 */

#include <windows.h>
#include "../include/window.h"
#include "../include/config.h"
#include "../include/drag_scale.h"

// Add variable to record the topmost state before edit mode
BOOL PREVIOUS_TOPMOST_STATE = FALSE;

void StartDragWindow(HWND hwnd) {
    if (CLOCK_EDIT_MODE) {
        CLOCK_IS_DRAGGING = TRUE;
        SetCapture(hwnd);
        GetCursorPos(&CLOCK_LAST_MOUSE_POS);
    }
}

void StartEditMode(HWND hwnd) {
    // Record current topmost state
    PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
    
    // If currently not in topmost state, set to topmost
    if (!CLOCK_WINDOW_TOPMOST) {
        SetWindowTopmost(hwnd, TRUE);
    }
    
    // Then enable edit mode
    CLOCK_EDIT_MODE = TRUE;
    
    // Apply blur effect
    SetBlurBehind(hwnd, TRUE);
    
    // Disable click-through
    SetClickThrough(hwnd, FALSE);
    
    // Ensure mouse cursor is default arrow
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    
    // Refresh window, add immediate update
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);  // Ensure immediate refresh
}

void EndEditMode(HWND hwnd) {
    if (CLOCK_EDIT_MODE) {
        CLOCK_EDIT_MODE = FALSE;
        
        // Remove blur effect
        SetBlurBehind(hwnd, FALSE);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
        
        // Restore click-through
        SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
        
        // If previously not in topmost state, restore to non-topmost
        if (!PREVIOUS_TOPMOST_STATE) {
            SetWindowTopmost(hwnd, FALSE);
            
            // Force redraw and schedule a delayed re-assertion to ensure visibility
            // This follows the same pattern used in window_procedure.c for topmost toggle
            InvalidateRect(hwnd, NULL, TRUE);
            RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
            KillTimer(hwnd, 1002);
            SetTimer(hwnd, 1002, 150, NULL);
        } else {
            // If restoring to topmost state, just refresh normally
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);  // Ensure immediate refresh
        }
    }
}

void EndDragWindow(HWND hwnd) {
    if (CLOCK_EDIT_MODE && CLOCK_IS_DRAGGING) {
        CLOCK_IS_DRAGGING = FALSE;
        ReleaseCapture();
        // In edit mode, don't force window to stay on screen, allow dragging out
        AdjustWindowPosition(hwnd, FALSE);
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

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
        
        // Update position variables and save settings
        CLOCK_WINDOW_POS_X = windowRect.left + deltaX;
        CLOCK_WINDOW_POS_Y = windowRect.top + deltaY;
        SaveWindowSettings(hwnd);
        
        return TRUE;
    }
    return FALSE;
}

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
        
        // Maintain scale range limits
        if (CLOCK_FONT_SCALE_FACTOR < MIN_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MIN_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MIN_SCALE_FACTOR;
        }
        if (CLOCK_FONT_SCALE_FACTOR > MAX_SCALE_FACTOR) {
            CLOCK_FONT_SCALE_FACTOR = MAX_SCALE_FACTOR;
            CLOCK_WINDOW_SCALE = MAX_SCALE_FACTOR;
        }
        
        if (old_scale != CLOCK_FONT_SCALE_FACTOR) {
            // Calculate new dimensions
            int newWidth = (int)(oldWidth * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            int newHeight = (int)(oldHeight * (CLOCK_FONT_SCALE_FACTOR / old_scale));
            
            // Keep window center position unchanged
            int newX = windowRect.left + (oldWidth - newWidth)/2;
            int newY = windowRect.top + (oldHeight - newHeight)/2;
            
            SetWindowPos(hwnd, NULL, 
                newX, newY,
                newWidth, newHeight,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
            
            // Trigger redraw
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            
            // Save settings
            SaveWindowSettings(hwnd);
            return TRUE;
        }
    }
    return FALSE;
}