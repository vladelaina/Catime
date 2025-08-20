/**
 * @file window.h
 * @brief Main window management and properties
 * 
 * Functions for window creation, positioning, scaling, and visual effects
 */

#ifndef WINDOW_H
#define WINDOW_H

#include <windows.h>
#include <dwmapi.h>

/** @brief Base window width in pixels */
extern int CLOCK_BASE_WINDOW_WIDTH;

/** @brief Base window height in pixels */
extern int CLOCK_BASE_WINDOW_HEIGHT;

/** @brief Current window scale factor */
extern float CLOCK_WINDOW_SCALE;

/** @brief Window X position on screen */
extern int CLOCK_WINDOW_POS_X;

/** @brief Window Y position on screen */
extern int CLOCK_WINDOW_POS_Y;

/** @brief Edit mode active state */
extern BOOL CLOCK_EDIT_MODE;

/** @brief Window currently being dragged */
extern BOOL CLOCK_IS_DRAGGING;

/** @brief Last recorded mouse position */
extern POINT CLOCK_LAST_MOUSE_POS;

/** @brief Window always-on-top state */
extern BOOL CLOCK_WINDOW_TOPMOST;

/** @brief Text bounding rectangle */
extern RECT CLOCK_TEXT_RECT;

/** @brief Text rectangle validity flag */
extern BOOL CLOCK_TEXT_RECT_VALID;

/** @brief Minimum allowed scale factor */
#define MIN_SCALE_FACTOR 0.5f

/** @brief Maximum allowed scale factor */
#define MAX_SCALE_FACTOR 100.0f

/** @brief Current font scale factor */
extern float CLOCK_FONT_SCALE_FACTOR;

/** @brief Base font size in points */
extern int CLOCK_BASE_FONT_SIZE;

/**
 * @brief Enable or disable window click-through behavior
 * @param hwnd Window handle
 * @param enable TRUE to enable click-through, FALSE to disable
 */
void SetClickThrough(HWND hwnd, BOOL enable);

/**
 * @brief Enable or disable window blur-behind effect
 * @param hwnd Window handle
 * @param enable TRUE to enable blur effect, FALSE to disable
 */
void SetBlurBehind(HWND hwnd, BOOL enable);

/**
 * @brief Adjust window position to stay within screen bounds
 * @param hwnd Window handle
 * @param forceOnScreen TRUE to force window completely on screen
 */
void AdjustWindowPosition(HWND hwnd, BOOL forceOnScreen);

/**
 * @brief Save current window settings to configuration
 * @param hwnd Window handle
 */
void SaveWindowSettings(HWND hwnd);

/**
 * @brief Load window settings from configuration
 * @param hwnd Window handle
 */
void LoadWindowSettings(HWND hwnd);

/**
 * @brief Initialize Desktop Window Manager functions
 * @return TRUE on success, FALSE on failure
 */
BOOL InitDWMFunctions(void);

/**
 * @brief Handle mouse wheel events for window scaling
 * @param hwnd Window handle
 * @param delta Mouse wheel delta value
 * @return TRUE if handled, FALSE otherwise
 */
BOOL HandleMouseWheel(HWND hwnd, int delta);

/**
 * @brief Handle mouse movement for window dragging
 * @param hwnd Window handle
 * @return TRUE if handled, FALSE otherwise
 */
BOOL HandleMouseMove(HWND hwnd);

/**
 * @brief Create and configure main application window
 * @param hInstance Application instance handle
 * @param nCmdShow Initial window show state
 * @return Window handle or NULL on failure
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow);

/**
 * @brief Initialize application components
 * @param hInstance Application instance handle
 * @return TRUE on success, FALSE on failure
 */
BOOL InitializeApplication(HINSTANCE hInstance);

/**
 * @brief Show file selection dialog
 * @param hwnd Parent window handle
 * @param filePath Buffer to store selected file path
 * @param maxPath Maximum path buffer size
 * @return TRUE if file selected, FALSE if cancelled
 */
BOOL OpenFileDialog(HWND hwnd, wchar_t* filePath, DWORD maxPath);

/**
 * @brief Set window always-on-top state
 * @param hwnd Window handle
 * @param topmost TRUE for topmost, FALSE for normal
 */
void SetWindowTopmost(HWND hwnd, BOOL topmost);

/**
 * @brief Reattach window to desktop layer
 * @param hwnd Window handle
 */
void ReattachToDesktop(HWND hwnd);

#endif