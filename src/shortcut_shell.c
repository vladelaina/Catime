#include "shortcut_checker_internal.h"
#include "log.h"
#include "utils/string_convert.h"
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <stdio.h>
#include <string.h>

#define SHORTCUT_FILENAME "Catime.lnk"
#define SHORTCUT_DESCRIPTION L"A very useful timer (Pomodoro Clock)"

typedef struct {
    IShellLinkW* shellLink;
    IPersistFile* persistFile;
} ComShellLink;

typedef enum {
    FIND_SHORTCUT_ERROR = -1,
    FIND_SHORTCUT_NOT_FOUND = 0,
    FIND_SHORTCUT_FOUND = 1
} FindShortcutResult;

static bool CopyStringExact(const char* src, char* output, size_t outputSize) {
    if (!src || !output || outputSize == 0) return false;
    output[0] = '\0';
    size_t len = strlen(src);
    if (len >= outputSize) return false;
    memcpy(output, src, len + 1);
    return true;
}

static bool InitComShellLink(ComShellLink* link) {
    if (!link) return false;
    link->shellLink = NULL;
    link->persistFile = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IShellLinkW, (void**)&link->shellLink);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create IShellLink interface, hr=0x%08X",
                  (unsigned int)hr);
        return false;
    }
    hr = link->shellLink->lpVtbl->QueryInterface(
        link->shellLink, &IID_IPersistFile, (void**)&link->persistFile);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get IPersistFile interface, hr=0x%08X",
                  (unsigned int)hr);
        link->shellLink->lpVtbl->Release(link->shellLink);
        link->shellLink = NULL;
        return false;
    }
    return true;
}

static void CleanupComShellLink(ComShellLink* link) {
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

static bool GetDesktopPath(int desktopType, char* output, size_t outputSize) {
    wchar_t path[MAX_PATH];
    HRESULT hr = SHGetFolderPathW(NULL, desktopType, NULL, 0, path);
    return SUCCEEDED(hr) && WideToUtf8(path, output, outputSize);
}

static bool BuildShortcutPath(const char* desktopPath, char* output,
                              size_t outputSize) {
    if (!desktopPath || !output || outputSize == 0) return false;
    int written = snprintf(output, outputSize, "%s\\%s", desktopPath,
                           SHORTCUT_FILENAME);
    if (written < 0 || (size_t)written >= outputSize) {
        output[0] = '\0';
        return false;
    }
    return true;
}

static bool ExtractDirectory(const char* filePath, char* output,
                             size_t outputSize) {
    if (!CopyStringExact(filePath, output, outputSize)) return false;
    char* lastSlash = strrchr(output, '\\');
    if (!lastSlash || lastSlash == output) {
        output[0] = '\0';
        return false;
    }
    *lastSlash = '\0';
    return true;
}

static bool ReadShortcutDetails(const char* shortcutPath, char* target,
                                size_t targetSize, char* arguments,
                                size_t argumentsSize) {
    ComShellLink link;
    if (!InitComShellLink(&link)) return false;
    wchar_t shortcutPathW[MAX_PATH];
    wchar_t targetW[MAX_PATH];
    wchar_t argumentsW[MAX_PATH];
    WIN32_FIND_DATAW findData;
    bool success = false;
    if (target && targetSize > 0) target[0] = '\0';
    if (arguments && argumentsSize > 0) arguments[0] = '\0';
    if (!Utf8ToWide(shortcutPath, shortcutPathW, MAX_PATH)) goto cleanup;
    HRESULT hr = link.persistFile->lpVtbl->Load(link.persistFile,
                                                shortcutPathW, STGM_READ);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to load shortcut, hr=0x%08X", (unsigned int)hr);
        goto cleanup;
    }
    hr = link.shellLink->lpVtbl->GetPath(link.shellLink, targetW, MAX_PATH,
                                         &findData, SLGP_RAWPATH);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get shortcut target path, hr=0x%08X",
                  (unsigned int)hr);
        goto cleanup;
    }
    if (target && targetSize > 0 && !WideToUtf8(targetW, target, targetSize)) {
        goto cleanup;
    }
    if (arguments && argumentsSize > 0) {
        hr = link.shellLink->lpVtbl->GetArguments(
            link.shellLink, argumentsW, _countof(argumentsW));
        if (FAILED(hr) || !WideToUtf8(argumentsW, arguments, argumentsSize)) {
            goto cleanup;
        }
    }
    success = true;
cleanup:
    CleanupComShellLink(&link);
    return success;
}

static bool ReadShortcutTarget(const char* shortcutPath, char* target,
                               size_t targetSize) {
    return ReadShortcutDetails(shortcutPath, target, targetSize, NULL, 0);
}

