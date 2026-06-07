/**
 * @file dialog_notification_audio.c
 * @brief Audio file management for notification settings
 */

#include "dialog/dialog_notification_audio.h"
#include "config.h"
#include "language.h"
#include "audio_player.h"
#include "utils/natural_sort.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define NOTIFICATION_SOUND_ENTRY_LIMIT 256
#define NOTIFICATION_SOUND_SCAN_ENTRY_LIMIT 4096
#define NOTIFICATION_SOUND_SCAN_STOP_TIMEOUT_MS 2000
#define NOTIFICATION_SOUND_SCAN_REFRESH_COOLDOWN_MS 2000
#define NOTIFICATION_SOUND_SCAN_FOREGROUND_WAIT_MS 250
#define NOTIFICATION_SOUND_SCAN_FAILED (-1)

static wchar_t g_soundFileCache[NOTIFICATION_SOUND_ENTRY_LIMIT][MAX_PATH];
static int g_soundFileCacheCount = 0;
static BOOL g_soundFileCacheReady = FALSE;
static BOOL g_soundFileCacheFailed = FALSE;
static SRWLOCK g_soundFileCacheLock = SRWLOCK_INIT;
static SRWLOCK g_soundScanThreadLock = SRWLOCK_INIT;
static HANDLE g_hSoundScanThread = NULL;
static volatile LONG g_soundScanShuttingDown = 0;
static volatile LONG g_soundFileLastScanTick = 0;

/* ============================================================================
 * Static Helper Functions
 * ============================================================================ */

static BOOL IsSoundScanShuttingDown(void) {
    return InterlockedCompareExchange(&g_soundScanShuttingDown, 0, 0) != 0;
}

static BOOL IsSoundFileCacheRecentlyScanned(DWORD now) {
    DWORD lastScanTick = (DWORD)InterlockedCompareExchange(&g_soundFileLastScanTick, 0, 0);
    if (lastScanTick == 0 ||
        (DWORD)(now - lastScanTick) >= NOTIFICATION_SOUND_SCAN_REFRESH_COOLDOWN_MS) {
        return FALSE;
    }

    AcquireSRWLockShared(&g_soundFileCacheLock);
    BOOL recentlyScanned = g_soundFileCacheReady || g_soundFileCacheFailed;
    ReleaseSRWLockShared(&g_soundFileCacheLock);
    return recentlyScanned;
}

static BOOL IsSupportedAudioFileName(const wchar_t* fileName) {
    const wchar_t* ext = fileName ? wcsrchr(fileName, L'.') : NULL;
    return ext && (
        _wcsicmp(ext, L".flac") == 0 ||
        _wcsicmp(ext, L".mp3") == 0 ||
        _wcsicmp(ext, L".wav") == 0
    );
}

static BOOL GetCurrentSoundFileName(const char* currentFile, wchar_t* outFileName, size_t outSize) {
    if (!currentFile || currentFile[0] == '\0' || !outFileName || outSize == 0) {
        return FALSE;
    }

    wchar_t wSoundFile[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, currentFile, -1, wSoundFile, MAX_PATH) <= 0) {
        return FALSE;
    }

    wchar_t* fileName = wcsrchr(wSoundFile, L'\\');
    fileName = fileName ? fileName + 1 : wSoundFile;
    if (fileName[0] == L'\0') {
        return FALSE;
    }

    wcsncpy_s(outFileName, outSize, fileName, _TRUNCATE);
    return TRUE;
}

static int CompareSoundFileRows(const void* a, const void* b) {
    const wchar_t* fileA = (const wchar_t*)a;
    const wchar_t* fileB = (const wchar_t*)b;
    return NaturalCompareW(fileA, fileB);
}

