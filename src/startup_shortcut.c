#include "startup_shortcut.h"

#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shlwapi.h>
#include <strsafe.h>

typedef struct {
    IShellLinkW* shellLink;
    IPersistFile* persistFile;
} StartupShellLink;

static volatile LONG g_transactionCounter = 0;

static BOOL BuildTransactionPath(const wchar_t* shortcutPath,
                                 const wchar_t* suffix,
                                 wchar_t* output, size_t outputCount) {
    LONG sequence;
    if (!shortcutPath || !suffix || !output || outputCount == 0) return FALSE;
    sequence = InterlockedIncrement(&g_transactionCounter);
    return SUCCEEDED(StringCchPrintfW(
        output, outputCount, L"%ls.%lu.%ld.%ls", shortcutPath,
        GetCurrentProcessId(), sequence, suffix));
}

static BOOL EnsureComInitialized(BOOL* shouldUninitialize) {
    HRESULT hr;
    if (!shouldUninitialize) return FALSE;
    *shouldUninitialize = FALSE;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        *shouldUninitialize = TRUE;
        return TRUE;
    }
    return hr == RPC_E_CHANGED_MODE;
}

static BOOL InitializeShellLink(StartupShellLink* link) {
    HRESULT hr;
    if (!link) return FALSE;
    link->shellLink = NULL;
    link->persistFile = NULL;

    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkW, (void**)&link->shellLink);
    if (FAILED(hr)) return FALSE;

    hr = link->shellLink->lpVtbl->QueryInterface(
        link->shellLink, &IID_IPersistFile, (void**)&link->persistFile);
    if (FAILED(hr)) {
        link->shellLink->lpVtbl->Release(link->shellLink);
        link->shellLink = NULL;
        return FALSE;
    }
    return TRUE;
}

static void CleanupShellLink(StartupShellLink* link) {
    if (!link) return;
    if (link->persistFile) {
        link->persistFile->lpVtbl->Release(link->persistFile);
        link->persistFile = NULL;
    }
    if (link->shellLink) {
        link->shellLink->lpVtbl->Release(link->shellLink);
        link->shellLink = NULL;
    }
}

static BOOL GetExecutableDirectory(const wchar_t* executablePath,
                                   wchar_t* output, size_t outputCount) {
    if (!executablePath || !*executablePath || !output || outputCount == 0) {
        return FALSE;
    }
    if (wcscpy_s(output, outputCount, executablePath) != 0) return FALSE;
    return PathRemoveFileSpecW(output);
}

BOOL StartupShortcut_Write(const wchar_t* shortcutPath,
                           const wchar_t* executablePath,
                           const wchar_t* arguments) {
    StartupShellLink link;
    wchar_t workingDirectory[MAX_PATH] = {0};
    BOOL shouldUninitialize = FALSE;
    BOOL success = FALSE;
    HRESULT hr;

    if (!shortcutPath || !*shortcutPath || !executablePath ||
        !*executablePath || !arguments ||
        !GetExecutableDirectory(executablePath, workingDirectory,
                                _countof(workingDirectory)) ||
        !EnsureComInitialized(&shouldUninitialize)) {
        return FALSE;
    }

    if (!InitializeShellLink(&link)) {
        if (shouldUninitialize) CoUninitialize();
        return FALSE;
    }

    hr = link.shellLink->lpVtbl->SetPath(link.shellLink, executablePath);
    if (SUCCEEDED(hr)) {
        hr = link.shellLink->lpVtbl->SetArguments(link.shellLink, arguments);
    }
    if (SUCCEEDED(hr)) {
        hr = link.shellLink->lpVtbl->SetWorkingDirectory(
            link.shellLink, workingDirectory);
    }
    if (SUCCEEDED(hr)) {
        (void)link.shellLink->lpVtbl->SetIconLocation(
            link.shellLink, executablePath, 0);
        link.shellLink->lpVtbl->SetShowCmd(link.shellLink, SW_SHOWNORMAL);
        hr = link.persistFile->lpVtbl->Save(link.persistFile, shortcutPath, TRUE);
    }
    success = SUCCEEDED(hr);

    CleanupShellLink(&link);
    if (shouldUninitialize) CoUninitialize();
    return success;
}

