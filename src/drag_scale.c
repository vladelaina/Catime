/**
 * @file drag_scale.c
 * @brief Window dragging and scaling functionality for edit mode
 * 
 * Implements interactive window manipulation with optimizations:
 * - Debounced configuration saves to reduce disk I/O
 * - Centralized window refresh logic to eliminate code duplication
 * - Scale factor clamping with clear boundary enforcement
 * - Modular design for improved testability and maintenance
 * 
 * @version 2.0 - Refactored for enhanced code quality
 */

#include <windows.h>
#include "../include/window.h"
#include "../include/config.h"
#include "../include/drag_scale.h"

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/** @brief Previous topmost state before entering edit mode */
BOOL PREVIOUS_TOPMOST_STATE = FALSE;

/** @brief Timer ID for debounced configuration save */
static UINT_PTR g_configSaveTimer = 0;

/* ============================================================================
 * Internal Utility Functions
 * ============================================================================ */

/**
 * @brief Refresh window display with optional background erase
 * @param hwnd Window handle to refresh
 * @param eraseBackground Whether to erase background before redrawing
 * 
 * Centralizes the common pattern of InvalidateRect + UpdateWindow
 * to eliminate code duplication and improve consistency.
 */
static inline void RefreshWindow(HWND hwnd, BOOL eraseBackground) {
    InvalidateRect(hwnd, NULL, eraseBackground);
    UpdateWindow(hwnd);
}

/**
 * @brief Clamp scale factor within valid bounds
 * @param scale Scale factor to validate
 * @return Clamped scale factor within [MIN_SCALE_FACTOR, MAX_SCALE_FACTOR]
 * 
 * Ensures scale factor stays within hardware/performance limits.
 */
static inline float ClampScaleFactor(float scale) {
    if (scale < MIN_SCALE_FACTOR) return MIN_SCALE_FACTOR;
    if (scale > MAX_SCALE_FACTOR) return MAX_SCALE_FACTOR;
    return scale;
}

/**
 * @brief Calculate centered position for scaled window
 * @param originalPos Original window position
 * @param originalSize Original window size
 * @param newSize New window size after scaling
 * @return New position that centers the scaled window on original position
 */
static inline int CalculateCenteredPosition(int originalPos, int originalSize, int newSize) {
    return originalPos + (originalSize - newSize) / 2;
}

/* ============================================================================
 * Configuration Persistence
 * ============================================================================ */

/**
 * @brief Timer callback for delayed configuration save
 * @param hwnd Window handle
 * @param msg Message (unused)
 * @param idEvent Timer ID
 * @param dwTime System time (unused)
 * 
 * Executes actual save operation after debounce delay expires.
 */
static VOID CALLBACK ConfigSaveTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime) {
    (void)msg;
    (void)dwTime;
    
    if (idEvent == TIMER_ID_CONFIG_SAVE) {
        SaveWindowSettings(hwnd);
        /** Use constant TIMER_ID_CONFIG_SAVE for consistency */
        KillTimer(hwnd, TIMER_ID_CONFIG_SAVE);
        g_configSaveTimer = 0;
    }
}

/**
 * @brief Schedule delayed configuration save with debouncing
 * @param hwnd Window handle for timer context
 * 
 * Implements debouncing mechanism to prevent excessive disk I/O during
 * continuous drag/scale operations. Previous timer is cancelled and
 * replaced with new one on each call, ensuring save only occurs after
 * operations have stopped for CONFIG_SAVE_DELAY_MS milliseconds.
 */
void ScheduleConfigSave(HWND hwnd) {
    /** 
     * Cancel previous timer if exists. Always use TIMER_ID_CONFIG_SAVE
     * constant instead of g_configSaveTimer to ensure correctness even
     * if previous SetTimer failed.
     */
    if (g_configSaveTimer != 0) {
        KillTimer(hwnd, TIMER_ID_CONFIG_SAVE);
    }
    
    /** Schedule new delayed save */
    g_configSaveTimer = SetTimer(hwnd, TIMER_ID_CONFIG_SAVE, 
                                 CONFIG_SAVE_DELAY_MS, 
                                 (TIMERPROC)ConfigSaveTimerProc);
}

