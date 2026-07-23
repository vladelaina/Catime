#ifndef LOG_FILE_H
#define LOG_FILE_H

#include "log.h"

BOOL LogFile_Initialize(void);
BOOL LogFile_Write(LogLevel level, const char* data, DWORD length);
void LogFile_Shutdown(void);
HANDLE LogFile_GetHandle(void);
CRITICAL_SECTION* LogFile_GetCriticalSection(void);

#endif
