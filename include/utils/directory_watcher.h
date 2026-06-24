/**
 * @file directory_watcher.h
 * @brief Lightweight Win32 directory change watcher helper
 */

#ifndef UTILS_DIRECTORY_WATCHER_H
#define UTILS_DIRECTORY_WATCHER_H

#include <windows.h>

#define DIRECTORY_WATCHER_DEFAULT_DEBOUNCE_MS 250u
#define DIRECTORY_WATCHER_DEFAULT_FILTER \
    (FILE_NOTIFY_CHANGE_FILE_NAME | \
     FILE_NOTIFY_CHANGE_DIR_NAME | \
     FILE_NOTIFY_CHANGE_LAST_WRITE | \
     FILE_NOTIFY_CHANGE_SIZE)

typedef void (*DirectoryWatcherCallback)(void* context);

typedef struct {
    HANDLE thread;
    HANDLE stopEvent;
    wchar_t path[MAX_PATH];
    BOOL recursive;
    DWORD notifyFilter;
    DWORD debounceMs;
    DirectoryWatcherCallback callback;
    void* callbackContext;
    const char* label;
} DirectoryWatcher;

BOOL DirectoryWatcher_Start(DirectoryWatcher* watcher,
                            const wchar_t* directory,
                            BOOL recursive,
                            DWORD notifyFilter,
                            DWORD debounceMs,
                            DirectoryWatcherCallback callback,
                            void* callbackContext,
                            const char* label);

BOOL DirectoryWatcher_Stop(DirectoryWatcher* watcher, DWORD timeoutMs);

#endif /* UTILS_DIRECTORY_WATCHER_H */
