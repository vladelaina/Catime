/**
 * @file window_core.h
 * @brief Core window creation and lifecycle management
 */

#ifndef WINDOW_CORE_H
#define WINDOW_CORE_H

#include <windows.h>

/* ============================================================================
 * Global window state
 * ============================================================================ */

extern int CLOCK_BASE_WINDOW_WIDTH;
extern int CLOCK_BASE_WINDOW_HEIGHT;
extern float CLOCK_WINDOW_SCALE;
extern int CLOCK_WINDOW_POS_X;
extern int CLOCK_WINDOW_POS_Y;

extern BOOL CLOCK_EDIT_MODE;
extern BOOL CLOCK_IS_DRAGGING;
extern POINT CLOCK_LAST_MOUSE_POS;
extern BOOL CLOCK_WINDOW_TOPMOST;
extern int CLOCK_WINDOW_OPACITY;

extern RECT CLOCK_TEXT_RECT;
extern BOOL CLOCK_TEXT_RECT_VALID;

extern float CLOCK_FONT_SCALE_FACTOR;
extern float PLUGIN_FONT_SCALE_FACTOR;
extern int CLOCK_BASE_FONT_SIZE;
extern BOOL CLOCK_GLOW_EFFECT;
extern BOOL CLOCK_GLASS_EFFECT;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Create main application window with layered transparency
 * @param hInstance Application instance handle
 * @param nCmdShow Window show command from WinMain
 * @return Window handle on success, NULL on failure
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow);

/**
 * @brief Persist current window position and scale to configuration file
 * @param hwnd Window handle
 */
void SaveWindowSettings(HWND hwnd);

/**
 * @brief Display file selection dialog
 * @param hwnd Parent window handle
 * @param filePath Output buffer (wide string, pre-allocated)
 * @param maxPath Buffer size (wide chars)
 * @return TRUE if selected, FALSE if cancelled
 */
BOOL OpenFileDialog(HWND hwnd, wchar_t* filePath, DWORD maxPath);

#endif /* WINDOW_CORE_H */