/* ============================================================================
 * Edit Mode Management
 * ============================================================================ */

/**
 * @brief Initialize window dragging operation with mouse capture
 * @param hwnd Window handle to start dragging
 * 
 * Sets dragging state and captures mouse input. Only operates when
 * in edit mode to prevent accidental dragging during normal use.
 */
void StartDragWindow(HWND hwnd) {
    if (!CLOCK_EDIT_MODE) return;
    
    CLOCK_IS_DRAGGING = TRUE;
    SetCapture(hwnd);
    GetCursorPos(&CLOCK_LAST_MOUSE_POS);
}

/**
 * @brief Enter edit mode for interactive window manipulation
 * @param hwnd Window handle to enable edit mode for
 * 
 * Prepares window for user interaction by:
 * 1. Saving current topmost state for later restoration
 * 2. Ensuring window is topmost for easy manipulation
 * 3. Applying visual feedback (blur effect)
 * 4. Disabling click-through to capture mouse events
 * 5. Setting appropriate cursor and refreshing display
 */
void StartEditMode(HWND hwnd) {
    /** Save current topmost state for restoration on exit */
    PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
    
    /** Ensure window is topmost during editing for easy access */
    if (!CLOCK_WINDOW_TOPMOST) {
        SetWindowTopmost(hwnd, TRUE);
    }
    
    /** Enable edit mode globally */
    CLOCK_EDIT_MODE = TRUE;
    
    /** Apply visual feedback: blur effect indicates edit mode is active */
    SetBlurBehind(hwnd, TRUE);
    
    /** Disable click-through to allow window interaction */
    SetClickThrough(hwnd, FALSE);
    
    /** Set standard arrow cursor for better UX */
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
    
    /** Refresh display to show edit mode visual changes */
    RefreshWindow(hwnd, TRUE);
}

/**
 * @brief Exit edit mode and restore normal window behavior
 * @param hwnd Window handle to disable edit mode for
 * 
 * Restores window to pre-edit state:
 * 1. Disables edit mode flag
 * 2. Removes blur effect and restores transparency
 * 3. Re-enables click-through behavior
 * 4. Restores original topmost state
 * 5. Refreshes display with appropriate method based on topmost state
 */
void EndEditMode(HWND hwnd) {
    if (!CLOCK_EDIT_MODE) return;
    
    /** Disable edit mode globally */
    CLOCK_EDIT_MODE = FALSE;
    
    /** Remove visual effects and restore transparency */
    SetBlurBehind(hwnd, FALSE);
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    
    /** Re-enable click-through behavior for normal operation */
    SetClickThrough(hwnd, TRUE);
    
    /** Restore original topmost state */
    if (!PREVIOUS_TOPMOST_STATE) {
        SetWindowTopmost(hwnd, FALSE);
        
        /** Non-topmost windows require full redraw for proper rendering */
        InvalidateRect(hwnd, NULL, TRUE);
        RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
        
        /** Schedule refresh timer for post-mode-change stability */
        KillTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH);
        SetTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH, TIMER_REFRESH_INTERVAL_MS, NULL);
    } else {
        /** Topmost windows can use simpler refresh */
        RefreshWindow(hwnd, TRUE);
    }
}

/* ============================================================================
 * Window Dragging Implementation
 * ============================================================================ */

/**
 * @brief Finalize window dragging and ensure valid position
 * @param hwnd Window handle to stop dragging
 * 
 * Releases mouse capture, validates window position, and refreshes display.
 * Only operates when both edit mode and dragging state are active.
 */
void EndDragWindow(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || !CLOCK_IS_DRAGGING) return;
    
    /** Disable dragging state and release mouse capture */
    CLOCK_IS_DRAGGING = FALSE;
    ReleaseCapture();
    
    /** Ensure window position is within screen bounds */
    AdjustWindowPosition(hwnd, FALSE);
    
    /** Refresh display and schedule final config save */
    RefreshWindow(hwnd, TRUE);
    ScheduleConfigSave(hwnd);
}

