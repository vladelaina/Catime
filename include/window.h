/**
 * @file window.h
 * @brief Main window management and visual effects
 * @version 2.0 - Refactored: Removed deprecated functions, enhanced documentation
 * 
 * Provides comprehensive window management including:
 * - DPI-aware window creation and positioning
 * - Multi-monitor support with active display detection
 * - Visual effects (transparency, blur, click-through)
 * - Always-on-top and desktop-level attachment
 * - Window state persistence
 */

#ifndef WINDOW_H
#define WINDOW_H

#include <windows.h>
#include <dwmapi.h>

/* ============================================================================
 * Window geometry
 * ============================================================================ */

/** @brief Base window width in pixels (before scaling) */
extern int CLOCK_BASE_WINDOW_WIDTH;

/** @brief Base window height in pixels (before scaling) */
extern int CLOCK_BASE_WINDOW_HEIGHT;

/** @brief Current window scale factor (0.5 to 100.0) */
extern float CLOCK_WINDOW_SCALE;

/** @brief Window X position on screen (in pixels) */
extern int CLOCK_WINDOW_POS_X;

/** @brief Window Y position on screen (in pixels) */
extern int CLOCK_WINDOW_POS_Y;

/* ============================================================================
 * Window interaction state
 * ============================================================================ */

/** @brief Edit mode enabled (allows dragging and resizing) */
extern BOOL CLOCK_EDIT_MODE;

/** @brief Window currently being dragged */
extern BOOL CLOCK_IS_DRAGGING;

/** @brief Last recorded mouse position (for drag operations) */
extern POINT CLOCK_LAST_MOUSE_POS;

/** @brief Window always-on-top state */
extern BOOL CLOCK_WINDOW_TOPMOST;

/* ============================================================================
 * Text rendering optimization
 * ============================================================================ */

/** @brief Cached text bounding rectangle */
extern RECT CLOCK_TEXT_RECT;

/** @brief Text rectangle validity flag */
extern BOOL CLOCK_TEXT_RECT_VALID;

/* ============================================================================
 * Scaling constraints
 * ============================================================================ */

/** @brief Minimum allowed scale factor */
#define MIN_SCALE_FACTOR 0.5f

/** @brief Maximum allowed scale factor */
#define MAX_SCALE_FACTOR 100.0f

/* ============================================================================
 * Font configuration
 * ============================================================================ */

/** @brief Current font scale factor (synchronized with window scale) */
extern float CLOCK_FONT_SCALE_FACTOR;

/** @brief Base font size in points */
extern int CLOCK_BASE_FONT_SIZE;

/* ============================================================================
 * Window lifecycle functions
 * ============================================================================ */

/**
 * @brief Create and initialize main application window
 * @param hInstance Application instance handle
 * @param nCmdShow Initial window show state (SW_SHOW, SW_HIDE, etc.)
 * @return Window handle on success, NULL on failure
 * 
 * Performs complete window setup including:
 * - Window class registration
 * - Extended style configuration (layered, transparency)
 * - Initial positioning and sizing
 * - Tray icon initialization
 * - Visual effects application
 * 
 * @note Logs all major steps for diagnostics
 * @see InitializeApplication, SetWindowTopmost, SetBlurBehind
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow);

/**
 * @brief Initialize application subsystems and load configuration
 * @param hInstance Application instance handle
 * @return TRUE if initialization succeeded, FALSE on error
 * 
 * Initialization sequence:
 * 1. DPI awareness (with fallback for older Windows versions)
 * 2. Configuration loading (from config.ini)
 * 3. Font extraction and loading
 * 4. Language and startup settings
 * 
 * @note This must be called before CreateMainWindow
 * @see CreateMainWindow
 */
BOOL InitializeApplication(HINSTANCE hInstance);

/* ============================================================================
 * Visual effects
 * ============================================================================ */

