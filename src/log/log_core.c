/**
 * @file log_core.c
 * @brief Core logging implementation with rotation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include "log/log_core.h"
#include "log/log_system_info.h"
#include "config.h"
#include "../../resource/resource.h"

static const char* const LOG_LEVEL_STRINGS[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL"
};

static wchar_t LOG_FILE_PATH[MAX_PATH] = {0};
static HANDLE hLogFile = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION logCS;
static volatile LONG csInitialized = 0;
static LogLevel minLogLevel = LOG_LEVEL_DEBUG;

void GetLogFilePath(wchar_t* logPath, size_t size) {
    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, MAX_PATH);
    
    wchar_t configPathW[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, configPathW, MAX_PATH);
    
    wchar_t* lastSeparator = wcsrchr(configPathW, L'\\');
    if (lastSeparator) {
        size_t dirLen = lastSeparator - configPathW + 1;
        wcsncpy(logPath, configPathW, dirLen);
        _snwprintf_s(logPath + dirLen, size - dirLen, _TRUNCATE, L"Catime_Logs.log");
    } else {
        _snwprintf_s(logPath, size, _TRUNCATE, L"Catime_Logs.log");
    }
}

HANDLE GetLogFileHandle(void) {
    return hLogFile;
}

CRITICAL_SECTION* GetLogCriticalSection(void) {
    return &logCS;
}

static ULONGLONG GetLogFileSize(void) {
    if (!LOG_FILE_PATH[0]) {
        return 0;
    }
    
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExW(LOG_FILE_PATH, GetFileExInfoStandard, &fileInfo)) {
        ULARGE_INTEGER size;
        size.LowPart = fileInfo.nFileSizeLow;
        size.HighPart = fileInfo.nFileSizeHigh;
        return size.QuadPart;
    }
    
    return 0;
}

/** Open log file with shared delete permission */
static BOOL OpenLogFile(void) {
    if (hLogFile != INVALID_HANDLE_VALUE) {
        return TRUE;
    }

    hLogFile = CreateFileW(
        LOG_FILE_PATH,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hLogFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    /* Write UTF-8 BOM */
    DWORD written;
    WriteFile(hLogFile, UTF8_BOM, 3, &written, NULL);

    return TRUE;
}

/** Check if log file still exists and reopen if deleted */
static BOOL EnsureLogFileOpen(void) {
    if (hLogFile == INVALID_HANDLE_VALUE) {
        return OpenLogFile();
    }

    /* Check if file was deleted by verifying the file path still exists */
    DWORD attrs = GetFileAttributesW(LOG_FILE_PATH);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        /* File was deleted, close handle and reopen */
        CloseHandle(hLogFile);
        hLogFile = INVALID_HANDLE_VALUE;
        return OpenLogFile();
    }

    return TRUE;
}

/** Rotation: .log → .log.1 → .log.2 → .log.3 (oldest deleted) */
static void RotateLogFiles(void) {
    wchar_t oldPath[MAX_PATH];
    wchar_t newPath[MAX_PATH];
    
    _snwprintf_s(oldPath, MAX_PATH, _TRUNCATE, L"%s.%d", LOG_FILE_PATH, LOG_ROTATION_COUNT);
    DeleteFileW(oldPath);
    
    for (int i = LOG_ROTATION_COUNT - 1; i >= 1; i--) {
        _snwprintf_s(oldPath, MAX_PATH, _TRUNCATE, L"%s.%d", LOG_FILE_PATH, i);
        _snwprintf_s(newPath, MAX_PATH, _TRUNCATE, L"%s.%d", LOG_FILE_PATH, i + 1);
        MoveFileExW(oldPath, newPath, MOVEFILE_REPLACE_EXISTING);
    }
    
    _snwprintf_s(newPath, MAX_PATH, _TRUNCATE, L"%s.1", LOG_FILE_PATH);
    MoveFileExW(LOG_FILE_PATH, newPath, MOVEFILE_REPLACE_EXISTING);
}

