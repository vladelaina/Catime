#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <windows.h>

void CheckForUpdate(HWND hwnd);

void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck);

int CompareVersions(const char* version1, const char* version2);

#endif