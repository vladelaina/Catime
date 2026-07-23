#include "log_file.h"
#include "config.h"

#include <stdbool.h>
#include <wchar.h>

#define LOG_CS_UNINITIALIZED 0
#define LOG_CS_INITIALIZING 1
#define LOG_CS_INITIALIZED 2
#define LOG_WAIT_SPIN_LIMIT 64
#define LOG_ROTATE_CHECK_INTERVAL 100
#define LOG_EXISTENCE_CHECK_INTERVAL 64
#define LOG_FLUSH_INTERVAL 64

static wchar_t g_logPath[MAX_PATH];
static HANDLE g_logFile = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_logCriticalSection;
static volatile LONG g_logCriticalSectionState = LOG_CS_UNINITIALIZED;
static int g_rotateCheckCounter;
static int g_existenceCheckCounter;
static int g_flushCounter;

static void WaitForCriticalSection(void) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(
               &g_logCriticalSectionState, 0, 0) == LOG_CS_INITIALIZING) {
        Sleep(spins++ < LOG_WAIT_SPIN_LIMIT ? 0 : 1);
    }
}

static void EnsureCriticalSection(void) {
    if (InterlockedCompareExchange(
            &g_logCriticalSectionState, LOG_CS_INITIALIZING,
            LOG_CS_UNINITIALIZED) == LOG_CS_UNINITIALIZED) {
        InitializeCriticalSection(&g_logCriticalSection);
        InterlockedExchange(&g_logCriticalSectionState, LOG_CS_INITIALIZED);
    }
    WaitForCriticalSection();
}

