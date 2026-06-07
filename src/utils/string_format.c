/**
 * @file string_format.c
 * @brief String formatting utilities implementation
 */

#include "utils/string_format.h"
#include <string.h>
#include <stdio.h>
#include <wchar.h>

/**
 * @brief Truncate long filenames for menu display
 * @param fileName Original filename
 * @param truncated Output buffer
 * @param maxLen Maximum display length
 * @note Uses middle truncation ("start...end.ext") for very long names
 */
void TruncateFileName(const wchar_t* fileName, wchar_t* truncated, size_t maxLen) {
    if (!truncated || maxLen == 0) return;
    truncated[0] = L'\0';
    if (!fileName) return;

    size_t nameLen = wcslen(fileName);
    if (maxLen <= 7) {
        size_t copyLen = nameLen < maxLen ? nameLen : maxLen;
        wmemcpy(truncated, fileName, copyLen);
        truncated[copyLen] = L'\0';
        return;
    }

    if (nameLen <= maxLen) {
        wcscpy_s(truncated, maxLen + 1, fileName);
        return;
    }

    size_t available = maxLen - 3;
    size_t prefixLen = (available + 1) / 2;
    size_t suffixLen = available - prefixLen;

    wmemcpy(truncated, fileName, prefixLen);
    wmemcpy(truncated + prefixLen, L"...", 3);
    wmemcpy(truncated + prefixLen + 3, fileName + nameLen - suffixLen, suffixLen);
    truncated[maxLen] = L'\0';
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

