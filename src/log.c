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

static wchar_t LOG_FILE_PATH[MAX_PATH] = {0};
static FILE* logFile = NULL;
static CRITICAL_SECTION logCS;

static const char* LOG_LEVEL_STRINGS[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL"
};

static void LogSystemInformation(void) {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    
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
            if (pRtlGetVersion(&rovi) == 0) {
                major = rovi.dwMajorVersion;
                minor = rovi.dwMinorVersion;
                build = rovi.dwBuildNumber;
            }
        }
    }
    
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
    
    const char* windowsVersion = "Unknown version";
    
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
    
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        WriteLog(LOG_LEVEL_INFO, "Physical Memory: %.2f GB / %.2f GB (%d%% used)", 
            (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024.0 * 1024 * 1024), 
            memInfo.ullTotalPhys / (1024.0 * 1024 * 1024),
            memInfo.dwMemoryLoad);
    }
    
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

BOOL InitializeLogSystem(void) {
    InitializeCriticalSection(&logCS);
    
    GetLogFilePath(LOG_FILE_PATH, MAX_PATH);
    
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
    
    fflush(logFile);
    
    LeaveCriticalSection(&logCS);
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
    
    if (size > 0) {
        if (size >= 2 && messageBuffer[size-2] == L'\r' && messageBuffer[size-1] == L'\n') {
            messageBuffer[size-2] = L'\0';
        }
        
        WideCharToMultiByte(CP_UTF8, 0, messageBuffer, -1, buffer, bufferSize, NULL, NULL);
        LocalFree(messageBuffer);
    } else {
        _snprintf_s(buffer, bufferSize, _TRUNCATE, "Unknown error (code: %lu)", errorCode);
    }
}

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

void SetupExceptionHandler(void) {
    signal(SIGFPE, SignalHandler);
    signal(SIGILL, SignalHandler);
    signal(SIGSEGV, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGABRT, SignalHandler);
    signal(SIGINT, SignalHandler);
}

void CleanupLogSystem(void) {
    if (logFile) {
        WriteLog(LOG_LEVEL_INFO, "Catime exited normally");
        WriteLog(LOG_LEVEL_INFO, "==================================================");
        fclose(logFile);
        logFile = NULL;
    }
    
    DeleteCriticalSection(&logCS);
}