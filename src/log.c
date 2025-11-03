/**
 * @file log.c
 * @brief Table-driven logging with rotation and crash handling
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

/** Easy to extend for future Windows versions */
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

static const CPUArchInfo CPU_ARCH_TABLE[] = {
    {PROCESSOR_ARCHITECTURE_AMD64, "x64 (AMD64)"},
    {PROCESSOR_ARCHITECTURE_INTEL, "x86 (Intel)"},
    {PROCESSOR_ARCHITECTURE_ARM,   "ARM"},
    {PROCESSOR_ARCHITECTURE_ARM64, "ARM64"},
};

static const SignalInfo SIGNAL_TABLE[] = {
    {SIGFPE,   "Floating point exception"},
    {SIGILL,   "Illegal instruction"},
    {SIGSEGV,  "Segmentation fault/memory access error"},
    {SIGTERM,  "Termination signal"},
    {SIGABRT,  "Abnormal termination/abort"},
    {SIGINT,   "User interrupt"},
};

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

/** Atomic flag prevents deadlock in crash handler */
static volatile LONG inCrashHandler = 0;

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

static const char* GetCPUArchitectureName(WORD archId) {
    const size_t tableSize = sizeof(CPU_ARCH_TABLE) / sizeof(CPU_ARCH_TABLE[0]);
    
    for (size_t i = 0; i < tableSize; i++) {
        if (CPU_ARCH_TABLE[i].archId == archId) {
            return CPU_ARCH_TABLE[i].name;
        }
    }
    
    return "Unknown architecture";
}

static const char* GetSignalDescription(int signal) {
    const size_t tableSize = sizeof(SIGNAL_TABLE) / sizeof(SIGNAL_TABLE[0]);
    
    for (size_t i = 0; i < tableSize; i++) {
        if (SIGNAL_TABLE[i].signal == signal) {
            return SIGNAL_TABLE[i].description;
        }
    }
    
    return "Unknown signal";
}

/** RtlGetVersion bypasses app manifest compatibility layer */
static BOOL GetOSVersionInfo(DWORD* major, DWORD* minor, DWORD* build) {
    if (!major || !minor || !build) {
        return FALSE;
    }
    
    *major = 0;
    *minor = 0;
    *build = 0;
    
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

static void GetLogFilePath(wchar_t* logPath, size_t size) {
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

/**
 * Lock-free crash handler prevents deadlock
 * @note Skips critical section since crash may occur while holding it
 */
static void SignalHandler(int signal) {
    if (InterlockedExchange(&inCrashHandler, 1) != 0) {
        exit(signal);
    }
    
    const char* signalDesc = GetSignalDescription(signal);
    
    /** No critical section to avoid deadlock */
    if (logFile) {
        fprintf(logFile, "[FATAL] Fatal signal occurred: %s (signal number: %d)\n", 
                signalDesc, signal);
        fflush(logFile);
        fclose(logFile);
        logFile = NULL;
    }
    
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
