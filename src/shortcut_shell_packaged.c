#include "shortcut_checker_internal.h"

#include "log.h"
#include "utils/string_convert.h"

#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

#define SHORTCUT_FILENAME "Catime.lnk"
#define SHORTCUT_DESCRIPTION L"A very useful timer (Pomodoro Clock)"

typedef struct {
    IShellLinkW* shellLink;
    IPersistFile* persistFile;
} PackagedShellLink;

static bool CopyExact(const char* source, char* output, size_t outputSize) {
    if (!source || !output || outputSize == 0) return false;
    size_t length = strlen(source);
    if (length >= outputSize) return false;
    memcpy(output, source, length + 1);
    return true;
}

static bool InitializeLink(PackagedShellLink* link) {
    if (!link) return false;
    memset(link, 0, sizeof(*link));
    HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IShellLinkW,
                                  (void**)&link->shellLink);
    if (FAILED(hr)) return false;
    hr = link->shellLink->lpVtbl->QueryInterface(
        link->shellLink, &IID_IPersistFile, (void**)&link->persistFile);
    if (SUCCEEDED(hr)) return true;
    link->shellLink->lpVtbl->Release(link->shellLink);
    memset(link, 0, sizeof(*link));
    return false;
}

static void CleanupLink(PackagedShellLink* link) {
    if (!link) return;
    if (link->persistFile) link->persistFile->lpVtbl->Release(link->persistFile);
    if (link->shellLink) link->shellLink->lpVtbl->Release(link->shellLink);
    memset(link, 0, sizeof(*link));
}

static bool BuildParsingName(const wchar_t* appUserModelId,
                             wchar_t* output, size_t outputSize) {
    return appUserModelId && *appUserModelId && output && outputSize > 0 &&
           _snwprintf_s(output, outputSize, _TRUNCATE,
                        L"shell:AppsFolder\\%ls", appUserModelId) >= 0;
}

static ShortcutStatus FindDesktopShortcut(char* output, size_t outputSize) {
    const int folders[] = {CSIDL_DESKTOP, CSIDL_COMMON_DESKTOPDIRECTORY};
    bool resolvedFolder = false;
    for (size_t i = 0; i < _countof(folders); ++i) {
        wchar_t desktop[MAX_PATH] = {0};
        wchar_t shortcut[MAX_PATH] = {0};
        if (FAILED(SHGetFolderPathW(NULL, folders[i], NULL, 0, desktop))) {
            continue;
        }
        resolvedFolder = true;
        if (!PathCombineW(shortcut, desktop, L"Catime.lnk")) {
            return SHORTCUT_CHECK_ERROR;
        }
        DWORD attributes = GetFileAttributesW(shortcut);
        if (attributes != INVALID_FILE_ATTRIBUTES) {
            return WideToUtf8(shortcut, output, outputSize)
                       ? SHORTCUT_POINTS_TO_CURRENT
                       : SHORTCUT_CHECK_ERROR;
        }
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            return SHORTCUT_CHECK_ERROR;
        }
    }
    return resolvedFolder ? SHORTCUT_NOT_FOUND : SHORTCUT_CHECK_ERROR;
}