static int ScanNotificationSoundFiles(wchar_t files[][MAX_PATH], int capacity) {
    if (!files || capacity <= 0) return NOTIFICATION_SOUND_SCAN_FAILED;

    char audio_path[MAX_PATH] = {0};
    GetAudioFolderPath(audio_path, MAX_PATH);
    if (audio_path[0] == '\0') {
        return NOTIFICATION_SOUND_SCAN_FAILED;
    }

    wchar_t wAudioPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH) <= 0) {
        return NOTIFICATION_SOUND_SCAN_FAILED;
    }

    wchar_t wSearchPath[MAX_PATH];
    int searchPathLen = _snwprintf_s(wSearchPath, MAX_PATH, _TRUNCATE,
                                     L"%s\\*.*", wAudioPath);
    if (searchPathLen < 0) {
        return NOTIFICATION_SOUND_SCAN_FAILED;
    }

    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(wSearchPath, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
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
    BOOL scanCancelled = FALSE;
    BOOL stoppedEarly = FALSE;
    do {
        if (IsSoundScanShuttingDown()) {
            scanCancelled = TRUE;
            stoppedEarly = TRUE;
            break;
        }
        if (++scannedEntries > NOTIFICATION_SOUND_SCAN_ENTRY_LIMIT) {
            stoppedEarly = TRUE;
            break;
        }
        if (fileCount >= capacity) {
            stoppedEarly = TRUE;
            break;
        }
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        if (!IsSupportedAudioFileName(find_data.cFileName)) {
            continue;
        }

        wcsncpy_s(files[fileCount], MAX_PATH, find_data.cFileName, _TRUNCATE);
        fileCount++;
    } while (FindNextFileW(hFind, &find_data));

    DWORD findError = stoppedEarly ? ERROR_SUCCESS : GetLastError();
    FindClose(hFind);

    if (scanCancelled || IsSoundScanShuttingDown()) {
        return NOTIFICATION_SOUND_SCAN_FAILED;
    }
    if (!stoppedEarly && findError != ERROR_NO_MORE_FILES) {
        return NOTIFICATION_SOUND_SCAN_FAILED;
    }

    if (fileCount > 1) {
        qsort(files, (size_t)fileCount, sizeof(files[0]), CompareSoundFileRows);
    }

    return fileCount;
}

static BOOL StoreNotificationSoundCache(wchar_t files[][MAX_PATH], int fileCount) {
    if (IsSoundScanShuttingDown()) {
        return FALSE;
    }

    if (!files || fileCount < 0) {
        return FALSE;
    }
    if (fileCount > NOTIFICATION_SOUND_ENTRY_LIMIT) {
        fileCount = NOTIFICATION_SOUND_ENTRY_LIMIT;
    }

    AcquireSRWLockExclusive(&g_soundFileCacheLock);
    if (IsSoundScanShuttingDown()) {
        ReleaseSRWLockExclusive(&g_soundFileCacheLock);
        return FALSE;
    }

    ZeroMemory(g_soundFileCache, sizeof(g_soundFileCache));
    if (fileCount > 0) {
        memcpy(g_soundFileCache, files,
               (size_t)fileCount * MAX_PATH * sizeof(wchar_t));
    }
    g_soundFileCacheCount = fileCount;
    g_soundFileCacheReady = TRUE;
    g_soundFileCacheFailed = FALSE;
    ReleaseSRWLockExclusive(&g_soundFileCacheLock);

    InterlockedExchange(&g_soundFileLastScanTick, (LONG)GetTickCount());
    return TRUE;
}

static void MarkNotificationSoundCacheScanFailed(void) {
    AcquireSRWLockExclusive(&g_soundFileCacheLock);
    if (!IsSoundScanShuttingDown()) {
        ZeroMemory(g_soundFileCache, sizeof(g_soundFileCache));
        g_soundFileCacheCount = 0;
        g_soundFileCacheReady = FALSE;
        g_soundFileCacheFailed = TRUE;
        InterlockedExchange(&g_soundFileLastScanTick, (LONG)GetTickCount());
    }
    ReleaseSRWLockExclusive(&g_soundFileCacheLock);
}

static BOOL CloseCompletedSoundScanThreadLocked(DWORD waitMs) {
    if (!g_hSoundScanThread) {
        return TRUE;
    }

    DWORD wait = WaitForSingleObject(g_hSoundScanThread, waitMs);
    if (wait == WAIT_OBJECT_0) {
        CloseHandle(g_hSoundScanThread);
        g_hSoundScanThread = NULL;
        return TRUE;
    }

    if (wait != WAIT_TIMEOUT) {
        OutputDebugStringW(L"NotificationSoundCache: sound scan wait failed\n");
    }
    return FALSE;
}

