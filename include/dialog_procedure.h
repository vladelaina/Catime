#ifndef DIALOG_PROCEDURE_H
#define DIALOG_PROCEDURE_H

#include <windows.h>

INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

void ShowAboutDialog(HWND hwndParent);

INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

void ShowErrorDialog(HWND hwndParent);

void ShowPomodoroLoopDialog(HWND hwndParent);

INT_PTR CALLBACK PomodoroLoopDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

void ShowWebsiteDialog(HWND hwndParent);

void ShowPomodoroComboDialog(HWND hwndParent);

BOOL ParseTimeInput(const char* input, int* seconds);

void ShowNotificationMessagesDialog(HWND hwndParent);

void ShowNotificationDisplayDialog(HWND hwndParent);

void ShowNotificationSettingsDialog(HWND hwndParent);

extern HWND g_hwndInputDialog;

#endif