/**
 * @file log_core.c
 * @brief Core logging implementation with rotation
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include "log/log_core.h"
#include "log/log_system_info.h"
#include "config.h"
#include "log.h"
#include "../../resource/resource.h"

static const char* const LOG_LEVEL_STRINGS[LOG_LEVEL_MAX] = {
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
static LogLevel minLogLevel = LOG_LEVEL_INFO;

#define LOG_CS_UNINITIALIZED 0
#define LOG_CS_INITIALIZING 1
#define LOG_CS_INITIALIZED 2
#define LOG_WAIT_SPIN_LIMIT 64
#define LOG_ROTATE_CHECK_INTERVAL 100
#define LOG_EXISTENCE_CHECK_INTERVAL 64
#define LOG_FLUSH_INTERVAL 64

static void WaitWhileLogCSInitializing(void) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(&csInitialized, 0, 0) == LOG_CS_INITIALIZING) {
        Sleep(spins++ < LOG_WAIT_SPIN_LIMIT ? 0 : 1);
    }
}

static void EnsureLogCSInitialized(void) {
    if (InterlockedCompareExchange(&csInitialized,
                                   LOG_CS_INITIALIZING,
                                   LOG_CS_UNINITIALIZED) == LOG_CS_UNINITIALIZED) {
        InitializeCriticalSection(&logCS);
        InterlockedExchange(&csInitialized, LOG_CS_INITIALIZED);
        return;
    }

    WaitWhileLogCSInitializing();
}

/**
 * @brief Get log file path
 */
static BOOL GetLogFilePath(void) {
    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, MAX_PATH);
    if (configPath[0] == '\0') {
        return FALSE;
    }
    
    wchar_t configPathW[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, configPath, -1, configPathW, MAX_PATH) <= 0) {
        return FALSE;
    }
    
    const wchar_t* lastSeparator = wcsrchr(configPathW, L'\\');
    if (lastSeparator) {
        size_t dirLen = lastSeparator - configPathW + 1;
        if (dirLen >= MAX_PATH) {
            return FALSE;
        }
        wcsncpy(LOG_FILE_PATH, configPathW, dirLen);
        LOG_FILE_PATH[dirLen] = L'\0';
        if (_snwprintf_s(LOG_FILE_PATH + dirLen, MAX_PATH - dirLen,
                         _TRUNCATE, L"Catime_Logs.log") < 0) {
            LOG_FILE_PATH[0] = L'\0';
            return FALSE;
        }
    } else {
        if (_snwprintf_s(LOG_FILE_PATH, MAX_PATH, _TRUNCATE, L"Catime_Logs.log") < 0) {
            LOG_FILE_PATH[0] = L'\0';
            return FALSE;
        }
    }
    return TRUE;
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
        return ((ULONGLONG)fileInfo.nFileSizeHigh << 32) | (ULONGLONG)fileInfo.nFileSizeLow;
    }
    
    return 0;
}

/** Open log file with shared delete permission and preserve previous launches */
static BOOL OpenLogFile(void) {
    if (hLogFile != INVALID_HANDLE_VALUE) {
        return true;
    }

    hLogFile = CreateFileW(
        LOG_FILE_PATH,
        FILE_APPEND_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hLogFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER fileSize = {0};
    if (!GetFileSizeEx(hLogFile, &fileSize)) {
        CloseHandle(hLogFile);
        hLogFile = INVALID_HANDLE_VALUE;
        return false;
    }

    if (fileSize.QuadPart == 0) {
        DWORD written = 0;
        if (!WriteFile(hLogFile, UTF8_BOM, 3, &written, NULL) || written != 3) {
            CloseHandle(hLogFile);
            hLogFile = INVALID_HANDLE_VALUE;
            return false;
        }
    }

    return true;
}

/** Check if log file still exists and reopen if deleted */
static bool EnsureLogFileOpen(bool verifyPathExists) {
    if (hLogFile == INVALID_HANDLE_VALUE) {
        return OpenLogFile();
    }

    if (!verifyPathExists) {
        return true;
    }

    /* Check if file was deleted by verifying the file path still exists */
    DWORD attrs = GetFileAttributesW(LOG_FILE_PATH);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        /* File was deleted, close handle and reopen */
        CloseHandle(hLogFile);
        hLogFile = INVALID_HANDLE_VALUE;
        return OpenLogFile();
    }

    return true;
}

