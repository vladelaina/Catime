/**
 * @file string_format.c
 * @brief String formatting utilities implementation
 */

#include "utils/string_format.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief Truncate long filenames for menu display
 * @param fileName Original filename
 * @param truncated Output buffer
 * @param maxLen Maximum display length
 * @note Uses middle truncation ("start...end.ext") for very long names
 */
void TruncateFileName(const wchar_t* fileName, wchar_t* truncated, size_t maxLen) {
    if (!fileName || !truncated || maxLen <= 7) return;
    
    size_t nameLen = wcslen(fileName);
    if (nameLen <= maxLen) {
        wcscpy_s(truncated, maxLen + 1, fileName);
        return;
    }
    
    const wchar_t* lastDot = wcsrchr(fileName, L'.');
    const wchar_t* fileNameNoExt = fileName;
    const wchar_t* ext = L"";
    size_t nameNoExtLen = nameLen;
    size_t extLen = 0;
    
    if (lastDot && lastDot != fileName) {
        ext = lastDot;
        extLen = wcslen(ext);
        nameNoExtLen = lastDot - fileName;
    }
    
    if (nameNoExtLen <= 27) {
        wcsncpy(truncated, fileName, maxLen - extLen - 3);
        truncated[maxLen - extLen - 3] = L'\0';
        wcscat_s(truncated, maxLen + 1, L"...");
        wcscat_s(truncated, maxLen + 1, ext);
        return;
    }
    
    wchar_t buffer[MAX_PATH];
    
    wcsncpy(buffer, fileName, 12);
    buffer[12] = L'\0';
    
    wcscat_s(buffer, MAX_PATH, L"...");
    
    wcsncat(buffer, fileName + nameNoExtLen - 12, 12);
    
    wcscat_s(buffer, MAX_PATH, ext);
    
    wcscpy_s(truncated, maxLen + 1, buffer);
}

/**
 * @brief Format time for Pomodoro menu display
 * @param seconds Duration in seconds
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @note Shows h:mm:ss if >1hr, mm:ss if has seconds, or just minutes
 */
void FormatPomodoroTime(int seconds, wchar_t* buffer, size_t bufferSize) {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    int hours = minutes / 60;
    minutes %= 60;
    
    if (hours > 0) {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d:%02d", hours, minutes, secs);
    } else if (secs == 0) {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d", minutes);
    } else {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d", minutes, secs);
    }
}

