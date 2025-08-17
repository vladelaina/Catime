/**
 * @file window.h
 * @brief Window management functionality interface
 * 
 * This file defines constants and function interfaces related to application window management,
 * including window creation, position adjustment, transparency, click-through, and drag functionality.
 */

#ifndef WINDOW_H
#define WINDOW_H

#include <windows.h>
#include <dwmapi.h>

/// @name Window size and position constants
/// @{
extern int CLOCK_BASE_WINDOW_WIDTH;    ///< Base window width
extern int CLOCK_BASE_WINDOW_HEIGHT;   ///< Base window height
extern float CLOCK_WINDOW_SCALE;       ///< Window scaling ratio
extern int CLOCK_WINDOW_POS_X;         ///< Window X coordinate
extern int CLOCK_WINDOW_POS_Y;         ///< Window Y coordinate
/// @}

/// @name Window state variables
/// @{
extern BOOL CLOCK_EDIT_MODE;           ///< Whether in edit mode
extern BOOL CLOCK_IS_DRAGGING;         ///< Whether currently dragging the window
extern POINT CLOCK_LAST_MOUSE_POS;     ///< Last mouse position
extern BOOL CLOCK_WINDOW_TOPMOST;       ///< Whether window is topmost
/// @}

/// @name Text area variables
/// @{
extern RECT CLOCK_TEXT_RECT;           ///< Text area rectangle
extern BOOL CLOCK_TEXT_RECT_VALID;     ///< Whether text area is valid
/// @}

/// @name Scaling limit constants
/// @{
#define MIN_SCALE_FACTOR 0.5f          ///< Minimum scaling ratio
#define MAX_SCALE_FACTOR 100.0f        ///< Maximum scaling ratio
/// @}

/// @name Font related constants
/// @{
extern float CLOCK_FONT_SCALE_FACTOR;    ///< Font scaling ratio
extern int CLOCK_BASE_FONT_SIZE;         ///< Base font size
/// @}

/**
 * @brief Set window click-through property
 * @param hwnd Window handle
 * @param enable Whether to enable click-through
 * 
 * Controls whether the window allows mouse click events to pass through to underlying windows.
 * When enabled, the window will not receive any mouse events.
 */
void SetClickThrough(HWND hwnd, BOOL enable);

/**
 * @brief Set window background blur effect
 * @param hwnd Window handle
 * @param enable Whether to enable blur effect
 * 
 * Controls whether to apply DWM blur effect to the window background,
 * making the background semi-transparent with a frosted glass effect.
 */
void SetBlurBehind(HWND hwnd, BOOL enable);

/**
 * @brief Adjust window position
 * @param hwnd Window handle
 * @param forceOnScreen Whether to force the window to stay on screen
 * 
 * When forceOnScreen is TRUE, ensures the window position is within screen boundaries,
 * automatically adjusting to a suitable position when the window position exceeds boundaries.
 * In edit mode, forceOnScreen can be set to FALSE, allowing the window to be dragged off-screen.
 */
void AdjustWindowPosition(HWND hwnd, BOOL forceOnScreen);

/**
 * @brief Save window settings
 * @param hwnd Window handle
 * 
 * Saves the window's current position, size, and scaling state to the configuration file.
 */
void SaveWindowSettings(HWND hwnd);

/**
 * @brief Load window settings
 * @param hwnd Window handle
 * 
 * Loads the window's position, size, and scaling state from the configuration file,
 * and applies them to the specified window.
 */
void LoadWindowSettings(HWND hwnd);

/**
 * @brief Initialize DWM blur functionality
 * @return BOOL Whether initialization was successful
 * 
 * Loads and initializes DWM API function pointers, used for window blur effects.
 */
BOOL InitDWMFunctions(void);

/**
 * @brief Handle window mouse wheel messages
 * @param hwnd Window handle
 * @param delta Wheel scroll amount
 * @return BOOL Whether the message was handled
 * 
 * Processes mouse wheel events, adjusting window size in edit mode.
 */
BOOL HandleMouseWheel(HWND hwnd, int delta);

/**
 * @brief Handle window mouse move messages
 * @param hwnd Window handle
 * @return BOOL Whether the message was handled
 * 
 * Processes mouse move events, dragging the window in edit mode.
 */
BOOL HandleMouseMove(HWND hwnd);

/**
 * @brief Create and initialize main window
 * @param hInstance Application instance handle
 * @param nCmdShow Display command parameter
 * @return HWND Created window handle
 * 
 * Creates the application's main window and sets initial properties.
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow);

/**
 * @brief Initialize application
 * @param hInstance Application instance handle
 * @return BOOL Whether initialization was successful
 * 
 * Performs initialization work when the application starts, including setting console code page,
 * initializing language, updating auto-start shortcuts, reading configuration files, and loading font resources.
 */
BOOL InitializeApplication(HINSTANCE hInstance);

/**
 * @brief Open file selection dialog
 * @param hwnd Parent window handle
 * @param filePath Buffer to store the selected file path
 * @param maxPath Maximum buffer length
 * @return BOOL Whether a file was successfully selected
 */
BOOL OpenFileDialog(HWND hwnd, wchar_t* filePath, DWORD maxPath);

/**
 * @brief Set window topmost state
 * @param hwnd Window handle
 * @param topmost Whether to set as topmost
 * 
 * Controls whether the window is always displayed above other windows.
 * Also updates the global state variable and saves the configuration.
 */
void SetWindowTopmost(HWND hwnd, BOOL topmost);

 /**
  * @brief Reattach window to desktop (WorkerW/Progman) in non-topmost mode
  * @param hwnd Window handle
  * 
  * Ensures the window is parented to the desktop container so it is not minimized by Win+D.
  */
 void ReattachToDesktop(HWND hwnd);

#endif // WINDOW_H