static FindShortcutResult FindExistingShortcut(char* output,
                                               size_t outputSize) {
    if (!output || outputSize == 0) return FIND_SHORTCUT_ERROR;
    output[0] = '\0';
    char desktopPath[MAX_PATH];
    char shortcutPath[MAX_PATH];
    wchar_t shortcutPathW[MAX_PATH];
    bool resolvedDesktop = false;
    const int desktopTypes[] = {CSIDL_DESKTOP, CSIDL_COMMON_DESKTOPDIRECTORY};
    for (size_t i = 0; i < sizeof(desktopTypes) / sizeof(desktopTypes[0]); i++) {
        if (!GetDesktopPath(desktopTypes[i], desktopPath,
                            _countof(desktopPath))) {
            continue;
        }
        resolvedDesktop = true;
        if (!BuildShortcutPath(desktopPath, shortcutPath,
                               _countof(shortcutPath)) ||
            !Utf8ToWide(shortcutPath, shortcutPathW,
                        _countof(shortcutPathW))) {
            return FIND_SHORTCUT_ERROR;
        }
        DWORD attributes = GetFileAttributesW(shortcutPathW);
        if (attributes != INVALID_FILE_ATTRIBUTES) {
            return CopyStringExact(shortcutPath, output, outputSize)
                       ? FIND_SHORTCUT_FOUND
                       : FIND_SHORTCUT_ERROR;
        }
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            return FIND_SHORTCUT_ERROR;
        }
    }
    return resolvedDesktop ? FIND_SHORTCUT_NOT_FOUND : FIND_SHORTCUT_ERROR;
}

ShortcutStatus ShortcutShell_CheckStatus(const char* exePath,
                                         char* shortcutPath, size_t shortcutSize,
                                         char* targetPath, size_t targetSize) {
    char shortcut[MAX_PATH];
    FindShortcutResult found = FindExistingShortcut(shortcut, MAX_PATH);
    if (found == FIND_SHORTCUT_NOT_FOUND) return SHORTCUT_NOT_FOUND;
    if (found == FIND_SHORTCUT_ERROR) return SHORTCUT_CHECK_ERROR;
    if (shortcutPath && shortcutSize > 0 &&
        !CopyStringExact(shortcut, shortcutPath, shortcutSize))
        return SHORTCUT_CHECK_ERROR;
    return ShortcutShell_CheckPath(
        exePath, shortcut, targetPath, targetSize);
}

ShortcutStatus ShortcutShell_CheckPath(const char* exePath,
                                       const char* shortcutPath,
                                       char* targetPath, size_t targetSize) {
    char target[MAX_PATH];
    if (!exePath || !*exePath || !shortcutPath || !*shortcutPath ||
        !ReadShortcutTarget(shortcutPath, target, MAX_PATH)) {
        return SHORTCUT_CHECK_ERROR;
    }
    if (targetPath && targetSize > 0 &&
        !CopyStringExact(target, targetPath, targetSize))
        return SHORTCUT_CHECK_ERROR;
    return _stricmp(target, exePath) == 0
        ? SHORTCUT_POINTS_TO_CURRENT : SHORTCUT_POINTS_TO_OTHER;
}

static bool ConfigureShellLink(ComShellLink* link, const char* exePath) {
    wchar_t exePathW[MAX_PATH];
    wchar_t workDirW[MAX_PATH];
    char workDir[MAX_PATH];
    if (!Utf8ToWide(exePath, exePathW, MAX_PATH)) return false;
    HRESULT hr = link->shellLink->lpVtbl->SetPath(link->shellLink, exePathW);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set shortcut target path, hr=0x%08X",
                  (unsigned int)hr);
        return false;
    }
    if (ExtractDirectory(exePath, workDir, MAX_PATH) &&
        Utf8ToWide(workDir, workDirW, MAX_PATH)) {
        hr = link->shellLink->lpVtbl->SetWorkingDirectory(link->shellLink,
                                                          workDirW);
        if (FAILED(hr)) LOG_WARNING("Failed to set working directory, hr=0x%08X", (unsigned int)hr);
    }
    hr = link->shellLink->lpVtbl->SetIconLocation(link->shellLink, exePathW, 0);
    if (FAILED(hr)) LOG_WARNING("Failed to set icon, hr=0x%08X", (unsigned int)hr);
    hr = link->shellLink->lpVtbl->SetDescription(link->shellLink,
                                                 SHORTCUT_DESCRIPTION);
    if (FAILED(hr)) LOG_WARNING("Failed to set description, hr=0x%08X", (unsigned int)hr);
    link->shellLink->lpVtbl->SetShowCmd(link->shellLink, SW_SHOWNORMAL);
    return true;
}

bool ShortcutShell_CreateOrUpdate(const char* exePath,
                                  const char* existingShortcutPath) {
    char shortcutPath[MAX_PATH];
    if (existingShortcutPath && *existingShortcutPath) {
        if (!CopyStringExact(existingShortcutPath, shortcutPath, MAX_PATH)) {
            LOG_ERROR("Existing shortcut path is too long");
            return false;
        }
    } else {
        char desktopPath[MAX_PATH];
        if (!GetDesktopPath(CSIDL_DESKTOP, desktopPath, MAX_PATH) ||
            !BuildShortcutPath(desktopPath, shortcutPath, MAX_PATH)) {
            LOG_ERROR("Failed to build desktop shortcut path");
            return false;
        }
    }
    ComShellLink link;
    if (!InitComShellLink(&link)) return false;
    bool success = false;
    wchar_t shortcutPathW[MAX_PATH];
    if (ConfigureShellLink(&link, exePath) &&
        Utf8ToWide(shortcutPath, shortcutPathW, MAX_PATH)) {
        HRESULT hr = link.persistFile->lpVtbl->Save(link.persistFile,
                                                    shortcutPathW, TRUE);
        if (FAILED(hr))
            LOG_ERROR("Failed to save shortcut, hr=0x%08X", (unsigned int)hr);
        else
            success = true;
    }
    CleanupComShellLink(&link);
    return success;
}
