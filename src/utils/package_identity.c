/**
 * @file package_identity.c
 * @brief Runtime detection for MSIX/Microsoft Store package identity
 */

#include "utils/package_identity.h"
#include "utils/win32_dynamic_loader.h"
#include <stdint.h>

#ifndef APPMODEL_ERROR_NO_PACKAGE
#define APPMODEL_ERROR_NO_PACKAGE 15700L
#endif

typedef LONG (WINAPI *GetCurrentPackageFullNameFn)(UINT32*, PWSTR);
typedef LONG (WINAPI *GetCurrentPackageFamilyNameFn)(UINT32*, PWSTR);

BOOL IsRunningPackagedApp(void) {
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) return FALSE;

    GetCurrentPackageFullNameFn getCurrentPackageFullName = NULL;
    CATIME_LOAD_PROC_ADDRESS(kernel32, "GetCurrentPackageFullName",
                             getCurrentPackageFullName);
    if (!getCurrentPackageFullName) return FALSE;

    UINT32 length = 0;
    LONG result = getCurrentPackageFullName(&length, NULL);
    return result == ERROR_INSUFFICIENT_BUFFER;
}

BOOL GetCurrentPackageFamilyNameSafeW(wchar_t* familyName, size_t familyNameSize) {
    if (!familyName || familyNameSize == 0 || familyNameSize > UINT32_MAX) {
        return FALSE;
    }
    familyName[0] = L'\0';

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) return FALSE;

    GetCurrentPackageFamilyNameFn getCurrentPackageFamilyName = NULL;
    CATIME_LOAD_PROC_ADDRESS(kernel32, "GetCurrentPackageFamilyName",
                             getCurrentPackageFamilyName);
    if (!getCurrentPackageFamilyName) return FALSE;

    UINT32 length = (UINT32)familyNameSize;
    LONG result = getCurrentPackageFamilyName(&length, familyName);
    if (result == APPMODEL_ERROR_NO_PACKAGE || result != ERROR_SUCCESS) {
        familyName[0] = L'\0';
        return FALSE;
    }

    return TRUE;
}
