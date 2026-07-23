/**
 * @file window_custom_text_io.c
 * @brief Custom text content paths, UTF-8 loading, and persistence.
 */

#include "window_commands_plugin_internal.h"

wchar_t* WindowPlugin_DuplicateWideString(const wchar_t* text) {
    const wchar_t* source = text ? text : L"";
    size_t len = wcslen(source);
    if (len > (SIZE_MAX / sizeof(wchar_t)) - 1) {
        return NULL;
    }

    wchar_t* copy = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (!copy) {
        return NULL;
    }

    memcpy(copy, source, (len + 1) * sizeof(wchar_t));
    return copy;
}

static size_t ClampUtf8ByteLength(const char* content, size_t contentLen) {
    if (!content || contentLen == 0) {
        return 0;
    }

    size_t len = contentLen;
    size_t seqStart = len;
    while (seqStart > 0 && (((unsigned char)content[seqStart - 1] & 0xC0u) == 0x80u)) {
        seqStart--;
    }

    if (seqStart == 0) {
        return 0;
    }

    if (seqStart == len) {
        unsigned char last = (unsigned char)content[len - 1];
        if ((last & 0x80u) == 0) {
            return len;
        }
        return ((last & 0xC0u) == 0xC0u) ? len - 1 : len;
    }

    unsigned char lead = (unsigned char)content[seqStart - 1];
    size_t expected = 0;
    if ((lead & 0xE0u) == 0xC0u) {
        expected = 2;
    } else if ((lead & 0xF0u) == 0xE0u) {
        expected = 3;
    } else if ((lead & 0xF8u) == 0xF0u) {
        expected = 4;
    } else {
        return (lead & 0x80u) == 0 ? seqStart : seqStart - 1;
    }

    return (seqStart - 1 + expected <= len) ? len : seqStart - 1;
}

static void ClampCustomTextDisplayText(wchar_t* text) {
    if (!text) {
        return;
    }

    size_t len = wcslen(text);
    if (len <= CUSTOM_TEXT_DISPLAY_MAX_CHARS) {
        return;
    }

    size_t limit = CUSTOM_TEXT_DISPLAY_MAX_CHARS;
    if (text[limit - 1] >= 0xD800 && text[limit - 1] <= 0xDBFF) {
        limit--;
    }
    text[limit] = L'\0';
}

static BOOL EnsureParentDirectoryExists(const wchar_t* filePath) {
    if (!filePath || filePath[0] == L'\0') {
        return FALSE;
    }

    wchar_t dirPath[MAX_PATH];
    wcsncpy_s(dirPath, _countof(dirPath), filePath, _TRUNCATE);

    wchar_t* lastSlash = wcsrchr(dirPath, L'\\');
    wchar_t* lastForwardSlash = wcsrchr(dirPath, L'/');
    if (!lastSlash || (lastForwardSlash && lastForwardSlash > lastSlash)) {
        lastSlash = lastForwardSlash;
    }
    if (!lastSlash || lastSlash == dirPath) {
        return FALSE;
    }

    *lastSlash = L'\0';
    int result = SHCreateDirectoryExW(NULL, dirPath, NULL);
    return result == ERROR_SUCCESS ||
           result == ERROR_ALREADY_EXISTS ||
           result == ERROR_FILE_EXISTS;
}

BOOL WindowPlugin_GetCustomTextDisplayPath(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }
    buffer[0] = L'\0';

    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] == '\0') {
        return FALSE;
    }

    wchar_t configPathW[MAX_PATH] = {0};
    if (!Utf8ToWide(configPath, configPathW, _countof(configPathW))) {
        return FALSE;
    }

    wchar_t* lastSlash = wcsrchr(configPathW, L'\\');
    wchar_t* lastForwardSlash = wcsrchr(configPathW, L'/');
    if (!lastSlash || (lastForwardSlash && lastForwardSlash > lastSlash)) {
        lastSlash = lastForwardSlash;
    }
    if (!lastSlash || lastSlash == configPathW) {
        return FALSE;
    }

    *lastSlash = L'\0';
    int written = _snwprintf_s(buffer, bufferSize, _TRUNCATE,
                               L"%s\\%s", configPathW,
                               CUSTOM_TEXT_DISPLAY_FILENAME_W);
    if (written < 0 || (size_t)written >= bufferSize) {
        buffer[0] = L'\0';
        return FALSE;
    }

    return TRUE;
}

