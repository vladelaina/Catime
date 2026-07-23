#include "markdown/markdown_interactive.h"
#include "markdown_interactive_internal.h"
#include "plugin/plugin_data.h"
#include "utils/string_convert.h"
#include <stdlib.h>
#include <wchar.h>

#define CHECKBOX_OUTPUT_MAX_BYTES (1024ll * 1024ll)
#define CHECKBOX_OUTPUT_FILE_SHARE \
    (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)

static BOOL WriteDisplaySourceContentAtomicW(const wchar_t* filePath,
                                             const char* content,
                                             DWORD contentSize) {
    if (!filePath || !content) return FALSE;
    wchar_t tempDir[MAX_PATH] = {0};
    wchar_t tempPath[MAX_PATH] = {0};
    const wchar_t* lastSlash = wcsrchr(filePath, L'\\');
    if (!lastSlash || lastSlash == filePath) return FALSE;
    size_t dirLen = (size_t)(lastSlash - filePath + 1);
    if (dirLen >= _countof(tempDir)) return FALSE;
    wmemcpy(tempDir, filePath, dirLen);
    tempDir[dirLen] = L'\0';
    if (GetTempFileNameW(tempDir, L"cto", 0, tempPath) == 0) return FALSE;
    HANDLE file = CreateFileW(tempPath, GENERIC_WRITE,
                              CHECKBOX_OUTPUT_FILE_SHARE, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DeleteFileW(tempPath);
        return FALSE;
    }
    DWORD bytesWritten = 0;
    BOOL writeOk = WriteFile(file, content, contentSize, &bytesWritten, NULL) &&
                   bytesWritten == contentSize;
    if (writeOk && !FlushFileBuffers(file)) writeOk = FALSE;
    if (!CloseHandle(file)) writeOk = FALSE;
    if (!writeOk) {
        DeleteFileW(tempPath);
        return FALSE;
    }
    if (!MoveFileExW(tempPath, filePath,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tempPath);
        return FALSE;
    }
    return TRUE;
}

BOOL ToggleCheckboxInOutput(int index, HWND hwnd) {
    wchar_t filePath[MAX_PATH];
    if (!PluginData_GetDisplaySourcePath(filePath, MAX_PATH)) return FALSE;
    HANDLE file = CreateFileW(filePath, GENERIC_READ,
                              CHECKBOX_OUTPUT_FILE_SHARE, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER fileSizeValue;
    if (!GetFileSizeEx(file, &fileSizeValue) || fileSizeValue.QuadPart <= 0 ||
        fileSizeValue.QuadPart > CHECKBOX_OUTPUT_MAX_BYTES) {
        CloseHandle(file);
        return FALSE;
    }
    DWORD contentSize = (DWORD)fileSizeValue.QuadPart;
    char* content = (char*)malloc(contentSize + 1);
    if (!content) {
        CloseHandle(file);
        return FALSE;
    }
    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(file, content, contentSize, &bytesRead, NULL) &&
                  bytesRead == contentSize;
    CloseHandle(file);
    if (!readOk) {
        free(content);
        return FALSE;
    }
    content[bytesRead] = '\0';
    int currentIndex = 0;
    char* cursor = content;
    const char* end = content + bytesRead;
    BOOL modified = FALSE;
    while (end - cursor >= 6) {
        if ((cursor[0] == '-' || cursor[0] == '+' || cursor[0] == '*') &&
            cursor[1] == ' ' && cursor[2] == '[' &&
            (cursor[3] == ' ' || cursor[3] == 'x' || cursor[3] == 'X') &&
            cursor[4] == ']' && cursor[5] == ' ') {
            if (currentIndex == index) {
                cursor[3] = cursor[3] == ' ' ? 'x' : ' ';
                modified = TRUE;
                break;
            }
            currentIndex++;
        }
        cursor++;
    }
    if (modified && WriteDisplaySourceContentAtomicW(filePath, content, bytesRead)) {
        wchar_t* updatedText = Utf8ToWideAlloc(content);
        if (updatedText) {
            PluginData_SetPreviewTextWithSource(updatedText, filePath);
            free(updatedText);
        }
        if (MarkdownInteractive_IsValidWindow(hwnd))
            InvalidateRect(hwnd, NULL, TRUE);
    } else if (modified) {
        modified = FALSE;
    }
    free(content);
    return modified;
}