BOOL StartupShortcut_IsCurrent(const wchar_t* shortcutPath,
                               const wchar_t* executablePath,
                               const wchar_t* arguments) {
    StartupShellLink link;
    wchar_t actualPath[MAX_PATH] = {0};
    wchar_t actualArguments[128] = {0};
    wchar_t actualWorkingDirectory[MAX_PATH] = {0};
    wchar_t expectedWorkingDirectory[MAX_PATH] = {0};
    WIN32_FIND_DATAW findData = {0};
    BOOL shouldUninitialize = FALSE;
    BOOL current = FALSE;
    HRESULT hr;

    if (!shortcutPath || !*shortcutPath || !executablePath ||
        !*executablePath || !arguments ||
        !GetExecutableDirectory(executablePath, expectedWorkingDirectory,
                                _countof(expectedWorkingDirectory)) ||
        !EnsureComInitialized(&shouldUninitialize)) {
        return FALSE;
    }

    if (!InitializeShellLink(&link)) {
        if (shouldUninitialize) CoUninitialize();
        return FALSE;
    }

    hr = link.persistFile->lpVtbl->Load(link.persistFile, shortcutPath, STGM_READ);
    if (SUCCEEDED(hr)) {
        hr = link.shellLink->lpVtbl->GetPath(
            link.shellLink, actualPath, _countof(actualPath), &findData,
            SLGP_RAWPATH);
    }
    if (SUCCEEDED(hr)) {
        hr = link.shellLink->lpVtbl->GetArguments(
            link.shellLink, actualArguments, _countof(actualArguments));
    }
    if (SUCCEEDED(hr)) {
        hr = link.shellLink->lpVtbl->GetWorkingDirectory(
            link.shellLink, actualWorkingDirectory,
            _countof(actualWorkingDirectory));
    }
    if (SUCCEEDED(hr)) {
        current = _wcsicmp(actualPath, executablePath) == 0 &&
                  wcscmp(actualArguments, arguments) == 0 &&
                  _wcsicmp(actualWorkingDirectory,
                           expectedWorkingDirectory) == 0;
    }

    CleanupShellLink(&link);
    if (shouldUninitialize) CoUninitialize();
    return current;
}

BOOL StartupShortcut_ReplaceAtomically(const wchar_t* shortcutPath,
                                       const wchar_t* executablePath,
                                       const wchar_t* arguments) {
    wchar_t temporaryPath[MAX_PATH] = {0};
    wchar_t backupPath[MAX_PATH] = {0};
    BOOL hadExistingShortcut;
    BOOL backupCreated = FALSE;

    if (!shortcutPath || !*shortcutPath || !executablePath ||
        !*executablePath || !arguments ||
        !BuildTransactionPath(shortcutPath, L"tmp", temporaryPath,
                              _countof(temporaryPath)) ||
        !BuildTransactionPath(shortcutPath, L"bak", backupPath,
                              _countof(backupPath))) {
        return FALSE;
    }

    hadExistingShortcut =
        GetFileAttributesW(shortcutPath) != INVALID_FILE_ATTRIBUTES;
    DeleteFileW(temporaryPath);
    DeleteFileW(backupPath);

    if (!StartupShortcut_Write(temporaryPath, executablePath, arguments) ||
        !StartupShortcut_IsCurrent(temporaryPath, executablePath, arguments)) {
        DeleteFileW(temporaryPath);
        return FALSE;
    }

    if (hadExistingShortcut) {
        if (!CopyFileW(shortcutPath, backupPath, FALSE)) {
            DeleteFileW(temporaryPath);
            return FALSE;
        }
        backupCreated = TRUE;
    }

    if (!MoveFileExW(temporaryPath, shortcutPath,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporaryPath);
        DeleteFileW(backupPath);
        return FALSE;
    }

    if (!StartupShortcut_IsCurrent(shortcutPath, executablePath, arguments)) {
        if (backupCreated) {
            BOOL restored = MoveFileExW(
                backupPath, shortcutPath,
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
            if (!restored) {
                restored = CopyFileW(backupPath, shortcutPath, FALSE);
            }
            if (restored) DeleteFileW(backupPath);
        } else {
            DeleteFileW(shortcutPath);
        }
        DeleteFileW(temporaryPath);
        return FALSE;
    }

    DeleteFileW(backupPath);
    return TRUE;
}
