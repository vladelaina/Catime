/**
 * @file plugin_data_cache.c
 * @brief Content and output-file state caches.
 */

#include "plugin_data_internal.h"

size_t ChooseLastContentCacheCapacity(size_t requiredSize) {
    size_t capacity = PLUGIN_OUTPUT_STACK_BUFFER_BYTES + 1;
    while (capacity < requiredSize && capacity < PLUGIN_LAST_CONTENT_RETAIN_BYTES) {
        capacity *= 2;
    }
    return capacity < requiredSize ? requiredSize : capacity;
}

BOOL UpdateLastContentCache(const char* content, DWORD contentSize) {
    size_t requiredSize = (size_t)contentSize + 1;
    if (requiredSize > g_lastContentCapacity ||
        (g_lastContentCapacity > PLUGIN_LAST_CONTENT_RETAIN_BYTES &&
         requiredSize <= PLUGIN_LAST_CONTENT_RETAIN_BYTES)) {
        size_t newCapacity = ChooseLastContentCacheCapacity(requiredSize);
        char* newBuf = (char*)realloc(g_lastContent, newCapacity);
        if (!newBuf) {
            LOG_ERROR("PluginData: Failed to resize last-content cache to %zu bytes", newCapacity);
            if (g_lastContent && g_lastContentCapacity > 0) {
                g_lastContent[0] = '\0';
            }
            g_lastContentSize = 0;
            return FALSE;
        }
        g_lastContent = newBuf;
        g_lastContentCapacity = newCapacity;
    }

    g_lastContentSize = requiredSize;
    memcpy(g_lastContent, content, contentSize);
    g_lastContent[contentSize] = '\0';
    return TRUE;
}

void ClearLastContentCacheLocked(void) {
    if (g_lastContentCapacity > PLUGIN_LAST_CONTENT_RETAIN_BYTES) {
        free(g_lastContent);
        g_lastContent = NULL;
        g_lastContentCapacity = 0;
        g_lastContentSize = 0;
        return;
    }
    if (g_lastContent && g_lastContentSize > 0) {
        g_lastContent[0] = '\0';
    }
    g_lastContentSize = 0;
}

BOOL ClearPluginDisplayDataLocked(void) {
    BOOL hadDisplayData = g_hasPluginData ||
                          (g_pluginDisplayText && g_pluginDisplayText[0] != L'\0');

    g_hasPluginData = FALSE;
    ClearPluginDisplayTextLocked();
    SetDisplaySourcePathLocked(NULL);
    SetPollIntervalMs(DEFAULT_POLL_INTERVAL_MS);

    return hadDisplayData;
}

void InvalidateLastOutputFileStateLocked(void) {
    ZeroMemory(&g_lastOutputWriteTime, sizeof(g_lastOutputWriteTime));
    g_lastOutputFileSize = 0;
    g_hasLastOutputFileState = FALSE;
}

void UpdateLastOutputFileStateLocked(const FILETIME* writeTime, ULONGLONG fileSize) {
    if (writeTime) {
        g_lastOutputWriteTime = *writeTime;
    } else {
        ZeroMemory(&g_lastOutputWriteTime, sizeof(g_lastOutputWriteTime));
    }
    g_lastOutputFileSize = fileSize;
    g_hasLastOutputFileState = TRUE;
}

void FreePluginDataBuffersLocked(void) {
    if (g_pluginDisplayText) {
        free(g_pluginDisplayText);
        g_pluginDisplayText = NULL;
    }
    g_pluginDisplayTextLen = 0;

    if (g_lastContent) {
        free(g_lastContent);
        g_lastContent = NULL;
    }
    g_lastContentSize = 0;
    g_lastContentCapacity = 0;
}

void ResetPluginDataStateLocked(void) {
    g_pluginModeActive = FALSE;
    g_hasPluginData = FALSE;
    FreePluginDataBuffersLocked();
    InvalidateLastOutputFileStateLocked();
    SetDisplaySourcePathLocked(NULL);
    ResetPendingNotificationLocked();
    SetDefaultPluginOutputDirectoryLocked();
}

BOOL CopyLastOutputFileStateLocked(FILETIME* writeTime, ULONGLONG* fileSize) {
    if (!g_hasLastOutputFileState || !writeTime || !fileSize) {
        return FALSE;
    }

    *writeTime = g_lastOutputWriteTime;
    *fileSize = g_lastOutputFileSize;
    return TRUE;
}

BOOL GetPluginOutputFileStateW(const wchar_t* filePath,
                                      FILETIME* writeTime,
                                      ULONGLONG* fileSize) {
    if (!filePath || !writeTime || !fileSize) return FALSE;

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(filePath, GetFileExInfoStandard, &data)) {
        return FALSE;
    }

    *writeTime = data.ftLastWriteTime;
    *fileSize = ((ULONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    return TRUE;
}