static BOOL ResolveLogPath(void) {
    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, MAX_PATH);
    wchar_t widePath[MAX_PATH] = {0};
    if (!configPath[0] ||
        MultiByteToWideChar(CP_UTF8, 0, configPath, -1,
                            widePath, MAX_PATH) <= 0) {
        return FALSE;
    }

    const wchar_t* separator = wcsrchr(widePath, L'\\');
    size_t directoryLength = separator
        ? (size_t)(separator - widePath + 1) : 0;
    if (directoryLength >= MAX_PATH) return FALSE;
    if (directoryLength > 0) {
        wcsncpy(g_logPath, widePath, directoryLength);
        g_logPath[directoryLength] = L'\0';
    }
    if (_snwprintf_s(
            g_logPath + directoryLength, MAX_PATH - directoryLength,
            _TRUNCATE, L"Catime_Logs.log") < 0) {
        g_logPath[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL OpenLogFile(void) {
    if (g_logFile != INVALID_HANDLE_VALUE) return TRUE;
    g_logFile = CreateFileW(
        g_logPath, FILE_APPEND_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_logFile == INVALID_HANDLE_VALUE) return FALSE;

    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(g_logFile, &size)) {
        CloseHandle(g_logFile);
        g_logFile = INVALID_HANDLE_VALUE;
        return FALSE;
    }
    if (size.QuadPart == 0) {
        DWORD written = 0;
        if (!WriteFile(g_logFile, UTF8_BOM, 3, &written, NULL) ||
            written != 3) {
            CloseHandle(g_logFile);
            g_logFile = INVALID_HANDLE_VALUE;
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL EnsureOpen(BOOL verifyPath) {
    if (g_logFile == INVALID_HANDLE_VALUE) return OpenLogFile();
    if (!verifyPath || GetFileAttributesW(g_logPath) != INVALID_FILE_ATTRIBUTES) {
        return TRUE;
    }
    CloseHandle(g_logFile);
    g_logFile = INVALID_HANDLE_VALUE;
    return OpenLogFile();
}

static BOOL RotationPath(wchar_t* path, size_t count, int index) {
    return path && count > 0 && index > 0 &&
           _snwprintf_s(path, count, _TRUNCATE,
                        L"%s.%d", g_logPath, index) >= 0;
}

static void RotateIfNeeded(void) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (!GetFileAttributesExW(
            g_logPath, GetFileExInfoStandard, &info)) return;
    ULONGLONG size = ((ULONGLONG)info.nFileSizeHigh << 32) |
                     info.nFileSizeLow;
    if (size < LOG_MAX_FILE_SIZE) return;

    if (g_logFile != INVALID_HANDLE_VALUE) {
        CloseHandle(g_logFile);
        g_logFile = INVALID_HANDLE_VALUE;
    }
    wchar_t oldPath[MAX_PATH];
    wchar_t newPath[MAX_PATH];
    if (RotationPath(oldPath, MAX_PATH, LOG_ROTATION_COUNT)) {
        DeleteFileW(oldPath);
    }
    for (int i = LOG_ROTATION_COUNT - 1; i >= 1; i--) {
        if (RotationPath(oldPath, MAX_PATH, i) &&
            RotationPath(newPath, MAX_PATH, i + 1)) {
            MoveFileExW(oldPath, newPath, MOVEFILE_REPLACE_EXISTING);
        }
    }
    if (RotationPath(newPath, MAX_PATH, 1)) {
        MoveFileExW(g_logPath, newPath, MOVEFILE_REPLACE_EXISTING);
    }
    OpenLogFile();
}

BOOL LogFile_Initialize(void) {
    EnsureCriticalSection();
    if (!ResolveLogPath()) return FALSE;
    wchar_t directory[MAX_PATH];
    wcscpy_s(directory, MAX_PATH, g_logPath);
    wchar_t* separator = wcsrchr(directory, L'\\');
    if (separator) {
        *separator = L'\0';
        if (GetFileAttributesW(directory) == INVALID_FILE_ATTRIBUTES) {
            CreateDirectoryW(directory, NULL);
        }
    }
    return OpenLogFile();
}

BOOL LogFile_Write(LogLevel level, const char* data, DWORD length) {
    if (!data || length == 0 ||
        InterlockedCompareExchange(
            &g_logCriticalSectionState, 0, 0) != LOG_CS_INITIALIZED ||
        !g_logPath[0]) {
        return FALSE;
    }
    EnterCriticalSection(&g_logCriticalSection);
    if (++g_rotateCheckCounter >= LOG_ROTATE_CHECK_INTERVAL) {
        g_rotateCheckCounter = 0;
        RotateIfNeeded();
    }
    BOOL verify = ++g_existenceCheckCounter >=
                  LOG_EXISTENCE_CHECK_INTERVAL;
    if (verify) g_existenceCheckCounter = 0;
    BOOL success = EnsureOpen(verify);
    DWORD written = 0;
    if (success) {
        success = WriteFile(g_logFile, data, length, &written, NULL) &&
                  written == length;
    }
    if (success &&
        (level >= LOG_LEVEL_ERROR || ++g_flushCounter >= LOG_FLUSH_INTERVAL)) {
        success = FlushFileBuffers(g_logFile);
        g_flushCounter = 0;
    }
    if (!success && g_logFile != INVALID_HANDLE_VALUE) {
        CloseHandle(g_logFile);
        g_logFile = INVALID_HANDLE_VALUE;
    }
    LeaveCriticalSection(&g_logCriticalSection);
    return success;
}

void LogFile_Shutdown(void) {
    WaitForCriticalSection();
    if (InterlockedCompareExchange(
            &g_logCriticalSectionState, 0, 0) != LOG_CS_INITIALIZED) return;
    EnterCriticalSection(&g_logCriticalSection);
    if (g_logFile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(g_logFile);
        CloseHandle(g_logFile);
        g_logFile = INVALID_HANDLE_VALUE;
    }
    LeaveCriticalSection(&g_logCriticalSection);
}

HANDLE LogFile_GetHandle(void) {
    return g_logFile;
}

CRITICAL_SECTION* LogFile_GetCriticalSection(void) {
    return &g_logCriticalSection;
}
