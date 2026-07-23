#include "audio_player_internal.h"

BOOL IsCurrentProcessAudioWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return FALSE;
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    return processId == GetCurrentProcessId();
}

BOOL GetWideCharPath(
    const char* utf8Path, wchar_t* widePath, size_t widePathSize) {
    if (!widePath || widePathSize == 0 || widePathSize > INT_MAX) return FALSE;
    widePath[0] = L'\0';
    if (!utf8Path || MultiByteToWideChar(
            CP_UTF8, 0, utf8Path, -1,
            widePath, (int)widePathSize) <= 0) {
        widePath[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

BOOL GetAudioFileInfo(const char* filePath, AudioFileInfo* info) {
    if (!filePath || filePath[0] == '\0' || !info) return FALSE;
    info->path[0] = L'\0';
    info->sizeBytes = 0;
    if (!GetWideCharPath(filePath, info->path, _countof(info->path))) return FALSE;
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if (!GetFileAttributesExW(
            info->path, GetFileExInfoStandard, &attributes) ||
        (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return FALSE;
    info->sizeBytes = ((ULONGLONG)attributes.nFileSizeHigh << 32) |
                      attributes.nFileSizeLow;
    return TRUE;
}

BOOL IsAudioFileSizeAllowed(const char* filePath, ULONGLONG fileSize) {
    if (fileSize > MAX_NOTIFICATION_AUDIO_BYTES) {
        LOG_WARNING(
            "Audio file too large: %s (%llu bytes, limit %llu bytes)",
            filePath, fileSize, (ULONGLONG)MAX_NOTIFICATION_AUDIO_BYTES);
        return FALSE;
    }
    return TRUE;
}

BOOL IsValidFilePath(const char* filePath) {
    return filePath && filePath[0] != '\0' &&
           strchr(filePath, '=') == NULL && strlen(filePath) < MAX_PATH;
}

void ResetPlaybackState(void) {
    g_isPlaying = MA_FALSE;
    g_isPaused = MA_FALSE;
    g_audioTimerId = 0;
    g_audioTimerKind = AUDIO_TIMER_NONE;
    g_audioTimerHwnd = NULL;
    InterlockedExchange(&g_decoderAtEnd, 0);
    InterlockedExchange(&g_decoderDrainDeadline, 0);
    InterlockedExchange(&g_decoderDrainRemainingMs, 0);
    InterlockedExchange(&g_decoderPaused, 0);
}

static UINT_PTR NextAudioTimerId(void) {
    LONG serial = InterlockedIncrement(&g_audioTimerSerial);
    return AUDIO_TIMER_ID_BASE + ((UINT_PTR)serial & AUDIO_TIMER_ID_MASK);
}

BOOL StartPlaybackTimer(
    HWND hwnd, AudioTimerKind timerKind, UINT interval) {
    if (!IsCurrentProcessAudioWindow(hwnd)) {
        LOG_WARNING(
            "Cannot start audio completion timer without a window handle");
        return FALSE;
    }
    if (g_audioTimerId != 0) {
        HWND timerWindow = g_audioTimerHwnd ? g_audioTimerHwnd : hwnd;
        if (IsCurrentProcessAudioWindow(timerWindow)) {
            KillTimer(timerWindow, g_audioTimerId);
        }
        g_audioTimerId = 0;
        g_audioTimerKind = AUDIO_TIMER_NONE;
        g_audioTimerHwnd = NULL;
    }
    UINT_PTR timerId = NextAudioTimerId();
    if (!SetTimer(
            hwnd, timerId, interval, (TIMERPROC)AudioTimerCallback)) {
        LOG_WARNING(
            "Failed to start audio completion timer (kind: %d, error: %lu)",
            (int)timerKind, GetLastError());
        return FALSE;
    }
    g_audioTimerId = timerId;
    g_audioTimerKind = timerKind;
    g_audioTimerHwnd = hwnd;
    return TRUE;
}

BOOL FallbackToPlaySound(HWND hwnd, const wchar_t* wideFilePath) {
    if (!PlaySoundW(
            wideFilePath, NULL, SND_FILENAME | SND_ASYNC)) return FALSE;
    if (!StartPlaybackTimer(
            hwnd, AUDIO_TIMER_PLAYSOUND, TIMER_INTERVAL_FALLBACK)) {
        PlaySoundW(NULL, NULL, SND_PURGE);
        ResetPlaybackState();
        return FALSE;
    }
    g_isPlaying = MA_TRUE;
    return TRUE;
}

BOOL FallbackToSystemBeep(HWND hwnd) {
    MessageBeep(MB_OK);
    if (!StartPlaybackTimer(
            hwnd, AUDIO_TIMER_BEEP, TIMER_INTERVAL_BEEP)) {
        ResetPlaybackState();
        return FALSE;
    }
    g_isPlaying = MA_TRUE;
    return TRUE;
}
