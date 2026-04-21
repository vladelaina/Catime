#ifndef URL_SAFETY_H
#define URL_SAFETY_H

#include <windows.h>

BOOL IsSafeOpenUrlA(const char* url);
BOOL IsSafeOpenUrlW(const wchar_t* url);

#endif
