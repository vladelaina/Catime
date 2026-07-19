/**
 * @file color_picker_dialog.h
 * @brief Lightweight modern HSV color picker.
 */

#ifndef COLOR_PICKER_DIALOG_H
#define COLOR_PICKER_DIALOG_H

#include <windows.h>
#include <stddef.h>

BOOL ModernColorPicker_Show(HWND hwndParent,
                            COLORREF initialColor,
                            COLORREF* customColors,
                            size_t customColorCapacity,
                            size_t* customColorCount,
                            COLORREF* selectedColor);

#endif /* COLOR_PICKER_DIALOG_H */
