#ifndef STARTUP_H
#define STARTUP_H

#include <windows.h>
#include <shlobj.h>

BOOL IsAutoStartEnabled(void);

BOOL CreateShortcut(void);

BOOL RemoveShortcut(void);

BOOL UpdateStartupShortcut(void);

#endif