wchar_t* WindowPlugin_LoadCustomTextDisplayContent(const wchar_t* filePath) {
    if (!filePath || filePath[0] == L'\0') {
        return WindowPlugin_DuplicateWideString(L"");
    }

    HANDLE hFile = CreateFileW(filePath, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            LOG_WARNING("Custom text display failed to open content file for reading: %lu", error);
        }
        return WindowPlugin_DuplicateWideString(L"");
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart <= 0) {
        CloseHandle(hFile);
        return WindowPlugin_DuplicateWideString(L"");
    }

    DWORD bytesToRead = fileSize.QuadPart > CUSTOM_TEXT_DISPLAY_MAX_FILE_BYTES
                            ? CUSTOM_TEXT_DISPLAY_MAX_FILE_BYTES
                            : (DWORD)fileSize.QuadPart;
    char* bytes = (char*)malloc((size_t)bytesToRead + 1);
    if (!bytes) {
        CloseHandle(hFile);
        return WindowPlugin_DuplicateWideString(L"");
    }

    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, bytes, bytesToRead, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!ok) {
        LOG_WARNING("Custom text display failed to read content file: %lu", GetLastError());
        free(bytes);
        return WindowPlugin_DuplicateWideString(L"");
    }

    size_t validBytes = ClampUtf8ByteLength(bytes, bytesRead);
    bytes[validBytes] = '\0';
    wchar_t* content = Utf8ToWideAlloc(bytes);
    free(bytes);
    if (!content) {
        return WindowPlugin_DuplicateWideString(L"");
    }

    ClampCustomTextDisplayText(content);
    return content;
}

BOOL WindowPlugin_GetCustomTextDisplayText(HWND hwndDlg, wchar_t** outText) {
    if (!outText) {
        return FALSE;
    }
    *outText = NULL;

    HWND hwndEdit = GetDlgItem(hwndDlg, IDC_CUSTOM_TEXT_DISPLAY_TEXT);
    if (!hwndEdit) {
        return FALSE;
    }

    int textLen = GetWindowTextLengthW(hwndEdit);
    if (textLen < 0 || textLen > CUSTOM_TEXT_DISPLAY_MAX_CHARS) {
        return FALSE;
    }

    wchar_t* text = (wchar_t*)malloc(((size_t)textLen + 1) * sizeof(wchar_t));
    if (!text) {
        return FALSE;
    }

    GetWindowTextW(hwndEdit, text, textLen + 1);
    *outText = text;
    return TRUE;
}

BOOL WindowPlugin_SaveCustomTextDisplayContent(const wchar_t* filePath, const wchar_t* text) {
    if (!filePath || filePath[0] == L'\0') {
        LOG_WARNING("Custom text display could not resolve content file path");
        return FALSE;
    }

    char* utf8 = WideToUtf8Alloc(text ? text : L"");
    if (!utf8) {
        LOG_WARNING("Custom text display failed to convert content to UTF-8");
        return FALSE;
    }

    if (!EnsureParentDirectoryExists(filePath)) {
        LOG_WARNING("Custom text display could not ensure content directory exists");
        free(utf8);
        return FALSE;
    }

    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_WARNING("Custom text display failed to open content file for writing: %lu", GetLastError());
        free(utf8);
        return FALSE;
    }

    DWORD bytesToWrite = (DWORD)strlen(utf8);
    DWORD bytesWritten = 0;
    BOOL ok = bytesToWrite == 0 ||
              WriteFile(hFile, utf8, bytesToWrite, &bytesWritten, NULL);
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(hFile);
    free(utf8);

    if (!ok || bytesWritten != bytesToWrite) {
        LOG_WARNING("Custom text display failed to write content file: %lu", error);
        return FALSE;
    }

    return TRUE;
}
