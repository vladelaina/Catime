/**
 * @file log_system_info.c
 * @brief System information collection implementation
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "utils/win32_dynamic_loader.h"
#include "utils/package_identity.h"
#include "utils/string_convert.h"
#include "log/log_system_info.h"
#include "log.h"

#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif

#ifndef likely
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#else
#define likely(x)   (!!(x))
#endif
#endif
#ifndef unlikely
#if defined(__GNUC__) || defined(__clang__)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define unlikely(x) (!!(x))
#endif
#endif

#define KB  (1024.0)
#define MB  (KB * 1024.0)
#define GB  (MB * 1024.0)

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

static const char* GetOSVersionName(const OSVersionInfo* osVersion) {
    if (unlikely(!osVersion)){
        return "Invalid OS version information";
    }
    const DWORD major = osVersion->major;
    const DWORD minor = osVersion->minor;
    const DWORD build = osVersion->minBuild;
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
    switch (archId) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x64 (AMD64)";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86 (Intel)";
        case PROCESSOR_ARCHITECTURE_ARM:   return "ARM";
        case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";
        default: return "Unknown architecture";
    }
}

/** RtlGetVersion bypasses app manifest compatibility layer */
static bool GetOSVersionInfo(OSVersionInfo* osVersion) {
    if (unlikely(!osVersion)) {
        return false;
    }
        
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    
    if (unlikely(!hNtdll)) return false;
    
    RtlGetVersionPtr pRtlGetVersion = NULL;
    CATIME_LOAD_PROC_ADDRESS(hNtdll, "RtlGetVersion", pRtlGetVersion);
    if (unlikely(!pRtlGetVersion)) return false;
    
    RTL_OSVERSIONINFOW rovi = {0};
    rovi.dwOSVersionInfoSize = sizeof(rovi);
    
    if (likely(pRtlGetVersion(&rovi) == 0)) {
        osVersion->major = rovi.dwMajorVersion;
        osVersion->minor = rovi.dwMinorVersion;
        osVersion->minBuild = rovi.dwBuildNumber;
        return true;
    }
    
    return false;
}

void LogOSVersion(void) {
    OSVersionInfo osVersion = {0};
    if (likely(GetOSVersionInfo(&osVersion))) {
        osVersion.name = GetOSVersionName(&osVersion);
        WriteLog(LOG_LEVEL_INFO, "Operating System: %s (%lu.%lu) Build %lu", 
                 osVersion.name, osVersion.major, osVersion.minor, osVersion.minBuild);
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

/**
 * Format byte size to human-readable string
 * @param bytes Size in bytes
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 */
static void FormatBytes(DWORDLONG bytes, char* buffer, size_t bufferSize) {
    if (unlikely(!buffer || bufferSize == 0)) return;
    
    
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

void LogMemoryInfo(void) {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    
    if (likely(GlobalMemoryStatusEx(&memInfo))) {
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
    bool uacEnabled = false;
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

void LogPackageIdentity(void) {
    wchar_t familyName[MAX_PATH] = {0};
    if (!IsRunningPackagedApp()) {
        WriteLog(LOG_LEVEL_INFO, "Package Identity: Unpackaged Win32");
        return;
    }

    if (GetCurrentPackageFamilyNameSafeW(familyName, _countof(familyName))) {
        char* familyNameUtf8 = WideToUtf8Alloc(familyName);
        if (familyNameUtf8) {
            WriteLog(LOG_LEVEL_INFO, "Package Identity: MSIX (%s)", familyNameUtf8);
            free(familyNameUtf8);
            return;
        }
    }

    WriteLog(LOG_LEVEL_INFO, "Package Identity: MSIX");
}

