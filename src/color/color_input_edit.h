#ifndef COLOR_INPUT_EDIT_H
#define COLOR_INPUT_EDIT_H

#include <windows.h>

#define COLOR_INPUT_EDIT_CHANGED (WM_APP + 1)

BOOL ColorInputEdit_Attach(HWND hwndEdit);
void ColorInputEdit_Detach(HWND hwndEdit);

#endif
