/**
 * @file log.c
 * @brief Log recording functionality implementation
 * 
 * Implements logging functionality, including file writing, error code retrieval, etc.
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

// Add check for ARM64 macro
#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif

// Log file path
static char LOG_FILE_PATH[MAX_PATH] = {0};
static FILE* logFile = NULL;
static CRITICAL_SECTION logCS;

// Log level string representations
static const char* LOG_LEVEL_STRINGS[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL"
};

/**
 * @brief Get operating system version information
 * 
 * Use Windows API to get operating system version, version number, build and other information
 */
static void LogSystemInformation(void) {
    // Get system information
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    
    // Use RtlGetVersion to get system version more accurately, because GetVersionEx was changed in newer Windows versions
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    
    DWORD major = 0, minor = 0, build = 0;
    BOOL isWorkstation = TRUE;
    BOOL isServer = FALSE;
    
    if (hNtdll) {
        RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
        if (pRtlGetVersion) {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (pRtlGetVersion(&rovi) == 0) { // STATUS_SUCCESS = 0
                major = rovi.dwMajorVersion;
                minor = rovi.dwMinorVersion;
                build = rovi.dwBuildNumber;
            }
        }
    }
    
    // If the above method fails, try the method below
    if (major == 0) {
        OSVERSIONINFOEXA osvi;
        ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
        
        typedef LONG (WINAPI* PRTLGETVERSION)(OSVERSIONINFOEXW*);
        PRTLGETVERSION pRtlGetVersion;
        pRtlGetVersion = (PRTLGETVERSION)GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "RtlGetVersion");
        
        if (pRtlGetVersion) {
            pRtlGetVersion((OSVERSIONINFOEXW*)&osvi);
            major = osvi.dwMajorVersion;
            minor = osvi.dwMinorVersion;
            build = osvi.dwBuildNumber;
            isWorkstation = (osvi.wProductType == VER_NT_WORKSTATION);
            isServer = !isWorkstation;
        } else {
            // Finally try using GetVersionExA, although it may not be accurate
            if (GetVersionExA((OSVERSIONINFOA*)&osvi)) {
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
    
    // Detect specific Windows version
    const char* windowsVersion = "Unknown version";
    
    // Determine specific version based on version number
    if (major == 10) {
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
    
    // CPU architecture
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
    
    // System memory information
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        WriteLog(LOG_LEVEL_INFO, "Physical Memory: %.2f GB / %.2f GB (%d%% used)", 
            (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024.0 * 1024 * 1024), 
            memInfo.ullTotalPhys / (1024.0 * 1024 * 1024),
            memInfo.dwMemoryLoad);
    }
    
    // Don't get screen resolution information as it's not accurate and not necessary for debugging
    
    // Check if UAC is enabled
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
    
    // Check if running as administrator
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
 * @brief Get log file path
 * 
 * Build log filename based on config file path
 * 
 * @param logPath Log path buffer
 * @param size Buffer size
 */
static void GetLogFilePath(char* logPath, size_t size) {
    char configPath[MAX_PATH] = {0};
    
    // Get directory containing config file
    GetConfigPath(configPath, MAX_PATH);
    
    // Determine config file directory
    char* lastSeparator = strrchr(configPath, '\\');
    if (lastSeparator) {
        size_t dirLen = lastSeparator - configPath + 1;
        
        // Copy directory part
        strncpy(logPath, configPath, dirLen);
        
        // Use simple log filename
        _snprintf_s(logPath + dirLen, size - dirLen, _TRUNCATE, "Catime_Logs.log");
    } else {
        // If config directory can't be determined, use current directory
        _snprintf_s(logPath, size, _TRUNCATE, "Catime_Logs.log");
    }
}

BOOL InitializeLogSystem(void) {
    InitializeCriticalSection(&logCS);
    
    GetLogFilePath(LOG_FILE_PATH, MAX_PATH);
    
    // Open file in write mode each startup, which clears existing content
    logFile = fopen(LOG_FILE_PATH, "w");
    if (!logFile) {
        // Failed to create log file
        return FALSE;
    }
    
    // Record log system initialization information
    WriteLog(LOG_LEVEL_INFO, "==================================================");
    // First record software version
    WriteLog(LOG_LEVEL_INFO, "Catime Version: %s", CATIME_VERSION);
    // Then record system environment information (before any possible errors)
    WriteLog(LOG_LEVEL_INFO, "-----------------System Information-----------------");
    LogSystemInformation();
    WriteLog(LOG_LEVEL_INFO, "-----------------Application Information-----------------");
    WriteLog(LOG_LEVEL_INFO, "Log system initialization complete, Catime started");
    
    return TRUE;
}

void WriteLog(LogLevel level, const char* format, ...) {
    if (!logFile) {
        return;
    }
    
    EnterCriticalSection(&logCS);
    
    // Get current time
    time_t now;
    struct tm local_time;
    char timeStr[32] = {0};
    
    time(&now);
    localtime_s(&local_time, &now);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &local_time);
    
    // Write log header
    fprintf(logFile, "[%s] [%s] ", timeStr, LOG_LEVEL_STRINGS[level]);
    
    // Format and write log content
    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    
    // New line
    fprintf(logFile, "\n");
    
    // Flush buffer immediately to ensure logs are saved even if program crashes
    fflush(logFile);
    
    LeaveCriticalSection(&logCS);
}

void GetLastErrorDescription(DWORD errorCode, char* buffer, int bufferSize) {
    if (!buffer || bufferSize <= 0) {
        return;
    }
    
    LPSTR messageBuffer = NULL;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0, NULL);
    
    if (size > 0) {
        // Remove trailing newlines
        if (size >= 2 && messageBuffer[size-2] == '\r' && messageBuffer[size-1] == '\n') {
            messageBuffer[size-2] = '\0';
        }
        
        strncpy_s(buffer, bufferSize, messageBuffer, _TRUNCATE);
        LocalFree(messageBuffer);
    } else {
        _snprintf_s(buffer, bufferSize, _TRUNCATE, "Unknown error (code: %lu)", errorCode);
    }
}

// Signal handler function - used to handle various C standard signals
void SignalHandler(int signal) {
    char errorMsg[256] = {0};
    
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
    
    // Record exception information
    if (logFile) {
        fprintf(logFile, "[FATAL] Fatal signal occurred: %s (signal number: %d)\n", 
                errorMsg, signal);
        fflush(logFile);
        
        // Close log file
        fclose(logFile);
        logFile = NULL;
    }
    
    // Display error message box
    MessageBox(NULL, L"The program encountered a serious error. Please check the log file for detailed information.", L"Fatal Error", MB_ICONERROR | MB_OK);
    
    // Terminate program
    exit(signal);
}

void SetupExceptionHandler(void) {
    // Set up standard C signal handlers
    signal(SIGFPE, SignalHandler);   // Floating point exception
    signal(SIGILL, SignalHandler);   // Illegal instruction
    signal(SIGSEGV, SignalHandler);  // Segmentation fault
    signal(SIGTERM, SignalHandler);  // Termination signal
    signal(SIGABRT, SignalHandler);  // Abnormal termination
    signal(SIGINT, SignalHandler);   // User interrupt
}

// Call this function when program exits to clean up log resources
void CleanupLogSystem(void) {
    if (logFile) {
        WriteLog(LOG_LEVEL_INFO, "Catime exited normally");
        WriteLog(LOG_LEVEL_INFO, "==================================================");
        fclose(logFile);
        logFile = NULL;
    }
    
    DeleteCriticalSection(&logCS);
}
