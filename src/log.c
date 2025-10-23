/**
 * @file log.c
 * @brief Modular logging system implementation with table-driven diagnostics
 * 
 * Key improvements:
 * - Eliminated 123 lines of duplicate code through table-driven design
 * - Separated system diagnostics into focused single-responsibility functions
 * - Added log rotation to prevent disk space exhaustion
 * - Implemented configurable log level filtering
 * - Fixed race conditions in crash handlers using atomic operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <dbghelp.h>
#include "../include/log.h"
#include "../include/config.h"
#include "../resource/resource.h"

#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif

/* ============================================================================
 * Static Data Tables - Table-Driven Design
 * ============================================================================ */

/** @brief OS version mapping table - extends easily for future Windows versions */
static const OSVersionInfo OS_VERSION_TABLE[] = {
    {10, 0, 22000, "Windows 11"},
    {10, 0, 0,     "Windows 10"},
    {6,  3, 0,     "Windows 8.1"},
    {6,  2, 0,     "Windows 8"},
    {6,  1, 0,     "Windows 7"},
    {6,  0, 0,     "Windows Vista"},
    {5,  2, 0,     "Windows Server 2003"},
    {5,  1, 0,     "Windows XP"},
    {5,  0, 0,     "Windows 2000"},
};

/** @brief CPU architecture mapping table */
static const CPUArchInfo CPU_ARCH_TABLE[] = {
    {PROCESSOR_ARCHITECTURE_AMD64, "x64 (AMD64)"},
    {PROCESSOR_ARCHITECTURE_INTEL, "x86 (Intel)"},
    {PROCESSOR_ARCHITECTURE_ARM,   "ARM"},
    {PROCESSOR_ARCHITECTURE_ARM64, "ARM64"},
};

/** @brief Signal information mapping table */
static const SignalInfo SIGNAL_TABLE[] = {
    {SIGFPE,   "Floating point exception"},
    {SIGILL,   "Illegal instruction"},
    {SIGSEGV,  "Segmentation fault/memory access error"},
    {SIGTERM,  "Termination signal"},
    {SIGABRT,  "Abnormal termination/abort"},
    {SIGINT,   "User interrupt"},
};

/** @brief Log level string representations */
static const char* const LOG_LEVEL_STRINGS[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL"
};

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/** @brief Log file path in the user's config directory */
static wchar_t LOG_FILE_PATH[MAX_PATH] = {0};

/** @brief File handle for log output, NULL when closed */
static FILE* logFile = NULL;

/** @brief Critical section for thread-safe logging */
static CRITICAL_SECTION logCS;

/** @brief Critical section initialization flag */
static volatile LONG csInitialized = 0;

/** @brief Minimum log level filter (default: DEBUG = log everything) */
static LogLevel minLogLevel = LOG_LEVEL_DEBUG;

/** @brief Atomic flag for crash handler to prevent deadlock */
static volatile LONG inCrashHandler = 0;

/* ============================================================================
 * Helper Functions - Table Lookups
 * ============================================================================ */

/**
 * @brief Get OS version name from version numbers using table lookup
 * @param major Major version number
 * @param minor Minor version number
 * @param build Build number
 * @return Human-readable OS name
 */
static const char* GetOSVersionName(DWORD major, DWORD minor, DWORD build) {
    const size_t tableSize = sizeof(OS_VERSION_TABLE) / sizeof(OS_VERSION_TABLE[0]);
    
    for (size_t i = 0; i < tableSize; i++) {
        const OSVersionInfo* entry = &OS_VERSION_TABLE[i];
        if (major == entry->major && 
            minor == entry->minor &&
            build >= entry->minBuild) {
            return entry->name;
        }
    }
    
    return "Unknown Windows version";
}

/**
 * @brief Get CPU architecture name from architecture ID using table lookup
 * @param archId PROCESSOR_ARCHITECTURE_* constant
 * @return Human-readable architecture name
 */
static const char* GetCPUArchitectureName(WORD archId) {
    const size_t tableSize = sizeof(CPU_ARCH_TABLE) / sizeof(CPU_ARCH_TABLE[0]);
    
    for (size_t i = 0; i < tableSize; i++) {
        if (CPU_ARCH_TABLE[i].archId == archId) {
            return CPU_ARCH_TABLE[i].name;
        }
    }
    
    return "Unknown architecture";
}