static BOOL RefreshNotificationSoundCacheNow(BOOL forceRefresh) {
    BOOL refreshed = FALSE;
    DWORD now = GetTickCount();

    AcquireSRWLockExclusive(&g_soundScanThreadLock);
    if (IsSoundScanShuttingDown()) {
        ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        return FALSE;
    }

    if (!CloseCompletedSoundScanThreadLocked(NOTIFICATION_SOUND_SCAN_FOREGROUND_WAIT_MS)) {
        ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        return FALSE;
    }

    if (!forceRefresh && IsSoundFileCacheRecentlyScanned(now)) {
        ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        return TRUE;
    }

    wchar_t (*files)[MAX_PATH] = (wchar_t (*)[MAX_PATH])malloc(
        (size_t)NOTIFICATION_SOUND_ENTRY_LIMIT * sizeof(*files));
    if (!files) {
        MarkNotificationSoundCacheScanFailed();
        ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        return FALSE;
    }

    ZeroMemory(files, (size_t)NOTIFICATION_SOUND_ENTRY_LIMIT * sizeof(*files));
    int fileCount = ScanNotificationSoundFiles(files, NOTIFICATION_SOUND_ENTRY_LIMIT);
    if (fileCount >= 0) {
        refreshed = StoreNotificationSoundCache(files, fileCount);
    } else {
        MarkNotificationSoundCacheScanFailed();
    }

    free(files);
    ReleaseSRWLockExclusive(&g_soundScanThreadLock);
    return refreshed;
}

static DWORD WINAPI NotificationSoundScanThread(LPVOID lpParam) {
    (void)lpParam;

    wchar_t (*files)[MAX_PATH] = (wchar_t (*)[MAX_PATH])malloc(
        (size_t)NOTIFICATION_SOUND_ENTRY_LIMIT * sizeof(*files));
    if (!files) {
        MarkNotificationSoundCacheScanFailed();
        return 0;
    }

    ZeroMemory(files, (size_t)NOTIFICATION_SOUND_ENTRY_LIMIT * sizeof(*files));
    int fileCount = ScanNotificationSoundFiles(files, NOTIFICATION_SOUND_ENTRY_LIMIT);

    if (fileCount >= 0) {
        StoreNotificationSoundCache(files, fileCount);
    } else {
        MarkNotificationSoundCacheScanFailed();
    }
    free(files);
    return 0;
}

static int CopyNotificationSoundCache(wchar_t files[][MAX_PATH], int capacity, BOOL* cacheReady) {
    int count = 0;

    if (cacheReady) {
        *cacheReady = FALSE;
    }
    if (!files || capacity <= 0) return 0;

    AcquireSRWLockShared(&g_soundFileCacheLock);
    if (cacheReady) {
        *cacheReady = g_soundFileCacheReady || g_soundFileCacheFailed;
    }
    count = g_soundFileCacheCount;
    if (count > capacity) {
        count = capacity;
    }
    if (count > 0) {
        memcpy(files, g_soundFileCache, (size_t)count * MAX_PATH * sizeof(wchar_t));
    }
    ReleaseSRWLockShared(&g_soundFileCacheLock);

    return count;
}

void NotificationSoundCache_RequestScanAsync(void) {
    AcquireSRWLockExclusive(&g_soundScanThreadLock);

    if (IsSoundScanShuttingDown()) {
        ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        return;
    }

    if (!CloseCompletedSoundScanThreadLocked(0)) {
        ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        return;
    }

    if (IsSoundFileCacheRecentlyScanned(GetTickCount())) {
        ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, NotificationSoundScanThread, NULL, 0, NULL);
    if (hThread) {
        g_hSoundScanThread = hThread;
    } else {
        MarkNotificationSoundCacheScanFailed();
    }

    ReleaseSRWLockExclusive(&g_soundScanThreadLock);
}