static void CheckAndRotateLog(void) {
    ULONGLONG fileSize = GetLogFileSize();

    if (fileSize >= LOG_MAX_FILE_SIZE) {
        if (hLogFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hLogFile);
            hLogFile = INVALID_HANDLE_VALUE;
        }

        RotateLogFiles();

        OpenLogFile();
        if (hLogFile != INVALID_HANDLE_VALUE) {
            WriteLog(LOG_LEVEL_INFO, "Log file rotated (size exceeded %d MB)",
                     LOG_MAX_FILE_SIZE / (1024 * 1024));
        }
    }
}

BOOL InitializeLogSystem(void) {
    if (InterlockedCompareExchange(&csInitialized, 1, 0) == 0) {
        InitializeCriticalSection(&logCS);
    }

    GetLogFilePath(LOG_FILE_PATH, MAX_PATH);

    wchar_t dirPath[MAX_PATH] = {0};
    wcsncpy(dirPath, LOG_FILE_PATH, MAX_PATH - 1);
    wchar_t* lastSep = wcsrchr(dirPath, L'\\');
    if (lastSep) {
        *lastSep = L'\0';
        DWORD attrs = GetFileAttributesW(dirPath);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            CreateDirectoryW(dirPath, NULL);
        }
    }

    if (!OpenLogFile()) {
        return FALSE;
    }

    WriteLog(LOG_LEVEL_INFO, "==================================================");
    WriteLog(LOG_LEVEL_INFO, "Catime Version: %s", CATIME_VERSION);
    WriteLog(LOG_LEVEL_INFO, "----------------- System Information -----------------");

    LogOSVersion();
    LogCPUArchitecture();
    LogMemoryInfo();
    LogUACStatus();
    LogAdminPrivileges();

    WriteLog(LOG_LEVEL_INFO, "----------------- Application Start -----------------");
    WriteLog(LOG_LEVEL_INFO, "Log system initialized successfully");

    return TRUE;
}

void WriteLog(LogLevel level, const char* format, ...) {
    if (hLogFile == INVALID_HANDLE_VALUE) {
        return;
    }

    if (level < minLogLevel) {
        return;
    }

    EnterCriticalSection(&logCS);

    /* Check rotation every 100 entries to reduce overhead */
    static int logCounter = 0;
    if (++logCounter >= 100) {
        logCounter = 0;
        CheckAndRotateLog();
        if (hLogFile == INVALID_HANDLE_VALUE) {
            LeaveCriticalSection(&logCS);
            return;
        }
    }

    /* Check if file was deleted and reopen if needed */
    if (!EnsureLogFileOpen()) {
        LeaveCriticalSection(&logCS);
        return;
    }

    time_t now;
    struct tm local_time;
    char timeStr[32] = {0};

    time(&now);
    localtime_s(&local_time, &now);
    strftime(timeStr, sizeof(timeStr), LOG_TIMESTAMP_FORMAT, &local_time);

    /* Format log message */
    char logBuffer[4096];
    int offset = snprintf(logBuffer, sizeof(logBuffer), "[%s] [%s] ", timeStr, LOG_LEVEL_STRINGS[level]);

    va_list args;
    va_start(args, format);
    offset += vsnprintf(logBuffer + offset, sizeof(logBuffer) - offset, format, args);
    va_end(args);

    if (offset < (int)sizeof(logBuffer) - 1) {
        logBuffer[offset++] = '\n';
    }

    /* Write to file */
    DWORD written;
    WriteFile(hLogFile, logBuffer, offset, &written, NULL);
    FlushFileBuffers(hLogFile);

    LeaveCriticalSection(&logCS);
}

void CleanupLogSystem(void) {
    if (hLogFile != INVALID_HANDLE_VALUE) {
        WriteLog(LOG_LEVEL_INFO, "Catime exited normally");
        WriteLog(LOG_LEVEL_INFO, "==================================================");
        CloseHandle(hLogFile);
        hLogFile = INVALID_HANDLE_VALUE;
    }

    if (InterlockedCompareExchange(&csInitialized, 0, 1) == 1) {
        DeleteCriticalSection(&logCS);
    }
}

void SetMinimumLogLevel(LogLevel level) {
    minLogLevel = level;
}

LogLevel GetMinimumLogLevel(void) {
    return minLogLevel;
}

