#include "log/log_core.h"
#include "log/log_system_info.h"
#include "log_file.h"
#include "config.h"
#include "log.h"
#include "../../resource/resource.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char* const LOG_LEVEL_NAMES[LOG_LEVEL_MAX] = {
    "DEBUG", "INFO", "WARNING", "ERROR", "FATAL"
};
static volatile LONG g_minimumLogLevel = LOG_LEVEL_INFO;

HANDLE GetLogFileHandle(void) {
    return LogFile_GetHandle();
}

CRITICAL_SECTION* GetLogCriticalSection(void) {
    return LogFile_GetCriticalSection();
}

BOOL InitializeLogSystem(void) {
    if (!LogFile_Initialize()) return FALSE;
    WriteLog(LOG_LEVEL_INFO, "Catime %s starting", CATIME_VERSION);
    LogOSVersion();
    LogCPUArchitecture();
    LogPackageIdentity();

    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0]) {
        WriteLog(LOG_LEVEL_INFO, "Configuration Path: %s", configPath);
    }
    LogMemoryInfo();
    LogUACStatus();
    LogAdminPrivileges();
    return TRUE;
}

static size_t FormatTimestamp(char* buffer, size_t capacity) {
    time_t now;
    struct tm localTime = {0};
    time(&now);
    if (localtime_s(&localTime, &now) != 0) {
        strcpy_s(buffer, capacity, "1970-01-01 00:00:00");
        return strlen(buffer);
    }
    size_t length = strftime(
        buffer, capacity, LOG_TIMESTAMP_FORMAT, &localTime);
    if (length == 0) {
        strcpy_s(buffer, capacity, "1970-01-01 00:00:00");
        return strlen(buffer);
    }
    return length;
}

void WriteLog(LogLevel level, const char* format, ...) {
    if (level < LOG_LEVEL_DEBUG || level >= LOG_LEVEL_MAX) {
        level = LOG_LEVEL_ERROR;
    }
    LogLevel minimumLevel = (LogLevel)InterlockedCompareExchange(
        &g_minimumLogLevel, 0, 0);
    if (level < minimumLevel) {
        return;
    }
    if (!format) format = "(null)";

    char timestamp[32];
    FormatTimestamp(timestamp, sizeof(timestamp));
    char buffer[4096];
    int prefixLength = snprintf(
        buffer, sizeof(buffer), "[%s] [%s] ",
        timestamp, LOG_LEVEL_NAMES[level]);
    if (prefixLength < 0) return;
    size_t offset = (size_t)prefixLength < sizeof(buffer)
        ? (size_t)prefixLength : sizeof(buffer) - 1;

    va_list arguments;
    va_start(arguments, format);
    int messageLength = vsnprintf(
        buffer + offset, sizeof(buffer) - offset, format, arguments);
    va_end(arguments);
    if (messageLength < 0) {
        static const char fallback[] = "[log formatting error]";
        size_t remaining = sizeof(buffer) - offset;
        int fallbackLength = snprintf(
            buffer + offset, remaining, "%s", fallback);
        if (fallbackLength > 0) {
            size_t added = (size_t)fallbackLength;
            offset += added < remaining ? added : remaining - 1;
        }
    } else if ((size_t)messageLength >= sizeof(buffer) - offset) {
        offset = sizeof(buffer) - 1;
    } else {
        offset += (size_t)messageLength;
    }

    if (offset < sizeof(buffer) - 1) {
        buffer[offset++] = '\n';
    } else {
        buffer[sizeof(buffer) - 2] = '\n';
        offset = sizeof(buffer) - 1;
    }
    LogFile_Write(level, buffer, (DWORD)offset);
}

void CleanupLogSystem(void) {
    WriteLog(LOG_LEVEL_INFO, "Catime exited normally");
    LogFile_Shutdown();
}

void SetMinimumLogLevel(LogLevel minLevel) {
    if (minLevel < LOG_LEVEL_DEBUG || minLevel >= LOG_LEVEL_MAX) {
        minLevel = LOG_LEVEL_DEBUG;
    }
    InterlockedExchange(&g_minimumLogLevel, minLevel);
}

LogLevel GetMinimumLogLevel(void) {
    return (LogLevel)InterlockedCompareExchange(
        &g_minimumLogLevel, 0, 0);
}