ShortcutStatus ShortcutShell_CheckPackagedPath(
    const wchar_t* appUserModelId, const char* shortcutPath,
    char* targetPath, size_t targetSize) {
    wchar_t shortcutW[MAX_PATH] = {0};
    wchar_t parsingName[MAX_PATH] = {0};
    wchar_t targetW[MAX_PATH] = {0};
    PIDLIST_ABSOLUTE expected = NULL;
    PIDLIST_ABSOLUTE actual = NULL;
    PackagedShellLink link;
    ShortcutStatus status = SHORTCUT_CHECK_ERROR;

    if (targetPath && targetSize > 0) targetPath[0] = '\0';
    if (!shortcutPath || !*shortcutPath ||
        !BuildParsingName(appUserModelId, parsingName, _countof(parsingName)) ||
        !Utf8ToWide(shortcutPath, shortcutW, _countof(shortcutW)) ||
        !InitializeLink(&link)) {
        return SHORTCUT_CHECK_ERROR;
    }

    HRESULT hr = SHParseDisplayName(parsingName, NULL, &expected, 0, NULL);
    if (SUCCEEDED(hr)) {
        hr = link.persistFile->lpVtbl->Load(
            link.persistFile, shortcutW, STGM_READ);
    }
    if (SUCCEEDED(hr)) {
        hr = link.shellLink->lpVtbl->GetIDList(link.shellLink, &actual);
    }
    if (SUCCEEDED(hr) && actual && expected && ILIsEqual(actual, expected)) {
        status = SHORTCUT_POINTS_TO_CURRENT;
    } else if (targetPath && targetSize > 0 &&
               SUCCEEDED(link.shellLink->lpVtbl->GetPath(
                   link.shellLink, targetW, _countof(targetW), NULL,
                   SLGP_RAWPATH)) && targetW[0] != L'\0' &&
               WideToUtf8(targetW, targetPath, targetSize)) {
        status = SHORTCUT_POINTS_TO_OTHER;
    }

    if (actual) CoTaskMemFree(actual);
    if (expected) CoTaskMemFree(expected);
    CleanupLink(&link);
    return status;
}

ShortcutStatus ShortcutShell_CheckPackagedStatus(
    const wchar_t* appUserModelId, char* shortcutPath, size_t shortcutSize,
    char* targetPath, size_t targetSize) {
    char foundPath[MAX_PATH] = {0};
    ShortcutStatus found = FindDesktopShortcut(foundPath, sizeof(foundPath));
    if (found != SHORTCUT_POINTS_TO_CURRENT) return found;
    if (shortcutPath && shortcutSize > 0 &&
        !CopyExact(foundPath, shortcutPath, shortcutSize)) {
        return SHORTCUT_CHECK_ERROR;
    }
    return ShortcutShell_CheckPackagedPath(
        appUserModelId, foundPath, targetPath, targetSize);
}

bool ShortcutShell_CreateOrUpdatePackaged(
    const wchar_t* appUserModelId, const char* existingShortcutPath) {
    wchar_t parsingName[MAX_PATH] = {0};
    wchar_t shortcutPath[MAX_PATH] = {0};
    PIDLIST_ABSOLUTE appPidl = NULL;
    PackagedShellLink link;
    if (!BuildParsingName(appUserModelId, parsingName, _countof(parsingName)) ||
        !InitializeLink(&link)) return false;

    bool pathReady = existingShortcutPath && *existingShortcutPath
        ? Utf8ToWide(existingShortcutPath, shortcutPath,
                     _countof(shortcutPath))
        : SUCCEEDED(SHGetFolderPathW(
              NULL, CSIDL_DESKTOP, NULL, 0, shortcutPath)) &&
          PathAppendW(shortcutPath, L"Catime.lnk");
    HRESULT hr = pathReady
        ? SHParseDisplayName(parsingName, NULL, &appPidl, 0, NULL)
        : E_FAIL;
    if (SUCCEEDED(hr)) {
        hr = link.shellLink->lpVtbl->SetIDList(link.shellLink, appPidl);
    }
    if (SUCCEEDED(hr)) {
        hr = link.shellLink->lpVtbl->SetDescription(
            link.shellLink, SHORTCUT_DESCRIPTION);
    }
    if (SUCCEEDED(hr)) {
        link.shellLink->lpVtbl->SetShowCmd(link.shellLink, SW_SHOWNORMAL);
        hr = link.persistFile->lpVtbl->Save(
            link.persistFile, shortcutPath, TRUE);
    }
    if (appPidl) CoTaskMemFree(appPidl);
    CleanupLink(&link);
    return SUCCEEDED(hr);
}