static BOOL FormatLogRotationPath(wchar_t* buffer, size_t bufferCount, int index) {
    if (!buffer || bufferCount == 0 || index <= 0) {
        return FALSE;
    }

    int written = _snwprintf_s(buffer, bufferCount, _TRUNCATE,
                               L"%s.%d", LOG_FILE_PATH, index);
    if (written < 0 || (size_t)written >= bufferCount) {
        buffer[0] = L'\0';
        return FALSE;
    }

    return TRUE;
}

/** Rotation: .log → .log.1 → .log.2 → .log.3 (oldest deleted) */
static BOOL RotateLogFiles(void) {
    wchar_t oldPath[MAX_PATH];
    wchar_t newPath[MAX_PATH];

    if (!FormatLogRotationPath(oldPath, MAX_PATH, LOG_ROTATION_COUNT)) {
        return FALSE;
    }
    DeleteFileW(oldPath);

    for (int i = LOG_ROTATION_COUNT - 1; i >= 1; i--) {
        if (!FormatLogRotationPath(oldPath, MAX_PATH, i) ||
            !FormatLogRotationPath(newPath, MAX_PATH, i + 1)) {
            return FALSE;
        }
        MoveFileExW(oldPath, newPath, MOVEFILE_REPLACE_EXISTING);
    }

    if (!FormatLogRotationPath(newPath, MAX_PATH, 1)) {
        return FALSE;
    }
    MoveFileExW(LOG_FILE_PATH, newPath, MOVEFILE_REPLACE_EXISTING);
    return TRUE;
}

static void CheckAndRotateLog(void) {
    ULONGLONG fileSize = GetLogFileSize();

    if (fileSize < LOG_MAX_FILE_SIZE) {
        return;
    }
    
    if (hLogFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hLogFile);
        hLogFile = INVALID_HANDLE_VALUE;
    }

    BOOL rotated = RotateLogFiles();

    OpenLogFile();
    if (hLogFile != INVALID_HANDLE_VALUE) {
        if (rotated) {
            WriteLog(LOG_LEVEL_INFO, "Log file rotated (size exceeded %d MB)",
                        LOG_MAX_FILE_SIZE / (1024 * 1024));
        } else {
            WriteLog(LOG_LEVEL_WARNING,
                     "Log rotation path too long; started a fresh log file");
        }
    }
}