void NotificationSoundCache_Shutdown(void) {
    HANDLE hThread = NULL;

    InterlockedExchange(&g_soundScanShuttingDown, 1);

    AcquireSRWLockExclusive(&g_soundScanThreadLock);
    if (g_hSoundScanThread) {
        hThread = g_hSoundScanThread;
        g_hSoundScanThread = NULL;
    }
    ReleaseSRWLockExclusive(&g_soundScanThreadLock);

    if (hThread) {
        DWORD wait = WaitForSingleObject(hThread, NOTIFICATION_SOUND_SCAN_STOP_TIMEOUT_MS);
        if (wait != WAIT_OBJECT_0) {
            OutputDebugStringW(L"NotificationSoundCache: sound scan stop timed out\n");
        }
        CloseHandle(hThread);
    }

    AcquireSRWLockExclusive(&g_soundFileCacheLock);
    ZeroMemory(g_soundFileCache, sizeof(g_soundFileCache));
    g_soundFileCacheCount = 0;
    g_soundFileCacheReady = FALSE;
    g_soundFileCacheFailed = FALSE;
    ReleaseSRWLockExclusive(&g_soundFileCacheLock);
    InterlockedExchange(&g_soundFileLastScanTick, 0);
}

/**
 * @brief Audio playback completion callback
 * @param hwnd Dialog window handle
 */
static void OnAudioPlaybackComplete(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        PostMessage(hwnd, WM_NOTIFICATION_SOUND_PLAYBACK_COMPLETE, 0, 0);
    }
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void PopulateNotificationSoundComboBox(HWND hwndCombo, const char* currentFile) {
    if (!hwndCombo) return;

    NotificationSoundCache_RequestScanAsync();

    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0);
    SendMessage(hwndCombo, CB_INITSTORAGE, NOTIFICATION_SOUND_ENTRY_LIMIT + 3,
                (LPARAM)((NOTIFICATION_SOUND_ENTRY_LIMIT + 3) * MAX_PATH * sizeof(wchar_t)));

    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(NULL, L"None"));
    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(NULL, L"System Beep"));

    wchar_t currentFileName[MAX_PATH] = {0};
    if (currentFile && strcmp(currentFile, "SYSTEM_BEEP") != 0) {
        GetCurrentSoundFileName(currentFile, currentFileName, MAX_PATH);
    }

    wchar_t (*soundFiles)[MAX_PATH] = (wchar_t (*)[MAX_PATH])malloc(
        (size_t)NOTIFICATION_SOUND_ENTRY_LIMIT * sizeof(*soundFiles));
    if (soundFiles) {
        ZeroMemory(soundFiles, (size_t)NOTIFICATION_SOUND_ENTRY_LIMIT * sizeof(*soundFiles));
        int soundCount = CopyNotificationSoundCache(soundFiles, NOTIFICATION_SOUND_ENTRY_LIMIT,
                                                    NULL);

        for (int i = 0; i < soundCount; i++) {
            SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)soundFiles[i]);
        }
        free(soundFiles);
    }

    if (currentFile && currentFile[0] != '\0') {
        if (strcmp(currentFile, "SYSTEM_BEEP") == 0) {
            SendMessage(hwndCombo, CB_SETCURSEL, 1, 0);
        } else {
            LRESULT index = CB_ERR;
            if (currentFileName[0] != L'\0') {
                index = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)currentFileName);
                if (index == CB_ERR && IsSupportedAudioFileName(currentFileName)) {
                    index = SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)currentFileName);
                }
            }

            if (index != CB_ERR) {
                SendMessage(hwndCombo, CB_SETCURSEL, index, 0);
            } else {
                SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
            }
        }
    } else {
        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
    }
}