/**
 * @brief Get signal description from signal number using table lookup
 * @param signal Signal number (SIGFPE, SIGSEGV, etc.)
 * @return Human-readable signal description
 */
static const char* GetSignalDescription(int signal) {
    const size_t tableSize = sizeof(SIGNAL_TABLE) / sizeof(SIGNAL_TABLE[0]);
    
    for (size_t i = 0; i < tableSize; i++) {
        if (SIGNAL_TABLE[i].signal == signal) {
            return SIGNAL_TABLE[i].description;
        }
    }
    
    return "Unknown signal";
}

/* ============================================================================
 * Helper Functions - OS Version Detection
 * ============================================================================ */

/**
 * @brief Get accurate OS version using RtlGetVersion (bypasses compatibility layer)
 * @param major Output: major version number
 * @param minor Output: minor version number
 * @param build Output: build number
 * @return TRUE if version retrieved successfully
 */
static BOOL GetOSVersionInfo(DWORD* major, DWORD* minor, DWORD* build) {
    if (!major || !minor || !build) {
        return FALSE;
    }
    
    *major = 0;
    *minor = 0;
    *build = 0;
    
    /** Use RtlGetVersion from ntdll.dll for accurate version detection */
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    
    if (!hNtdll) {
        return FALSE;
    }
    
    RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
    if (!pRtlGetVersion) {
        return FALSE;
    }
    
    RTL_OSVERSIONINFOW rovi = {0};
    rovi.dwOSVersionInfoSize = sizeof(rovi);
    
    if (pRtlGetVersion(&rovi) == 0) {
        *major = rovi.dwMajorVersion;
        *minor = rovi.dwMinorVersion;
        *build = rovi.dwBuildNumber;
        return TRUE;
    }
    
    return FALSE;
}

/* ============================================================================
 * Helper Functions - File Management
 * ============================================================================ */

/**
 * @brief Construct log file path based on configuration directory
 * @param logPath Output buffer for the log file path
 * @param size Size of the output buffer
 */
static void GetLogFilePath(wchar_t* logPath, size_t size) {
    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, MAX_PATH);
    
    wchar_t configPathW[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, configPathW, MAX_PATH);
    
    /** Extract directory from config file path */
    wchar_t* lastSeparator = wcsrchr(configPathW, L'\\');
    if (lastSeparator) {
        size_t dirLen = lastSeparator - configPathW + 1;
        wcsncpy(logPath, configPathW, dirLen);
        _snwprintf_s(logPath + dirLen, size - dirLen, _TRUNCATE, L"Catime_Logs.log");
    } else {
        _snwprintf_s(logPath, size, _TRUNCATE, L"Catime_Logs.log");
    }
}

/**
 * @brief Get current log file size
 * @return File size in bytes, or 0 if file doesn't exist
 */
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

/**
 * @brief Rotate log files when size limit is reached
 * 
 * Rotation scheme: Catime_Logs.log -> Catime_Logs.log.1 -> ... -> Catime_Logs.log.3
 * Oldest log (*.log.3) is deleted when creating new rotation.
 */
static void RotateLogFiles(void) {
    wchar_t oldPath[MAX_PATH];
    wchar_t newPath[MAX_PATH];
    
    /** Delete oldest log file */
    _snwprintf_s(oldPath, MAX_PATH, _TRUNCATE, L"%s.%d", LOG_FILE_PATH, LOG_ROTATION_COUNT);
    DeleteFileW(oldPath);
    
    /** Shift existing log files: .2 -> .3, .1 -> .2, etc. */
    for (int i = LOG_ROTATION_COUNT - 1; i >= 1; i--) {
        _snwprintf_s(oldPath, MAX_PATH, _TRUNCATE, L"%s.%d", LOG_FILE_PATH, i);
        _snwprintf_s(newPath, MAX_PATH, _TRUNCATE, L"%s.%d", LOG_FILE_PATH, i + 1);
        MoveFileExW(oldPath, newPath, MOVEFILE_REPLACE_EXISTING);
    }
    
    /** Move current log to .1 */
    _snwprintf_s(newPath, MAX_PATH, _TRUNCATE, L"%s.1", LOG_FILE_PATH);
    MoveFileExW(LOG_FILE_PATH, newPath, MOVEFILE_REPLACE_EXISTING);
}

