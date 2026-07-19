/**
 * @file dialog_form_layout.h
 * @brief Reusable responsive layouts for localized dialog forms.
 */

#ifndef DIALOG_FORM_LAYOUT_H
#define DIALOG_FORM_LAYOUT_H

#include <windows.h>

/** Measure and arrange a localized instruction, edit field, and action. */
void DialogFormLayout_ApplyInstruction(HWND hwndDlg, int instructionId,
                                       int editId, int actionId);

#endif /* DIALOG_FORM_LAYOUT_H */