BOOL GetSelectedNotificationSoundFile(HWND hwndCombo, char* outSoundFile, size_t outSize) {
    if (!hwndCombo || !outSoundFile || outSize == 0) return FALSE;

    outSoundFile[0] = '\0';

    LRESULT index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR || index <= 0) {
        return TRUE;
    }

    LRESULT textLen = SendMessageW(hwndCombo, CB_GETLBTEXTLEN, (WPARAM)index, 0);
    if (textLen == CB_ERR || textLen < 0 || textLen >= MAX_PATH) {
        return FALSE;
    }

    wchar_t wFileName[MAX_PATH] = {0};
    if (SendMessageW(hwndCombo, CB_GETLBTEXT, (WPARAM)index, (LPARAM)wFileName) == CB_ERR) {
        return FALSE;
    }
    wFileName[MAX_PATH - 1] = L'\0';

    const wchar_t* sysBeepText = GetLocalizedString(NULL, L"System Beep");
    if (sysBeepText && wcscmp(wFileName, sysBeepText) == 0) {
        int written = snprintf(outSoundFile, outSize, "%s", "SYSTEM_BEEP");
        return written >= 0 && (size_t)written < outSize;
    }

    char audio_path[MAX_PATH] = {0};
    GetAudioFolderPath(audio_path, MAX_PATH);
    if (audio_path[0] == '\0') {
        return FALSE;
    }

    char fileName[MAX_PATH] = {0};
    if (WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL) <= 0) {
        return FALSE;
    }

    int pathLen = snprintf(outSoundFile, outSize, "%s\\%s", audio_path, fileName);
    if (pathLen < 0 || (size_t)pathLen >= outSize) {
        outSoundFile[0] = '\0';
        return FALSE;
    }

    return TRUE;
}

BOOL HandleSoundTestButton(HWND hwndDlg, HWND hwndCombo, HWND hwndSlider, BOOL* isPlaying) {
    if (!hwndDlg || !hwndCombo || !hwndSlider || !isPlaying) return FALSE;

    if (!(*isPlaying)) {
        char soundFile[MAX_PATH] = {0};
        if (!GetSelectedNotificationSoundFile(hwndCombo, soundFile, sizeof(soundFile))) {
            return FALSE;
        }

        if (soundFile[0] != '\0') {
            int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
            SetAudioVolume(volume);

            if (PlayNotificationSoundFile(hwndDlg, soundFile)) {
                SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(NULL, L"Stop"));
                *isPlaying = TRUE;
            }
        }
    } else {
        StopNotificationSound();
        SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(NULL, L"Test"));
        *isPlaying = FALSE;
    }
    
    return TRUE;
}

void HandleSoundDirButton(HWND hwndDlg, HWND hwndCombo) {
    if (!hwndDlg || !hwndCombo) return;

    char audio_path[MAX_PATH] = {0};
    GetAudioFolderPath(audio_path, MAX_PATH);
    if (audio_path[0] == '\0') {
        return;
    }

    wchar_t wAudioPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH) <= 0) {
        return;
    }

    ShellExecuteW(hwndDlg, L"open", wAudioPath, NULL, NULL, SW_SHOWNORMAL);
    
    int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
    wchar_t selectedFile[MAX_PATH] = {0};
    if (selectedIndex > 0) {
        SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
    }
    
    const char* currentFile = (selectedIndex > 0) ? g_AppConfig.notification.sound.sound_file : NULL;
    RefreshNotificationSoundCacheNow(TRUE);
    PopulateNotificationSoundComboBox(hwndCombo, currentFile);
    
    if (selectedFile[0] != L'\0') {
        LRESULT newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)selectedFile);
        if (newIndex != CB_ERR) {
            SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
        } else {
            SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
        }
    }
}

void HandleSoundComboDropdown(HWND hwndCombo) {
    if (!hwndCombo) return;

    int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
    wchar_t selectedFile[MAX_PATH] = {0};
    if (selectedIndex > 0) {
        SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
    }
    
    const char* currentFile = (selectedIndex > 0) ? g_AppConfig.notification.sound.sound_file : NULL;
    RefreshNotificationSoundCacheNow(FALSE);
    PopulateNotificationSoundComboBox(hwndCombo, currentFile);
    
    if (selectedFile[0] != L'\0') {
        LRESULT newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)selectedFile);
        if (newIndex != CB_ERR) {
            SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
        }
    }
}

void SetupAudioPlaybackCallback(HWND hwndDlg) {
    SetAudioPlaybackCompleteCallback(hwndDlg, OnAudioPlaybackComplete);
}

void CleanupAudioPlayback(BOOL isPlaying) {
    if (isPlaying) {
        StopNotificationSound();
    }
    SetAudioPlaybackCompleteCallback(NULL, NULL);
}

