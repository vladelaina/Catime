#include "config_path_internal.h"
#include "config.h"
#include "utils/package_identity.h"
#include "utils/win32_dynamic_loader.h"
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

static BOOL GetUserProfilePathW(wchar_t* outPath, size_t outSize) {
    if (!outPath || outSize == 0 || outSize > MAXDWORD) return FALSE;
    outPath[0] = L'\0';

    DWORD len = GetEnvironmentVariableW(L"USERPROFILE", outPath, (DWORD)outSize);
    if (len > 0 && len < outSize) {
        return TRUE;
    }

    outPath[0] = L'\0';
    return SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, outPath));
}

static BOOL ResolveStandardLocalAppDataW(wchar_t* outPath, size_t outSize) {
    if (!outPath || outSize == 0 || outSize > MAXDWORD) return FALSE;
    outPath[0] = L'\0';

    PWSTR knownFolderPath = NULL;
    HRESULT hr = E_FAIL;

    HMODULE hShell = LoadLibraryW(L"shell32.dll");
    if (hShell) {
        typedef HRESULT (WINAPI *PFN_SHGetKnownFolderPath)(const GUID*, DWORD, HANDLE, PWSTR*);
        PFN_SHGetKnownFolderPath pfn = NULL;
        CATIME_LOAD_PROC_ADDRESS(hShell, "SHGetKnownFolderPath", pfn);
        if (pfn) {
            static const GUID folderIdLocalAppDataGuid = {
                0xF1B32785, 0x6FBA, 0x4FCF,
                {0x9D, 0x55, 0x7B, 0x8E, 0x7F, 0x15, 0x70, 0x91}
            };
            hr = pfn(&folderIdLocalAppDataGuid, 0, NULL, &knownFolderPath);
        }
        FreeLibrary(hShell);
    }

    if (SUCCEEDED(hr) && knownFolderPath) {
        BOOL fits = wcslen(knownFolderPath) < outSize;
        if (fits) {
            wcscpy_s(outPath, outSize, knownFolderPath);
        }
        CoTaskMemFree(knownFolderPath);
        if (fits) return TRUE;
    }

    return SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, outPath));
}

BOOL ConfigPath_ResolveEffectiveLocalAppDataW(wchar_t* outPath, size_t outSize) {
    if (!outPath || outSize == 0 || outSize > MAXDWORD) return FALSE;
    outPath[0] = L'\0';

    if (!IsRunningPackagedApp()) {
        return ResolveStandardLocalAppDataW(outPath, outSize);
    }

    wchar_t userProfile[MAX_PATH] = {0};
    wchar_t packageFamily[MAX_PATH] = {0};
    if (!GetUserProfilePathW(userProfile, MAX_PATH) ||
        !GetCurrentPackageFamilyNameSafeW(packageFamily, MAX_PATH)) {
        return FALSE;
    }

    int written = _snwprintf_s(
        outPath, outSize, _TRUNCATE,
        L"%s\\AppData\\Local\\Packages\\%s\\LocalCache\\Local",
        userProfile, packageFamily);
    if (written < 0) {
        outPath[0] = L'\0';
        return FALSE;
    }

    int createResult = SHCreateDirectoryExW(NULL, outPath, NULL);
    return ConfigPath_IsDirectoryCreateResultOk(createResult, outPath);
}

BOOL GetEffectiveLocalAppDataPath(char* path, size_t size) {
    if (!path || size == 0 || size > INT_MAX) return FALSE;
    path[0] = '\0';

    wchar_t effectivePath[MAX_PATH] = {0};
    if (!ConfigPath_ResolveEffectiveLocalAppDataW(effectivePath, MAX_PATH)) {
        return FALSE;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, effectivePath, -1,
                            path, (int)size, NULL, NULL) <= 0) {
        path[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

BOOL ExpandEffectiveLocalAppDataPath(const char* value,
                                     char* expanded,
                                     size_t expandedSize) {
    if (!value || !expanded || expandedSize == 0) return FALSE;
    expanded[0] = '\0';

    static const char token[] = "%LOCALAPPDATA%";
    size_t tokenLen = sizeof(token) - 1;
    if (_strnicmp(value, token, tokenLen) != 0) {
        if (strlen(value) >= expandedSize) return FALSE;
        strcpy_s(expanded, expandedSize, value);
        return TRUE;
    }

    const char* suffix = value + tokenLen;
    char expansionRoot[MAX_PATH] = {0};

    /*
     * Older Store configurations may already contain the full package-private
     * suffix after %LOCALAPPDATA%. Expanding those against the effective root
     * would duplicate Packages\\<PFN>\\LocalCache\\Local.
     */
    if (IsRunningPackagedApp()) {
        wchar_t packageFamilyW[MAX_PATH] = {0};
        char packageFamilyUtf8[MAX_PATH] = {0};
        char legacyPrivatePrefix[MAX_PATH] = {0};
        if (GetCurrentPackageFamilyNameSafeW(packageFamilyW, MAX_PATH) &&
            WideCharToMultiByte(CP_UTF8, 0, packageFamilyW, -1,
                                packageFamilyUtf8, MAX_PATH, NULL, NULL) > 0 &&
            snprintf(legacyPrivatePrefix, sizeof(legacyPrivatePrefix),
                     "\\Packages\\%s\\LocalCache\\Local", packageFamilyUtf8) > 0 &&
            _strnicmp(suffix, legacyPrivatePrefix,
                      strlen(legacyPrivatePrefix)) == 0) {
            wchar_t standardRootW[MAX_PATH] = {0};
            if (!ResolveStandardLocalAppDataW(standardRootW, MAX_PATH) ||
                WideCharToMultiByte(CP_UTF8, 0, standardRootW, -1,
                                    expansionRoot, MAX_PATH, NULL, NULL) <= 0) {
                return FALSE;
            }
        }
    }

    if (expansionRoot[0] == '\0' &&
        !GetEffectiveLocalAppDataPath(expansionRoot, sizeof(expansionRoot))) {
        return FALSE;
    }

    int written = snprintf(expanded, expandedSize, "%s%s",
                           expansionRoot, suffix);
    if (written < 0 || (size_t)written >= expandedSize) {
        expanded[0] = '\0';
        return FALSE;
    }
    return TRUE;
}
