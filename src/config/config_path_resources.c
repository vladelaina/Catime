#include "config.h"
#include "utils/path_utils.h"
#include "log.h"
#include <windows.h>
#include <shlobj.h>
#include <limits.h>
#include <string.h>

static BOOL ResourceDirectoryResultOk(int result, const wchar_t* path) {
    if (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS) return TRUE;
    return path && GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES &&
           (GetFileAttributesW(path) & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static void GetResourceSubfolderPathUtf8(const wchar_t* subfolder,
                                         char* outPath, size_t outSize) {
    if (outPath && outSize) outPath[0] = '\0';
    char configPath[MAX_PATH] = {0};
    wchar_t wideConfigPath[MAX_PATH] = {0};
    if (!subfolder) return;
    GetConfigPath(configPath, MAX_PATH);
    if (!configPath[0] || !MultiByteToWideChar(CP_UTF8, 0, configPath, -1,
                             wideConfigPath, MAX_PATH)) return;
    wchar_t* slash = wcsrchr(wideConfigPath, L'\\');
    if (!slash) return;
    *slash = L'\0';
    wchar_t folder[MAX_PATH] = {0};
    if (_snwprintf_s(folder, MAX_PATH, _TRUNCATE, L"%s\\%s",
                     wideConfigPath, subfolder) < 0) return;
    int createResult = SHCreateDirectoryExW(NULL, folder, NULL);
    if (!ResourceDirectoryResultOk(createResult, folder)) {
        LOG_WARNING("Failed to create resource folder: %ls (error=%d)",
                    folder, createResult);
        return;
    }
    if (outPath && outSize &&
        (outSize > INT_MAX ||
         WideCharToMultiByte(CP_UTF8, 0, folder, -1, outPath,
                             (int)outSize, NULL, NULL) == 0)) outPath[0] = '\0';
}

static void EnsureDefaultResourceSubfolders(void) {
    const wchar_t* folders[] = {L"resources", L"resources\\audio",
        L"resources\\fonts", L"resources\\animations", L"resources\\plugins"};
    for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); ++i)
        GetResourceSubfolderPathUtf8(folders[i], NULL, 0);
}

void ExtractFileName(const char* path, char* name, size_t nameSize) {
    ExtractFileNameU8(path, name, nameSize);
}

void CheckAndCreateResourceFolders(void) {
    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, MAX_PATH);
    if (configPath[0] != '\0') EnsureDefaultResourceSubfolders();
}

BOOL IsFirstRun(void) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    if (!FileExists(configPath)) return TRUE;
    char firstRun[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "FIRST_RUN", "TRUE", firstRun,
                  sizeof(firstRun), configPath);
    return strcmp(firstRun, "TRUE") == 0;
}

BOOL SetFirstRunCompleted(void) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    return WriteIniString(INI_SECTION_GENERAL, "FIRST_RUN", "FALSE", configPath);
}

void GetAudioFolderPath(char* path, size_t size) {
    if (path && size) GetResourceSubfolderPathUtf8(L"resources\\audio", path, size);
}
void GetAnimationsFolderPath(char* path, size_t size) {
    if (path && size) GetResourceSubfolderPathUtf8(L"resources\\animations", path, size);
}
void GetPluginsFolderPath(char* path, size_t size) {
    if (path && size) GetResourceSubfolderPathUtf8(L"resources\\plugins", path, size);
}

BOOL IsShortcutCheckDone(void) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    return ReadIniBool(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", FALSE,
                       configPath);
}

BOOL SetShortcutCheckDone(BOOL done) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    return WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE",
                          done ? "TRUE" : "FALSE", configPath);
}
