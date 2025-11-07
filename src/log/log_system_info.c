/**
 * @file log_system_info.c
 * @brief System information collection implementation
 */

#include <stdio.h>
#include <windows.h>
#include "log/log_system_info.h"
#include "log.h"

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

