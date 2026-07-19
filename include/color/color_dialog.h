/**
 * @file color_dialog.h
 * @brief Lightweight modern color picker with live preview and eyedropper
 * 
 * Provides a custom Win32/GDI dialog with:
 * - Live preview on main window
 * - Mouse-based color sampling (eyedropper)
 * - Custom color palette persistence
 */

#ifndef COLOR_DIALOG_H
#define COLOR_DIALOG_H

#include <windows.h>

/**
 * @brief Show the modern color picker with live preview
 * @param hwnd Parent window handle
 * @return Selected COLORREF or -1 on cancel
 * 
 * @details
 * 16-color custom palette for quick access to frequent colors.
 * Black auto-converts to near-black (#000001) to prevent invisible text.
 * Returns -1 on cancel to distinguish from black (#000000 = 0).
 * 
 * @note Custom colors are persisted when the selection is accepted
 */
COLORREF ShowColorDialog(HWND hwnd);

#endif /* COLOR_DIALOG_H */

