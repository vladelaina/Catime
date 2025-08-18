#ifndef DIALOG_LANGUAGE_H
#define DIALOG_LANGUAGE_H

#include <windows.h>

BOOL InitDialogLanguageSupport(void);

BOOL ApplyDialogLanguage(HWND hwndDlg, int dialogID);

const wchar_t* GetDialogLocalizedString(int dialogID, int controlID);

#endif