/**
 * @file color_dialog.h
 * @brief Windows color picker with live preview and eyedropper
 * 
 * Provides system ChooseColor dialog with:
 * - Live preview on main window
 * - Mouse-based color sampling (eyedropper)
 * - Custom color palette persistence
 */

#ifndef COLOR_DIALOG_H
#define COLOR_DIALOG_H

#include <windows.h>

/**
 * @brief Windows color picker with live preview
 * @param hwnd Parent window handle
 * @return Selected COLORREF or -1 on cancel
 * 
 * @details
 * 16-color custom palette for quick access to frequent colors.
 * Black auto-converts to near-black (#000001) to prevent invisible text.
 * Returns -1 on cancel to distinguish from black (#000000 = 0).
 * 
 * @note Custom colors persist in session only (saved on explicit apply)
 */
COLORREF ShowColorDialog(HWND hwnd);

/**
 * @brief Hook procedure for mouse-based color sampling
 * @param hdlg Color dialog handle
 * @param msg Windows message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Hook processing result
 * 
 * @details
 * Enables "eyedropper" functionality for screen color sampling.
 * Click-to-lock prevents jitter when user finds desired color.
 * Filters dialog background (RGB(240,240,240)) to sample content only.
 * Cancel restores original color for risk-free exploration.
 * 
 * @note Uses GetPixel() with screen DC for cross-window sampling
 */
UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

#endif /* COLOR_DIALOG_H */