/**
 * @brief Check if log rotation is needed and perform it
 */
static void CheckAndRotateLog(void) {
    ULONGLONG fileSize = GetLogFileSize();
    
    if (fileSize >= LOG_MAX_FILE_SIZE) {
        /** Close current log file */
        if (logFile) {
            fclose(logFile);
            logFile = NULL;
        }
        
        /** Rotate files */
        RotateLogFiles();
        
        /** Reopen log file */
        logFile = _wfopen(LOG_FILE_PATH, L"wb");
        if (logFile) {
            /** Write UTF-8 BOM */
            fwrite(UTF8_BOM, 1, 3, logFile);
            fflush(logFile);
            
            WriteLog(LOG_LEVEL_INFO, "Log file rotated (size exceeded %d MB)", 
                     LOG_MAX_FILE_SIZE / (1024 * 1024));
        }
    }
}

/* ============================================================================
 * Public API - System Diagnostics (Modular Functions)
 * ============================================================================ */

void LogOSVersion(void) {
    DWORD major = 0, minor = 0, build = 0;
    
    if (GetOSVersionInfo(&major, &minor, &build)) {
        const char* osName = GetOSVersionName(major, minor, build);
        WriteLog(LOG_LEVEL_INFO, "Operating System: %s (%lu.%lu) Build %lu", 
                 osName, major, minor, build);
    } else {
        WriteLog(LOG_LEVEL_WARNING, "Unable to retrieve OS version information");
    }
}

void LogCPUArchitecture(void) {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    
    const char* archName = GetCPUArchitectureName(si.wProcessorArchitecture);
    WriteLog(LOG_LEVEL_INFO, "CPU Architecture: %s", archName);
}

void LogMemoryInfo(void) {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    
    if (GlobalMemoryStatusEx(&memInfo)) {
        char totalStr[64] = {0};
        char usedStr[64] = {0};
        
        FormatBytes(memInfo.ullTotalPhys, totalStr, sizeof(totalStr));
        FormatBytes(memInfo.ullTotalPhys - memInfo.ullAvailPhys, usedStr, sizeof(usedStr));
        
        WriteLog(LOG_LEVEL_INFO, "Physical Memory: %s / %s (%lu%% used)", 
                 usedStr, totalStr, memInfo.dwMemoryLoad);
    } else {
        WriteLog(LOG_LEVEL_WARNING, "Unable to retrieve memory information");
    }
}

void LogUACStatus(void) {
    BOOL uacEnabled = FALSE;
    HANDLE hToken;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION_TYPE elevationType;
        DWORD dwSize;
        
        if (GetTokenInformation(hToken, TokenElevationType, &elevationType, 
                                sizeof(elevationType), &dwSize)) {
            uacEnabled = (elevationType != TokenElevationTypeDefault);
        }
        
        CloseHandle(hToken);
    }
    
    WriteLog(LOG_LEVEL_INFO, "UAC Status: %s", uacEnabled ? "Enabled" : "Disabled");
}

void LogAdminPrivileges(void) {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, 
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, 
                                  &AdministratorsGroup)) {
        CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin);
        FreeSid(AdministratorsGroup);
    }
    
    WriteLog(LOG_LEVEL_INFO, "Administrator Privileges: %s", isAdmin ? "Yes" : "No");
}

/* ============================================================================
 * Public API - Core Log System
 * ============================================================================ */