BOOL InitializeLogSystem(void) {
    EnsureLogCSInitialized();

    if (!GetLogFilePath()) {
        return FALSE;
    }

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
    if (InterlockedCompareExchange(&csInitialized, 0, 0) != LOG_CS_INITIALIZED ||
        LOG_FILE_PATH[0] == L'\0') {
        return;
    }

    if (level < LOG_LEVEL_DEBUG || level >= LOG_LEVEL_MAX) {
        level = LOG_LEVEL_ERROR;
    }

    if (level < minLogLevel) {
        return;
    }

    if (!format) {
        format = "(null)";
    }

    EnterCriticalSection(&logCS);
    if (hLogFile == INVALID_HANDLE_VALUE) {
        if (!EnsureLogFileOpen(false)) {
            LeaveCriticalSection(&logCS);
            return;
        }
    }

    /* Check rotation periodically to reduce filesystem metadata calls */
    static int logCounter = 0;
    if (++logCounter >= LOG_ROTATE_CHECK_INTERVAL) {
        logCounter = 0;
        CheckAndRotateLog();
        if (hLogFile == INVALID_HANDLE_VALUE) {
            LeaveCriticalSection(&logCS);
            return;
        }
    }

    /* Check if file was deleted and reopen if needed without doing path IO per line */
    static int existenceCheckCounter = 0;
    bool verifyPathExists = (++existenceCheckCounter >= LOG_EXISTENCE_CHECK_INTERVAL);
    if (verifyPathExists) {
        existenceCheckCounter = 0;
    }
    if (!EnsureLogFileOpen(verifyPathExists)) {
        LeaveCriticalSection(&logCS);
        return;
    }

    time_t now;
    struct tm local_time = {0};
    char timeStr[32] = {0};

    time(&now);
    if (localtime_s(&local_time, &now) == 0) {
        strftime(timeStr, sizeof(timeStr), LOG_TIMESTAMP_FORMAT, &local_time);
    } else {
        strcpy_s(timeStr, sizeof(timeStr), "1970-01-01 00:00:00");
    }

    /* Format log message */
    char logBuffer[4096];
    int prefixLength = snprintf(logBuffer, sizeof(logBuffer), "[%s] [%s] ", timeStr, LOG_LEVEL_STRINGS[level]);
    size_t offset = 0;
    if (prefixLength < 0) {
        LeaveCriticalSection(&logCS);
        return;
    }
    if ((size_t)prefixLength >= sizeof(logBuffer)) {
        offset = sizeof(logBuffer) - 1;
    } else {
        offset = (size_t)prefixLength;
    }

    va_list args;
    va_start(args, format);
    int messageLength = vsnprintf(logBuffer + offset, sizeof(logBuffer) - offset, format, args);
    va_end(args);

    if (messageLength < 0) {
        const char fallback[] = "[log formatting error]";
        size_t remaining = sizeof(logBuffer) - offset;
        if (remaining > 0) {
            int fallbackLength = snprintf(logBuffer + offset, remaining, "%s", fallback);
            if (fallbackLength > 0) {
                offset += (size_t)fallbackLength < remaining ? (size_t)fallbackLength : remaining - 1;
            }
        }
    } else if ((size_t)messageLength >= sizeof(logBuffer) - offset) {
        offset = sizeof(logBuffer) - 1;
    } else {
        offset += (size_t)messageLength;
    }

    if (offset < sizeof(logBuffer) - 1) {
        logBuffer[offset++] = '\n';
    } else {
        logBuffer[sizeof(logBuffer) - 2] = '\n';
        offset = sizeof(logBuffer) - 1;
    }

    /* Write to file */
    DWORD written = 0;
    BOOL writeOk = WriteFile(hLogFile, logBuffer, (DWORD)offset, &written, NULL) &&
                   written == (DWORD)offset;

    /* Avoid per-line disk flush; flush periodically and on high-severity entries. */
    static int flushCounter = 0;
    if (writeOk && (level >= LOG_LEVEL_ERROR || ++flushCounter >= LOG_FLUSH_INTERVAL)) {
        if (!FlushFileBuffers(hLogFile)) {
            writeOk = FALSE;
        }
        flushCounter = 0;
    }

    if (!writeOk) {
        CloseHandle(hLogFile);
        hLogFile = INVALID_HANDLE_VALUE;
    }

    LeaveCriticalSection(&logCS);
}

void CleanupLogSystem(void) {
    WaitWhileLogCSInitializing();

    if (InterlockedCompareExchange(&csInitialized, 0, 0) == LOG_CS_INITIALIZED) {
        WriteLog(LOG_LEVEL_INFO, "Catime exited normally");
        WriteLog(LOG_LEVEL_INFO, "==================================================");

        EnterCriticalSection(&logCS);
        if (hLogFile != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(hLogFile);
            CloseHandle(hLogFile);
            hLogFile = INVALID_HANDLE_VALUE;
        }
        LeaveCriticalSection(&logCS);
    }
}

void SetMinimumLogLevel(LogLevel minLevel) {
    if (minLevel < 0 || minLevel >= LOG_LEVEL_MAX) {
        minLevel = LOG_LEVEL_DEBUG;
    }
    minLogLevel = minLevel;
}

LogLevel GetMinimumLogLevel(void) {
    return minLogLevel;
}

