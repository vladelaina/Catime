#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <windows.h>
#include <signal.h>

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} LogLevel;

BOOL InitializeLogSystem(void);

void WriteLog(LogLevel level, const char* format, ...);

void GetLastErrorDescription(DWORD errorCode, char* buffer, int bufferSize);

void SetupExceptionHandler(void);

void CleanupLogSystem(void);

#define LOG_DEBUG(format, ...) WriteLog(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) WriteLog(LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) WriteLog(LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) WriteLog(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define LOG_FATAL(format, ...) WriteLog(LOG_LEVEL_FATAL, format, ##__VA_ARGS__)

#define LOG_WINDOWS_ERROR(format, ...) do { \
    DWORD errorCode = GetLastError(); \
    char errorMessage[256] = {0}; \
    GetLastErrorDescription(errorCode, errorMessage, sizeof(errorMessage)); \
    WriteLog(LOG_LEVEL_ERROR, format " (Error code: %lu, Description: %s)", ##__VA_ARGS__, errorCode, errorMessage); \
} while(0)

#endif