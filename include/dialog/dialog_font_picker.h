/**
 * @file dialog_font_picker.h
 * @brief System font picker dialog
 */

#ifndef DIALOG_FONT_PICKER_H
#define DIALOG_FONT_PICKER_H

#include <windows.h>

/**
 * @brief Show system font picker dialog
 * @param hwndParent Parent window handle
 * @return TRUE if font was changed, FALSE if cancelled
 * 
 * @details Displays all installed TrueType fonts, allows preview
 *          by clicking, and applies selected font on OK
 */
BOOL ShowSystemFontDialog(HWND hwndParent);

/**
 * @brief Stop any pending font picker enumeration and release cached state
 *
 * @details Intended for application shutdown. Uses a finite wait so shutdown
 *          cannot hang on a slow system font enumeration.
 */
void CleanupSystemFontDialogResources(void);

#endif /* DIALOG_FONT_PICKER_H */
