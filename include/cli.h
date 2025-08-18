#ifndef CLI_H
#define CLI_H

#include <windows.h>

void ShowCliHelpDialog(HWND hwnd);

BOOL HandleCliArguments(HWND hwnd, const char* cmdLine);

HWND GetCliHelpDialog(void);

void CloseCliHelpDialog(void);

#endif