/**
 * @brief Enable or disable window click-through behavior
 * @param hwnd Window handle
 * @param enable TRUE to enable click-through (mouse events pass through), FALSE to disable
 * 
 * Click-through mode makes the window transparent to mouse input,
 * useful for overlay displays that should not intercept clicks.
 * 
 * @note Automatically adjusts WS_EX_TRANSPARENT style and layered attributes
 * @see SetBlurBehind
 */
void SetClickThrough(HWND hwnd, BOOL enable);

/**
 * @brief Enable or disable blur-behind visual effect
 * @param hwnd Window handle
 * @param enable TRUE to enable blur effect, FALSE to disable
 * 
 * Applies Windows 10+ acrylic/blur effect behind window.
 * Falls back to DWM blur on older systems.
 * 
 * @note Requires layered window and desktop composition enabled
 * @see InitDWMFunctions
 */
void SetBlurBehind(HWND hwnd, BOOL enable);

/**
 * @brief Initialize Desktop Window Manager functions for blur effects
 * @return TRUE if DWM functions loaded successfully, FALSE otherwise
 * 
 * Dynamically loads dwmapi.dll to access blur functionality.
 * Should be called during application initialization.
 * 
 * @note Not required on Windows 10+ where SetWindowCompositionAttribute is available
 */
BOOL InitDWMFunctions(void);

/* ============================================================================
 * Window positioning and multi-monitor support
 * ============================================================================ */

/**
 * @brief Adjust window position to ensure visibility on active monitor
 * @param hwnd Window handle
 * @param forceOnScreen TRUE to force repositioning if window is off-screen or on inactive display
 * 
 * Handles scenarios such as:
 * - Window positioned on disconnected monitor
 * - Monitor disabled (e.g., "Second screen only" mode)
 * - Window completely off-screen
 * 
 * Automatically centers window on best available active monitor.
 * 
 * @note Saves new position to configuration if repositioning occurs
 * @see SaveWindowSettings
 */
void AdjustWindowPosition(HWND hwnd, BOOL forceOnScreen);

/**
 * @brief Save current window position and scale to configuration file
 * @param hwnd Window handle
 * 
 * Persists window state to config.ini:
 * - Window position (X, Y coordinates)
 * - Window scale factor
 * 
 * @note Updates global variables CLOCK_WINDOW_POS_X/Y
 * @see AdjustWindowPosition
 */
void SaveWindowSettings(HWND hwnd);

/* ============================================================================
 * Window z-order management
 * ============================================================================ */

/**
 * @brief Set window always-on-top behavior
 * @param hwnd Window handle
 * @param topmost TRUE for always-on-top, FALSE for normal z-order
 * 
 * Topmost mode:
 * - Window stays above all non-topmost windows
 * - Can be activated and focused
 * 
 * Normal mode:
 * - Window parented to Progman (resists Win+D minimize-all)
 * - Stays visible but below topmost windows
 * 
 * @note Persists setting to configuration file
 * @see ReattachToDesktop, WriteConfigTopmost
 */
void SetWindowTopmost(HWND hwnd, BOOL topmost);

/**
 * @brief Attach window to desktop wallpaper level
 * @param hwnd Window handle
 * 
 * Parents window to WorkerW (desktop worker) for lowest z-order.
 * Window appears behind all other windows, above wallpaper.
 * 
 * Use cases:
 * - Desktop widget behavior
 * - Wallpaper-like overlays
 * 
 * @note Window will not respond to Win+D or taskbar clicks
 * @see SetWindowTopmost
 */
void ReattachToDesktop(HWND hwnd);

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * @brief Show file selection dialog
 * @param hwnd Parent window handle
 * @param filePath Buffer to store selected file path (wide string)
 * @param maxPath Maximum path buffer size (in wide characters)
 * @return TRUE if file was selected, FALSE if user cancelled
 * 
 * Uses standard Windows common dialog (OPENFILENAME).
 * Filter shows all files (*.*).
 * 
 * @note filePath buffer must be pre-allocated by caller
 */
BOOL OpenFileDialog(HWND hwnd, wchar_t* filePath, DWORD maxPath);

#endif // WINDOW_H
