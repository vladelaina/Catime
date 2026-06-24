/**
 * @file directory_watcher.c
 * @brief Lightweight Win32 directory change watcher helper
 */

#include "utils/directory_watcher.h"
#include "log.h"
#include <string.h>

#define DIRECTORY_WATCHER_REOPEN_DELAY_MS 1000u

static const char* DirectoryWatcherLabel(const DirectoryWatcher* watcher) {
    return (watcher && watcher->label) ? watcher->label : "DirectoryWatcher";
}

static HANDLE OpenDirectoryChangeHandle(const DirectoryWatcher* watcher) {
    if (!watcher) {
        return INVALID_HANDLE_VALUE;
    }
    return FindFirstChangeNotificationW(
        watcher->path,
        watcher->recursive,
        watcher->notifyFilter);
}

static void NotifyDirectoryWatcherCallback(DirectoryWatcher* watcher) {
    if (!watcher) {
        return;
    }

    DirectoryWatcherCallback callback = watcher->callback;
    if (callback) {
        callback(watcher->callbackContext);
    }
}

static HANDLE ReopenDirectoryChangeHandle(DirectoryWatcher* watcher) {
    if (!watcher || !watcher->stopEvent) {
        return INVALID_HANDLE_VALUE;
    }

    for (;;) {
        if (WaitForSingleObject(watcher->stopEvent,
                                DIRECTORY_WATCHER_REOPEN_DELAY_MS) == WAIT_OBJECT_0) {
            return INVALID_HANDLE_VALUE;
        }

        HANDLE changeHandle = OpenDirectoryChangeHandle(watcher);
        if (changeHandle != INVALID_HANDLE_VALUE) {
            LOG_INFO("%s: directory watch restored for '%ls'",
                     DirectoryWatcherLabel(watcher),
                     watcher->path);
            return changeHandle;
        }
    }
}

static DWORD WINAPI DirectoryWatcherThreadProc(LPVOID lpParam) {
    DirectoryWatcher* watcher = (DirectoryWatcher*)lpParam;
    if (!watcher || !watcher->stopEvent || watcher->path[0] == L'\0') {
        return 0;
    }

    HANDLE changeHandle = OpenDirectoryChangeHandle(watcher);
    if (changeHandle == INVALID_HANDLE_VALUE) {
        LOG_WARNING("%s: directory watch unavailable for '%ls' (error=%lu)",
                    DirectoryWatcherLabel(watcher),
                    watcher->path,
                    GetLastError());
        return 0;
    }

    HANDLE waitHandles[2] = { watcher->stopEvent, changeHandle };
    for (;;) {
        DWORD wait = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) {
            break;
        }
        if (wait == WAIT_FAILED) {
            LOG_WARNING("%s: directory watch wait failed (error=%lu)",
                        DirectoryWatcherLabel(watcher),
                        GetLastError());
            break;
        }
        if (wait != WAIT_OBJECT_0 + 1) {
            continue;
        }

        DWORD debounceMs = watcher->debounceMs
            ? watcher->debounceMs
            : DIRECTORY_WATCHER_DEFAULT_DEBOUNCE_MS;
        if (WaitForSingleObject(watcher->stopEvent, debounceMs) == WAIT_OBJECT_0) {
            break;
        }

        if (!FindNextChangeNotification(changeHandle)) {
            LOG_WARNING("%s: directory watch rearm failed (error=%lu)",
                        DirectoryWatcherLabel(watcher),
                        GetLastError());

            FindCloseChangeNotification(changeHandle);
            changeHandle = INVALID_HANDLE_VALUE;
            NotifyDirectoryWatcherCallback(watcher);

            changeHandle = ReopenDirectoryChangeHandle(watcher);
            if (changeHandle == INVALID_HANDLE_VALUE) {
                break;
            }
            waitHandles[1] = changeHandle;
            continue;
        }

        NotifyDirectoryWatcherCallback(watcher);
    }

    if (changeHandle != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(changeHandle);
    }
    return 0;
}

BOOL DirectoryWatcher_Start(DirectoryWatcher* watcher,
                            const wchar_t* directory,
                            BOOL recursive,
                            DWORD notifyFilter,
                            DWORD debounceMs,
                            DirectoryWatcherCallback callback,
                            void* callbackContext,
                            const char* label) {
    if (!watcher || !directory || directory[0] == L'\0' || !callback) {
        return FALSE;
    }

    if (watcher->thread) {
        return TRUE;
    }

    size_t pathLen = wcslen(directory);
    if (pathLen >= MAX_PATH) {
        LOG_WARNING("%s: directory path is too long", label ? label : "DirectoryWatcher");
        return FALSE;
    }

    HANDLE stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!stopEvent) {
        LOG_WARNING("%s: failed to create stop event (error=%lu)",
                    label ? label : "DirectoryWatcher",
                    GetLastError());
        return FALSE;
    }

    ZeroMemory(watcher, sizeof(*watcher));
    memcpy(watcher->path, directory, (pathLen + 1) * sizeof(wchar_t));
    watcher->recursive = recursive;
    watcher->notifyFilter = notifyFilter ? notifyFilter : DIRECTORY_WATCHER_DEFAULT_FILTER;
    watcher->debounceMs = debounceMs ? debounceMs : DIRECTORY_WATCHER_DEFAULT_DEBOUNCE_MS;
    watcher->callback = callback;
    watcher->callbackContext = callbackContext;
    watcher->label = label;
    watcher->stopEvent = stopEvent;

    HANDLE thread = CreateThread(NULL, 0, DirectoryWatcherThreadProc, watcher, 0, NULL);
    if (!thread) {
        LOG_WARNING("%s: failed to start directory watcher (error=%lu)",
                    DirectoryWatcherLabel(watcher),
                    GetLastError());
        CloseHandle(stopEvent);
        ZeroMemory(watcher, sizeof(*watcher));
        return FALSE;
    }

    watcher->thread = thread;
    return TRUE;
}

BOOL DirectoryWatcher_Stop(DirectoryWatcher* watcher, DWORD timeoutMs) {
    if (!watcher) {
        return FALSE;
    }

    HANDLE thread = watcher->thread;
    HANDLE stopEvent = watcher->stopEvent;
    if (!thread) {
        if (stopEvent) {
            CloseHandle(stopEvent);
        }
        ZeroMemory(watcher, sizeof(*watcher));
        return TRUE;
    }

    if (stopEvent) {
        SetEvent(stopEvent);
    }
    watcher->callback = NULL;

    DWORD wait = WaitForSingleObject(thread, timeoutMs);
    if (wait != WAIT_OBJECT_0) {
        LOG_WARNING("%s: directory watcher stop timed out (wait=%lu, error=%lu)",
                    DirectoryWatcherLabel(watcher),
                    wait,
                    GetLastError());
        return FALSE;
    }

    CloseHandle(thread);
    if (stopEvent) {
        CloseHandle(stopEvent);
    }
    ZeroMemory(watcher, sizeof(*watcher));
    return TRUE;
}
