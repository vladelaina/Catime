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
static FILE* logFile = NULL;
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

FILE* GetLogFileHandle(void) {
    return logFile;
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
        if (logFile) {
            fclose(logFile);
            logFile = NULL;
        }
        
        RotateLogFiles();
        
        logFile = _wfopen(LOG_FILE_PATH, L"wb");
        if (logFile) {
            fwrite(UTF8_BOM, 1, 3, logFile);
            fflush(logFile);
            
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
    
    logFile = _wfopen(LOG_FILE_PATH, L"wb");
    if (!logFile) {
        return FALSE;
    }
    
    fwrite(UTF8_BOM, 1, 3, logFile);
    fflush(logFile);
    
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
    if (!logFile) {
        return;
    }
    
    if (level < minLogLevel) {
        return;
    }
    
    EnterCriticalSection(&logCS);
    
    /** Check rotation every 100 entries to reduce overhead */
    static int logCounter = 0;
    if (++logCounter >= 100) {
        logCounter = 0;
        CheckAndRotateLog();
        if (!logFile) {
            LeaveCriticalSection(&logCS);
            return;
        }
    }
    
    time_t now;
    struct tm local_time;
    char timeStr[32] = {0};
    
    time(&now);
    localtime_s(&local_time, &now);
    strftime(timeStr, sizeof(timeStr), LOG_TIMESTAMP_FORMAT, &local_time);
    
    fprintf(logFile, "[%s] [%s] ", timeStr, LOG_LEVEL_STRINGS[level]);
    
    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    
    fprintf(logFile, "\n");
    
    /** fflush ensures crashes don't lose recent logs */
    fflush(logFile);
    
    LeaveCriticalSection(&logCS);
}

void CleanupLogSystem(void) {
    if (logFile) {
        WriteLog(LOG_LEVEL_INFO, "Catime exited normally");
        WriteLog(LOG_LEVEL_INFO, "==================================================");
        fclose(logFile);
        logFile = NULL;
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

