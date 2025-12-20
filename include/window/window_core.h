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

/* Text effect type enumeration */
typedef enum {
    TEXT_EFFECT_NONE = 0,
    TEXT_EFFECT_GLOW,
    TEXT_EFFECT_GLASS,
    TEXT_EFFECT_NEON,
    TEXT_EFFECT_HOLOGRAPHIC,
    TEXT_EFFECT_LIQUID
} TextEffectType;

extern TextEffectType CLOCK_TEXT_EFFECT;

/* Legacy compatibility macros - map old BOOL checks to new enum */
#define CLOCK_GLOW_EFFECT        (CLOCK_TEXT_EFFECT == TEXT_EFFECT_GLOW)
#define CLOCK_GLASS_EFFECT       (CLOCK_TEXT_EFFECT == TEXT_EFFECT_GLASS)
#define CLOCK_NEON_EFFECT        (CLOCK_TEXT_EFFECT == TEXT_EFFECT_NEON)
#define CLOCK_HOLOGRAPHIC_EFFECT (CLOCK_TEXT_EFFECT == TEXT_EFFECT_HOLOGRAPHIC)
#define CLOCK_LIQUID_EFFECT      (CLOCK_TEXT_EFFECT == TEXT_EFFECT_LIQUID)

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