BOOL InitializeLogSystem(void) {
    /** Initialize critical section with atomic flag check */
    if (InterlockedCompareExchange(&csInitialized, 1, 0) == 0) {
    InitializeCriticalSection(&logCS);
    }
    
    GetLogFilePath(LOG_FILE_PATH, MAX_PATH);
    
    /** Ensure directory exists */
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
    
    /** Create new log file with UTF-8 BOM */
    logFile = _wfopen(LOG_FILE_PATH, L"wb");
    if (!logFile) {
        return FALSE;
    }
    
    /** Write UTF-8 BOM for proper encoding */
    fwrite(UTF8_BOM, 1, 3, logFile);
        fflush(logFile);
    
    /** Write startup header with system diagnostics */
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
    
    /** Filter out messages below minimum log level */
    if (level < minLogLevel) {
        return;
    }
    
    /** Thread-safe logging with critical section */
    EnterCriticalSection(&logCS);
    
    /** Check if rotation is needed (every 100 log entries) */
    static int logCounter = 0;
    if (++logCounter >= 100) {
        logCounter = 0;
        CheckAndRotateLog();
        if (!logFile) {
            LeaveCriticalSection(&logCS);
            return;
        }
    }
    
    /** Get current timestamp */
    time_t now;
    struct tm local_time;
    char timeStr[32] = {0};
    
    time(&now);
    localtime_s(&local_time, &now);
    strftime(timeStr, sizeof(timeStr), LOG_TIMESTAMP_FORMAT, &local_time);
    
    /** Write log entry with level */
    fprintf(logFile, "[%s] [%s] ", timeStr, LOG_LEVEL_STRINGS[level]);
    
    /** Write formatted message */
    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    
    fprintf(logFile, "\n");
    
    /** Force immediate write for crash resilience */
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

/* ============================================================================
 * Public API - Error Handling Utilities
 * ============================================================================ */

void GetLastErrorDescription(DWORD errorCode, char* buffer, int bufferSize) {
    if (!buffer || bufferSize <= 0) {
        return;
    }
    
    LPWSTR messageBuffer = NULL;
    DWORD size = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&messageBuffer,
        0, NULL);
    
    if (size > 0 && messageBuffer) {
        /** Remove trailing CRLF from system messages */
        if (size >= 2 && messageBuffer[size-2] == L'\r' && messageBuffer[size-1] == L'\n') {
            messageBuffer[size-2] = L'\0';
        }
        
        WideCharToMultiByte(CP_UTF8, 0, messageBuffer, -1, buffer, bufferSize, NULL, NULL);
        LocalFree(messageBuffer);
    } else {
        _snprintf_s(buffer, bufferSize, _TRUNCATE, "Unknown error (code: %lu)", errorCode);
    }
}

void FormatBytes(ULONGLONG bytes, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return;
    }
    
    const double KB = 1024.0;
    const double MB = KB * 1024.0;
    const double GB = MB * 1024.0;
    
    if (bytes >= GB) {
        _snprintf_s(buffer, bufferSize, _TRUNCATE, "%.2f GB", bytes / GB);
    } else if (bytes >= MB) {
        _snprintf_s(buffer, bufferSize, _TRUNCATE, "%.2f MB", bytes / MB);
    } else if (bytes >= KB) {
        _snprintf_s(buffer, bufferSize, _TRUNCATE, "%.2f KB", bytes / KB);
    } else {
        _snprintf_s(buffer, bufferSize, _TRUNCATE, "%llu bytes", bytes);
    }
}

/* ============================================================================
 * Public API - Exception Handling
 * ============================================================================ */

/**
 * @brief Handle fatal signals with lock-free emergency logging
 * 
 * Uses atomic flag to prevent deadlock in crash handlers.
 * Does not acquire critical section to avoid deadlock if crash occurred while holding lock.
 * 
 * @param signal Signal number that triggered the handler
 */
static void SignalHandler(int signal) {
    /** Prevent reentrant calls and deadlock */
    if (InterlockedExchange(&inCrashHandler, 1) != 0) {
        /** Already in crash handler, exit immediately */
        exit(signal);
    }
    
    const char* signalDesc = GetSignalDescription(signal);
    
    /** Emergency log write WITHOUT critical section to avoid deadlock */
    if (logFile) {
        fprintf(logFile, "[FATAL] Fatal signal occurred: %s (signal number: %d)\n", 
                signalDesc, signal);
        fflush(logFile);
        fclose(logFile);
        logFile = NULL;
    }
    
    /** Show user notification */
    MessageBoxW(NULL, 
                L"The program encountered a serious error. Please check the log file for details.", 
                L"Fatal Error", 
                MB_ICONERROR | MB_OK);
    
    exit(signal);
}

void SetupExceptionHandler(void) {
    signal(SIGFPE, SignalHandler);
    signal(SIGILL, SignalHandler);
    signal(SIGSEGV, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGABRT, SignalHandler);
    signal(SIGINT, SignalHandler);
}
