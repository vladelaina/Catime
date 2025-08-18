#ifndef ASYNC_UPDATE_CHECKER_H
#define ASYNC_UPDATE_CHECKER_H

#include <windows.h>

void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck);

void CleanupUpdateThread(void);

#endif