/**
 * @brief Process window dragging based on mouse movement delta
 * @param hwnd Window handle being dragged
 * @return TRUE if window was repositioned, FALSE if not in drag mode
 * 
 * Calculates movement delta from last recorded mouse position and applies
 * the translation to window. Updates are performed without redrawing to
 * maintain smooth dragging performance. Configuration save is debounced.
 */
BOOL HandleDragWindow(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || !CLOCK_IS_DRAGGING) return FALSE;
    
    /** Get current mouse position and calculate movement delta */
    POINT currentPos;
    GetCursorPos(&currentPos);
    int deltaX = currentPos.x - CLOCK_LAST_MOUSE_POS.x;
    int deltaY = currentPos.y - CLOCK_LAST_MOUSE_POS.y;
    
    /** Get current window rectangle for size preservation */
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    
    /** Apply translation without changing size or z-order */
    SetWindowPos(hwnd, NULL,
        windowRect.left + deltaX,
        windowRect.top + deltaY,
        width, height,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    
    /** Update tracking variables for next iteration */
    CLOCK_LAST_MOUSE_POS = currentPos;
    CLOCK_WINDOW_POS_X = windowRect.left + deltaX;
    CLOCK_WINDOW_POS_Y = windowRect.top + deltaY;
    
    /** Force immediate visual update for smooth feedback */
    UpdateWindow(hwnd);
    
    /** Schedule debounced config save to reduce I/O frequency */
    ScheduleConfigSave(hwnd);
    
    return TRUE;
}

/* ============================================================================
 * Window Scaling Implementation
 * ============================================================================ */

/**
 * @brief Handle window scaling via mouse wheel input
 * @param hwnd Window handle to scale
 * @param delta Mouse wheel delta (positive = zoom in, negative = zoom out)
 * @return TRUE if scaling was applied, FALSE if not in edit mode or at limits
 * 
 * Applies proportional scaling centered on current window position. Scale
 * factor is adjusted by SCALE_FACTOR_STEP (10%) per wheel notch and clamped
 * to valid bounds. If scale reaches limits, returns FALSE to indicate no
 * change occurred. Configuration save is debounced for performance.
 */
BOOL HandleScaleWindow(HWND hwnd, int delta) {
    if (!CLOCK_EDIT_MODE) return FALSE;
    
    /** Store original scale for change detection */
    float oldScale = CLOCK_FONT_SCALE_FACTOR;
    
    /** Get current window dimensions */
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int oldWidth = windowRect.right - windowRect.left;
    int oldHeight = windowRect.bottom - windowRect.top;
    
    /** Calculate new scale factor based on wheel direction */
    float newScale = (delta > 0) 
        ? oldScale * SCALE_FACTOR_STEP      /** Zoom in: increase by 10% */
        : oldScale / SCALE_FACTOR_STEP;     /** Zoom out: decrease by 10% */
    
    /** Clamp scale to valid bounds */
    newScale = ClampScaleFactor(newScale);
    
    /** Check if scale actually changed (not at limits) */
    if (newScale == oldScale) return FALSE;
    
    /** Apply new scale to both font and window */
    CLOCK_FONT_SCALE_FACTOR = newScale;
    CLOCK_WINDOW_SCALE = newScale;
    
    /** Calculate new dimensions maintaining aspect ratio */
    float scalingRatio = newScale / oldScale;
    int newWidth = (int)(oldWidth * scalingRatio);
    int newHeight = (int)(oldHeight * scalingRatio);
    
    /** Calculate centered position to keep window visually stable */
    int newX = CalculateCenteredPosition(windowRect.left, oldWidth, newWidth);
    int newY = CalculateCenteredPosition(windowRect.top, oldHeight, newHeight);
    
    /** Apply new geometry without changing z-order */
    SetWindowPos(hwnd, NULL, 
        newX, newY, newWidth, newHeight,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    
    /** Refresh display to show scaled content */
    RefreshWindow(hwnd, FALSE);
    
    /** Schedule debounced config save */
    ScheduleConfigSave(hwnd);
    
    return TRUE;
}