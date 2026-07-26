/**
 * @file plugin_data_file.c
 * @brief Reading, debouncing, and applying output.txt changes.
 */

#include "plugin_data_internal.h"

BOOL ProcessPluginOutputFile(const wchar_t* filePath, BOOL forceRefresh,
                                    FILETIME* lastWriteTime, ULONGLONG* lastFileSize) {
    if (!filePath || !lastWriteTime || !lastFileSize) {
        return FALSE;
    }

    HANDLE hFile = CreateFileW(
        filePath,
        GENERIC_READ,
        PLUGIN_OUTPUT_FILE_SHARE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            ZeroMemory(lastWriteTime, sizeof(*lastWriteTime));
            *lastFileSize = 0;

            EnterCriticalSection(&g_dataCS);
            BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();
            BOOL displayChanged = ClearPluginDisplayDataLocked();
            BOOL displayTimerRecheck = hadCatimeTag != PluginDisplayHasCatimeTagLocked();
            ClearLastContentCacheLocked();
            InvalidateLastOutputFileStateLocked();
            LeaveCriticalSection(&g_dataCS);
            if (displayTimerRecheck) {
                QueuePluginDataTimerRecheck();
            }
            if ((displayChanged || displayTimerRecheck) && g_hNotifyWnd) {
                RequestPluginDataRedraw(g_hNotifyWnd);
            }
            return displayChanged;
        }
        return FALSE;
    }

    FILETIME currentWriteTime = {0};
    GetFileTime(hFile, NULL, NULL, &currentWriteTime);

    LARGE_INTEGER sizeValue;
    if (!GetFileSizeEx(hFile, &sizeValue) || sizeValue.QuadPart <= 0) {
        *lastWriteTime = currentWriteTime;
        *lastFileSize = 0;
        EnterCriticalSection(&g_dataCS);
        BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();
        BOOL displayChanged = ClearPluginDisplayDataLocked();
        BOOL timerRecheck = hadCatimeTag != PluginDisplayHasCatimeTagLocked();
        ClearLastContentCacheLocked();
        UpdateLastOutputFileStateLocked(&currentWriteTime, 0);
        LeaveCriticalSection(&g_dataCS);
        CloseHandle(hFile);
        if (timerRecheck) {
            QueuePluginDataTimerRecheck();
        }
        if ((displayChanged || timerRecheck) && g_hNotifyWnd) {
            RequestPluginDataRedraw(g_hNotifyWnd);
        }
        return displayChanged;
    }

    ULONGLONG fileSize64 = (ULONGLONG)sizeValue.QuadPart;
    if (!forceRefresh &&
        CompareFileTime(&currentWriteTime, lastWriteTime) == 0 &&
        fileSize64 == *lastFileSize) {
        CloseHandle(hFile);
        return FALSE;
    }

    if (fileSize64 > MAX_PLUGIN_OUTPUT_BYTES) {
        LOG_WARNING("PluginData: Skipping output.txt larger than %llu bytes",
                    (ULONGLONG)MAX_PLUGIN_OUTPUT_BYTES);
        *lastWriteTime = currentWriteTime;
        *lastFileSize = fileSize64;
        EnterCriticalSection(&g_dataCS);
        BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();
        BOOL displayChanged = ClearPluginDisplayDataLocked();
        BOOL timerRecheck = hadCatimeTag != PluginDisplayHasCatimeTagLocked();
        ClearLastContentCacheLocked();
        UpdateLastOutputFileStateLocked(&currentWriteTime, *lastFileSize);
        LeaveCriticalSection(&g_dataCS);
        CloseHandle(hFile);
        if (timerRecheck) {
            QueuePluginDataTimerRecheck();
        }
        if ((displayChanged || timerRecheck) && g_hNotifyWnd) {
            RequestPluginDataRedraw(g_hNotifyWnd);
        }
        return displayChanged;
    }

    DWORD fileSize = (DWORD)fileSize64;

    DWORD bytesToRead = fileSize;
    if (bytesToRead > PLUGIN_OUTPUT_READ_LIMIT_BYTES) {
        bytesToRead = PLUGIN_OUTPUT_READ_LIMIT_BYTES;
    }

    char stackContent[PLUGIN_OUTPUT_STACK_BUFFER_BYTES + 1];
    char* heapContent = NULL;
    char* currentContent = stackContent;
    if (bytesToRead > PLUGIN_OUTPUT_STACK_BUFFER_BYTES) {
        heapContent = (char*)malloc((size_t)bytesToRead + 1);
        if (!heapContent) {
            CloseHandle(hFile);
            return FALSE;
        }
        currentContent = heapContent;
    }

    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(hFile, currentContent, bytesToRead, &bytesRead, NULL) && bytesRead > 0;
    CloseHandle(hFile);

    if (!readOk) {
        free(heapContent);
        return FALSE;
    }

    currentContent[bytesRead] = '\0';
    EnterCriticalSection(&g_dataCS);
    BOOL contentChanged = g_lastContent == NULL ||
                          g_lastContentSize != (size_t)bytesRead + 1 ||
                          memcmp(currentContent, g_lastContent, (size_t)bytesRead + 1) != 0;

    LeaveCriticalSection(&g_dataCS);

    if (!contentChanged) {
        *lastWriteTime = currentWriteTime;
        *lastFileSize = fileSize;
        EnterCriticalSection(&g_dataCS);
        SetDisplaySourcePathLocked(filePath);
        UpdateLastOutputFileStateLocked(&currentWriteTime, fileSize);
        LeaveCriticalSection(&g_dataCS);
    } else {
        BOOL displayChanged = FALSE;
        BOOL timerRecheck = FALSE;
        PluginParseResult parseResult =
            ParseContent(currentContent, bytesRead, FALSE, &displayChanged, &timerRecheck);
        if (parseResult == PLUGIN_PARSE_OK) {
            *lastWriteTime = currentWriteTime;
            *lastFileSize = fileSize;
            EnterCriticalSection(&g_dataCS);
            SetDisplaySourcePathLocked(filePath);
            UpdateLastContentCache(currentContent, bytesRead);
            UpdateLastOutputFileStateLocked(&currentWriteTime, fileSize);
            LeaveCriticalSection(&g_dataCS);

            if (timerRecheck) {
                QueuePluginDataTimerRecheck();
            }
            if ((displayChanged || timerRecheck) && g_hNotifyWnd) {
                RequestPluginDataRedraw(g_hNotifyWnd);
            }
        } else if (parseResult == PLUGIN_PARSE_FAILED) {
            EnterCriticalSection(&g_dataCS);
            BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();
            BOOL clearedDisplayChanged = ClearPluginDisplayDataLocked();
            BOOL displayTimerRecheck = hadCatimeTag != PluginDisplayHasCatimeTagLocked();
            ClearLastContentCacheLocked();
            UpdateLastOutputFileStateLocked(&currentWriteTime, fileSize);
            LeaveCriticalSection(&g_dataCS);
            *lastWriteTime = currentWriteTime;
            *lastFileSize = fileSize;

            if (displayTimerRecheck) {
                QueuePluginDataTimerRecheck();
            }
            if ((clearedDisplayChanged || displayTimerRecheck) && g_hNotifyWnd) {
                RequestPluginDataRedraw(g_hNotifyWnd);
            }
        } else {
            contentChanged = FALSE;
        }
    }

    free(heapContent);
    return contentChanged;
}
