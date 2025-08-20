/**
 * @file log.c
 * @brief Centralized logging system with system diagnostics and crash handling
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <dbghelp.h>
#include <wininet.h>
#include "../include/log.h"
#include "../include/config.h"
#include "../resource/resource.h"

#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif

/** @brief Log file path in the user's config directory */
static wchar_t LOG_FILE_PATH[MAX_PATH] = {0};
/** @brief File handle for log output, NULL when closed */
static FILE* logFile = NULL;
/** @brief Critical section for thread-safe logging */
static CRITICAL_SECTION logCS;

/** @brief String representations for log levels in output */
static const char* LOG_LEVEL_STRINGS[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL"
};

/**
 * @brief Log comprehensive system information for diagnostics
 * Uses undocumented RtlGetVersion to bypass version lie compatibility
 */
static void LogSystemInformation(void) {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    
    /** Use RtlGetVersion to bypass version compatibility layer */
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    
    DWORD major = 0, minor = 0, build = 0;
    BOOL isWorkstation = TRUE;
    BOOL isServer = FALSE;
    
    /** Primary method: RtlGetVersion from ntdll (most reliable) */
    if (hNtdll) {
        RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
        if (pRtlGetVersion) {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (pRtlGetVersion(&rovi) == 0) {
                major = rovi.dwMajorVersion;
                minor = rovi.dwMinorVersion;
                build = rovi.dwBuildNumber;
            }
        }
    }
    
    /** Fallback methods for version detection */
    if (major == 0) {
        OSVERSIONINFOEXA osvi;
        ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
        
        typedef LONG (WINAPI* PRTLGETVERSION)(OSVERSIONINFOEXW*);
        PRTLGETVERSION pRtlGetVersion;
        pRtlGetVersion = (PRTLGETVERSION)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
        
        if (pRtlGetVersion) {
            pRtlGetVersion((OSVERSIONINFOEXW*)&osvi);
            major = osvi.dwMajorVersion;
            minor = osvi.dwMinorVersion;
            build = osvi.dwBuildNumber;
            isWorkstation = (osvi.wProductType == VER_NT_WORKSTATION);
            isServer = !isWorkstation;
        } else {
            /** Last resort: deprecated GetVersionEx */
            if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
                major = osvi.dwMajorVersion;
                minor = osvi.dwMinorVersion;
                build = osvi.dwBuildNumber;
                isWorkstation = (osvi.wProductType == VER_NT_WORKSTATION);
                isServer = !isWorkstation;
            } else {
                WriteLog(LOG_LEVEL_WARNING, "Unable to get operating system version information");
            }
        }
    }
    
    /** Map version numbers to Windows edition names */
    const char* windowsVersion = "Unknown version";
    
    if (major == 10) {
        /** Windows 11 uses build 22000+ threshold */
        if (build >= 22000) {
            windowsVersion = "Windows 11";
        } else {
            windowsVersion = "Windows 10";
        }
    } else if (major == 6) {
        if (minor == 3) {
            windowsVersion = "Windows 8.1";
        } else if (minor == 2) {
            windowsVersion = "Windows 8";
        } else if (minor == 1) {
            windowsVersion = "Windows 7";
        } else if (minor == 0) {
            windowsVersion = "Windows Vista";
        }
    } else if (major == 5) {
        if (minor == 2) {
            windowsVersion = "Windows Server 2003";
            /** Special case: XP Professional x64 Edition */
            if (isWorkstation && si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
                windowsVersion = "Windows XP Professional x64";
            }
        } else if (minor == 1) {
            windowsVersion = "Windows XP";
        } else if (minor == 0) {
            windowsVersion = "Windows 2000";
        }
    }
    
    WriteLog(LOG_LEVEL_INFO, "Operating System: %s (%d.%d) Build %d %s", 
        windowsVersion,
        major, minor, 
        build, 
        isWorkstation ? "Workstation" : "Server");
    
    /** Log CPU architecture for compatibility diagnostics */
    const char* arch;
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            arch = "x64 (AMD64)";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            arch = "x86 (Intel)";
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            arch = "ARM";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            arch = "ARM64";
            break;
        default:
            arch = "Unknown";
            break;
    }
    WriteLog(LOG_LEVEL_INFO, "CPU Architecture: %s", arch);
    
    /** Log memory usage for performance troubleshooting */
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        WriteLog(LOG_LEVEL_INFO, "Physical Memory: %.2f GB / %.2f GB (%d%% used)", 
            (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024.0 * 1024 * 1024), 
            memInfo.ullTotalPhys / (1024.0 * 1024 * 1024),
            memInfo.dwMemoryLoad);
    }
    
    /** Check UAC status for security context awareness */
    BOOL uacEnabled = FALSE;
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION_TYPE elevationType;
        DWORD dwSize;
        if (GetTokenInformation(hToken, TokenElevationType, &elevationType, sizeof(elevationType), &dwSize)) {
            uacEnabled = (elevationType != TokenElevationTypeDefault);
        }
        CloseHandle(hToken);
    }
    WriteLog(LOG_LEVEL_INFO, "UAC Status: %s", uacEnabled ? "Enabled" : "Disabled");
    
    /** Check administrator privileges for permission diagnostics */
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup)) {
        if (CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin)) {
            WriteLog(LOG_LEVEL_INFO, "Administrator Privileges: %s", isAdmin ? "Yes" : "No");
        }
        FreeSid(AdministratorsGroup);
    }
}

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
 * @brief Initialize logging system and write startup diagnostics
 * @return TRUE on success, FALSE if log file creation failed
 */
