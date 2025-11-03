/**
 * @file window.h
 * @brief Window management with multi-monitor and visual effects
 * 
 * DPI-aware creation prevents blurry text on high-DPI displays.
 * Multi-monitor support detects disconnected displays and repositions automatically.
 * Desktop attachment (WorkerW parent) resists Win+D minimize-all.
 */

#ifndef WINDOW_H
#define WINDOW_H

#include <windows.h>
#include <dwmapi.h>

/* ============================================================================
 * Window geometry
 * ============================================================================ */

extern int CLOCK_BASE_WINDOW_WIDTH;
extern int CLOCK_BASE_WINDOW_HEIGHT;
extern float CLOCK_WINDOW_SCALE;         /**< 0.5 to 100.0 */
extern int CLOCK_WINDOW_POS_X;
extern int CLOCK_WINDOW_POS_Y;

/* ============================================================================
 * Window interaction state
 * ============================================================================ */

extern BOOL CLOCK_EDIT_MODE;
extern BOOL CLOCK_IS_DRAGGING;
extern POINT CLOCK_LAST_MOUSE_POS;
extern BOOL CLOCK_WINDOW_TOPMOST;

/* ============================================================================
 * Text rendering optimization
 * ============================================================================ */

extern RECT CLOCK_TEXT_RECT;            /**< Cached bounds */
extern BOOL CLOCK_TEXT_RECT_VALID;

/* ============================================================================
 * Scaling constraints
 * ============================================================================ */

#define MIN_SCALE_FACTOR 0.5f
#define MAX_SCALE_FACTOR 100.0f

/* ============================================================================
 * Font configuration
 * ============================================================================ */

extern float CLOCK_FONT_SCALE_FACTOR;   /**< Synced with window scale */
extern int CLOCK_BASE_FONT_SIZE;

/* ============================================================================
 * Window lifecycle functions
 * ============================================================================ */

/**
 * @brief Create main window
 * @param nCmdShow Initial show state (SW_SHOW, SW_HIDE, etc.)
 * @return Window handle or NULL
 * 
 * @details
 * Setup: class registration, layered style, positioning, tray, visual effects.
 * Logs all steps for diagnostics.
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow);

/**
 * @brief Initialize subsystems and load config
 * @return TRUE on success
 * 
 * @details Sequence:
 * 1. DPI awareness (fallback for old Windows)
 * 2. Config load (config.ini)
 * 3. Font extraction
 * 4. Language/startup settings
 * 
 * @note Call before CreateMainWindow
 */
BOOL InitializeApplication(HINSTANCE hInstance);

/* ============================================================================
 * Visual effects
 * ============================================================================ */

/**
 * @brief Enable/disable click-through (mouse events pass through)
 * @param enable TRUE for click-through
 * 
 * @details Adjusts WS_EX_TRANSPARENT and layered attributes
 */
void SetClickThrough(HWND hwnd, BOOL enable);

/**
 * @brief Enable/disable blur effect
 * @param enable TRUE for blur
 * 
 * @details Windows 10+ acrylic, fallback to DWM blur on older systems
 */
void SetBlurBehind(HWND hwnd, BOOL enable);

/**
 * @brief Initialize DWM functions (for blur)
 * @return TRUE if loaded
 * 
 * @details Loads dwmapi.dll dynamically
 * @note Not required on Windows 10+ (SetWindowCompositionAttribute)
 */
BOOL InitDWMFunctions(void);

/* ============================================================================
 * Window positioning and multi-monitor support
 * ============================================================================ */

/**
 * @brief Adjust position for visibility on active monitor
 * @param forceOnScreen TRUE to reposition if off-screen/inactive display
 * 
 * @details Handles:
 * - Disconnected monitor
 * - Disabled monitor ("Second screen only")
 * - Off-screen position
 * 
 * Centers on best active monitor. Saves if repositioned.
 */
void AdjustWindowPosition(HWND hwnd, BOOL forceOnScreen);

/**
 * @brief Save position and scale to config
 * 
 * @details Persists X, Y, scale to config.ini
 */
void SaveWindowSettings(HWND hwnd);

/* ============================================================================
 * Window z-order management
 * ============================================================================ */

/**
 * @brief Set always-on-top behavior
 * @param topmost TRUE for topmost, FALSE for normal (parented to Progman)
 * 
 * @details
 * Topmost: Above all non-topmost windows
 * Normal: Parented to Progman (resists Win+D minimize-all)
 * 
 * Persists to config.
 */
void SetWindowTopmost(HWND hwnd, BOOL topmost);

/**
 * @brief Attach to desktop level (above wallpaper, below all windows)
 * 
 * @details Parents to WorkerW (desktop worker)
 * @note Won't respond to Win+D or taskbar clicks
 */
void ReattachToDesktop(HWND hwnd);

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * @brief Show file selection dialog
 * @param filePath Output buffer (wide string, pre-allocated)
 * @param maxPath Buffer size (wide chars)
 * @return TRUE if selected, FALSE if cancelled
 */
BOOL OpenFileDialog(HWND hwnd, wchar_t* filePath, DWORD maxPath);

#endif // WINDOW_H
