#include "dialog_notification_audio_internal.h"

static int CompareSoundFileRows(const void* first, const void* second) {
    return NaturalCompareW((const wchar_t*)first, (const wchar_t*)second);
}

static int ScanNotificationSoundFiles(
    wchar_t files[][MAX_PATH], int capacity, LONG generation) {
    if (!files || capacity <= 0) {
        return NOTIFICATION_SOUND_SCAN_FAILED;
    }

    char audioPath[MAX_PATH] = {0};
    GetAudioFolderPath(audioPath, MAX_PATH);
    if (audioPath[0] == '\0') {
        return NOTIFICATION_SOUND_SCAN_FAILED;
    }

    wchar_t wideAudioPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, audioPath, -1,
                            wideAudioPath, MAX_PATH) <= 0) {
        return NOTIFICATION_SOUND_SCAN_FAILED;
    }

    wchar_t searchPath[MAX_PATH];
    if (_snwprintf_s(searchPath, MAX_PATH, _TRUNCATE,
                     L"%s\\*.*", wideAudioPath) < 0) {
        return NOTIFICATION_SOUND_SCAN_FAILED;
    }

    WIN32_FIND_DATAW findData;
    HANDLE findHandle = FindFirstFileW(searchPath, &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND ||
            error == ERROR_PATH_NOT_FOUND ||
            error == ERROR_NO_MORE_FILES) {
            return 0;
        }
        return NOTIFICATION_SOUND_SCAN_FAILED;
    }

    int fileCount = 0;
    int scannedEntries = 0;
    BOOL scanCanceled = FALSE;
    BOOL stoppedEarly = FALSE;
    do {
        if (NotificationAudio_IsScanCanceled(generation)) {
            scanCanceled = TRUE;
            stoppedEarly = TRUE;
            break;
        }
        if (++scannedEntries > NOTIFICATION_SOUND_SCAN_ENTRY_LIMIT ||
            fileCount >= capacity) {
            stoppedEarly = TRUE;
            break;
        }
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            !NotificationAudio_IsSupportedFileName(findData.cFileName)) {
            continue;
        }

        wcsncpy_s(files[fileCount], MAX_PATH, findData.cFileName, _TRUNCATE);
        fileCount++;
    } while (FindNextFileW(findHandle, &findData));

    DWORD findError = stoppedEarly ? ERROR_SUCCESS : GetLastError();
    FindClose(findHandle);
    if (scanCanceled || NotificationAudio_IsScanCanceled(generation) ||
        (!stoppedEarly && findError != ERROR_NO_MORE_FILES)) {
        return NOTIFICATION_SOUND_SCAN_FAILED;
    }

    if (fileCount > 1) {
        qsort(files, (size_t)fileCount, sizeof(files[0]), CompareSoundFileRows);
    }
    return fileCount;
}

static BOOL GetAudioFolderPathW(wchar_t* outPath, size_t outSize) {
    if (!outPath || outSize == 0 || outSize > INT_MAX) {
        return FALSE;
    }
    outPath[0] = L'\0';

    char audioPath[MAX_PATH] = {0};
    GetAudioFolderPath(audioPath, MAX_PATH);
    if (audioPath[0] == '\0') {
        return FALSE;
    }

    return MultiByteToWideChar(CP_UTF8, 0, audioPath, -1,
                               outPath, (int)outSize) > 0;
}

static void OnNotificationSoundFolderChanged(void* context) {
    (void)context;
    InterlockedExchange(&g_soundFileLastScanTick, 0);
    NotificationAudio_RequestCacheScanAsync();
}

void NotificationAudio_StartFolderWatcher(void) {
    wchar_t audioPath[MAX_PATH];
    if (!GetAudioFolderPathW(audioPath, MAX_PATH)) {
        OutputDebugStringW(
            L"NotificationSoundCache: failed to resolve audio folder watcher path\n");
        return;
    }

    DirectoryWatcher_Start(
        &g_soundFolderWatcher, audioPath, FALSE,
        DIRECTORY_WATCHER_DEFAULT_FILTER,
        DIRECTORY_WATCHER_DEFAULT_DEBOUNCE_MS,
        OnNotificationSoundFolderChanged, NULL,
        "NotificationSoundFolderWatcher");
}

void NotificationAudio_StopFolderWatcher(void) {
    DirectoryWatcher_Stop(
        &g_soundFolderWatcher, NOTIFICATION_SOUND_SCAN_STOP_TIMEOUT_MS);
}

DWORD WINAPI NotificationAudio_ScanThread(LPVOID parameter) {
    LONG generation = (LONG)(INT_PTR)parameter;
    wchar_t (*files)[MAX_PATH] = malloc(
        (size_t)NOTIFICATION_SOUND_ENTRY_LIMIT * sizeof(*files));
    if (!files) {
        if (!NotificationAudio_IsScanCanceled(generation)) {
            NotificationAudio_MarkCacheScanFailed();
        }
        return 0;
    }

    ZeroMemory(files,
               (size_t)NOTIFICATION_SOUND_ENTRY_LIMIT * sizeof(*files));
    int fileCount = ScanNotificationSoundFiles(
        files, NOTIFICATION_SOUND_ENTRY_LIMIT, generation);
    if (fileCount >= 0) {
        NotificationAudio_StoreCache(&files[0][0], fileCount, generation);
    } else if (!NotificationAudio_IsScanCanceled(generation)) {
        NotificationAudio_MarkCacheScanFailed();
    }
    free(files);
    return 0;
}