BOOL InitializeLogSystem(void) {
    InitializeCriticalSection(&logCS);
    
    GetLogFilePath(LOG_FILE_PATH, MAX_PATH);
    
    /** Create new log file, truncating any existing content */
    logFile = _wfopen(LOG_FILE_PATH, L"w");
    if (!logFile) {
        return FALSE;
    }
    
    WriteLog(LOG_LEVEL_INFO, "==================================================");
    WriteLog(LOG_LEVEL_INFO, "Catime Version: %s", CATIME_VERSION);
    WriteLog(LOG_LEVEL_INFO, "-----------------System Information-----------------");
    LogSystemInformation();
    WriteLog(LOG_LEVEL_INFO, "-----------------Application Information-----------------");
    WriteLog(LOG_LEVEL_INFO, "Log system initialization complete, Catime started");
    
    return TRUE;
}

/**
 * @brief Thread-safe logging with timestamp and level formatting
 * @param level Log severity level
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 */
void WriteLog(LogLevel level, const char* format, ...) {
    if (!logFile) {
        return;
    }
    
    EnterCriticalSection(&logCS);
    
    time_t now;
    struct tm local_time;
    char timeStr[32] = {0};
    
    time(&now);
    localtime_s(&local_time, &now);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &local_time);
    
    fprintf(logFile, "[%s] [%s] ", timeStr, LOG_LEVEL_STRINGS[level]);
    
    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    
    fprintf(logFile, "\n");
    
    /** Force immediate write for crash resilience */
    fflush(logFile);
    
    LeaveCriticalSection(&logCS);
}

/**
 * @brief Convert Windows error code to human-readable UTF-8 description
 * @param errorCode Windows API error code from GetLastError()
 * @param buffer Output buffer for error description
 * @param bufferSize Size of the output buffer
 */
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
    
    if (size > 0) {
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

/**
 * @brief Handle fatal signals with emergency logging and user notification
 * @param signal Signal number that triggered the handler
 * Ensures log is flushed before termination for crash analysis
 */
void SignalHandler(int signal) {
    char errorMsg[256] = {0};
    
    /** Map signal numbers to descriptive error messages */
    switch (signal) {
        case SIGFPE:
            strcpy_s(errorMsg, sizeof(errorMsg), "Floating point exception");
            break;
        case SIGILL:
            strcpy_s(errorMsg, sizeof(errorMsg), "Illegal instruction");
            break;
        case SIGSEGV:
            strcpy_s(errorMsg, sizeof(errorMsg), "Segmentation fault/memory access error");
            break;
        case SIGTERM:
            strcpy_s(errorMsg, sizeof(errorMsg), "Termination signal");
            break;
        case SIGABRT:
            strcpy_s(errorMsg, sizeof(errorMsg), "Abnormal termination/abort");
            break;
        case SIGINT:
            strcpy_s(errorMsg, sizeof(errorMsg), "User interrupt");
            break;
        default:
            strcpy_s(errorMsg, sizeof(errorMsg), "Unknown signal");
            break;
    }
    
    /** Emergency log write without critical section to avoid deadlock */
    if (logFile) {
        fprintf(logFile, "[FATAL] Fatal signal occurred: %s (signal number: %d)\n", 
                errorMsg, signal);
        fflush(logFile);
        
        fclose(logFile);
        logFile = NULL;
    }
    
    MessageBoxW(NULL, L"The program encountered a serious error. Please check the log file for detailed information.", L"Fatal Error", MB_ICONERROR | MB_OK);
    
    exit(signal);
}

/**
 * @brief Register signal handlers for crash detection and logging
 * Covers arithmetic, memory, and termination signals
 */
void SetupExceptionHandler(void) {
    signal(SIGFPE, SignalHandler);
    signal(SIGILL, SignalHandler);
    signal(SIGSEGV, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGABRT, SignalHandler);
    signal(SIGINT, SignalHandler);
}

/**
 * @brief Clean shutdown of logging system with final log entry
 * Should be called during normal application termination
 */
void CleanupLogSystem(void) {
    if (logFile) {
        WriteLog(LOG_LEVEL_INFO, "Catime exited normally");
        WriteLog(LOG_LEVEL_INFO, "==================================================");
        fclose(logFile);
        logFile = NULL;
    }
    
    DeleteCriticalSection(&logCS);
}