/**
 * @file audio_player.c
 * @brief Three-tier audio fallback (miniaudio → PlaySound → beep)
 * 
 * Why three tiers: Unicode paths fail miniaudio, WAV-only fails PlaySound, beep never fails.
 * Path encoding: UTF-8 → short path (8.3) → ANSI for miniaudio compatibility.
 * Completion: miniaudio polls 500ms, PlaySound 3s timeout/purge (no API), beep 500ms fixed.
 */
#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include "../libs/miniaudio/miniaudio.h"
#include "config.h"
#include "log.h"

/* Audio polling intervals:
 * - 500ms for miniaudio: Frequent checks detect completion quickly while minimizing overhead
 * - 3000ms for PlaySound: API lacks completion callback, timeout based on typical notification length
 * - 500ms for beep: Fixed duration matching typical system beep length
 */
#define TIMER_INTERVAL_AUDIO_CHECK 500
#define TIMER_INTERVAL_FALLBACK    3000
#define TIMER_INTERVAL_BEEP        500
#define AUDIO_TIMER_ID_BASE        ((UINT_PTR)0xA7000000u)
#define AUDIO_TIMER_ID_MASK        0xFFFFu
#define MAX_NOTIFICATION_AUDIO_BYTES (32ull * 1024ull * 1024ull)

typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

typedef enum {
    AUDIO_TIMER_NONE = 0,
    AUDIO_TIMER_MINIAUDIO,
    AUDIO_TIMER_PLAYSOUND,
    AUDIO_TIMER_BEEP
} AudioTimerKind;

typedef struct {
    wchar_t path[MAX_PATH * 2];
    ULONGLONG sizeBytes;
} AudioFileInfo;

static ma_engine g_audioEngine;
static ma_sound g_sound;
static ma_bool32 g_engineInitialized = MA_FALSE;
static ma_bool32 g_soundInitialized = MA_FALSE;

static ma_bool32 g_isPlaying = MA_FALSE;
static ma_bool32 g_isPaused = MA_FALSE;
static AudioPlaybackCompleteCallback g_audioCompleteCallback = NULL;
static HWND g_audioCallbackHwnd = NULL;
static UINT_PTR g_audioTimerId = 0;
static AudioTimerKind g_audioTimerKind = AUDIO_TIMER_NONE;
static HWND g_audioTimerHwnd = NULL;
static volatile LONG g_audioTimerSerial = 0;
static wchar_t g_tempAudioFile[MAX_PATH] = {0};

static void CALLBACK AudioTimerCallback(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime);
static BOOL FallbackToPlaySound(HWND hwnd, const wchar_t* wFilePath);
static BOOL FallbackToSystemBeep(HWND hwnd);
static void ResetPlaybackState(void);
static BOOL StartPlaybackTimer(HWND hwnd, AudioTimerKind timerKind, UINT interval);
static void ShutdownAudioEngine(void);
static void CleanupMiniaudioAttempt(void);
static BOOL GetWideCharPath(const char* utf8Path, wchar_t* wPath, size_t wPathSize);

static BOOL IsCurrentProcessAudioWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    return processId == GetCurrentProcessId();
}

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/** Resolve once, then reuse the wide path and file metadata across fallback tiers. */
static BOOL GetAudioFileInfo(const char* filePath, AudioFileInfo* info) {
    if (!filePath || filePath[0] == '\0' || !info) return FALSE;

    info->path[0] = L'\0';
    info->sizeBytes = 0;

    if (!GetWideCharPath(filePath, info->path, MAX_PATH * 2)) {
        return FALSE;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(info->path, GetFileExInfoStandard, &attrs) ||
        (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return FALSE;
    }

    info->sizeBytes = ((ULONGLONG)attrs.nFileSizeHigh << 32) | attrs.nFileSizeLow;
    return TRUE;
}

static BOOL IsAudioFileSizeAllowed(const char* filePath, ULONGLONG fileSize) {
    if (fileSize > MAX_NOTIFICATION_AUDIO_BYTES) {
        LOG_WARNING("Audio file too large: %s (%llu bytes, limit %llu bytes)",
                    filePath, fileSize, (ULONGLONG)MAX_NOTIFICATION_AUDIO_BYTES);
        return FALSE;
    }

    return TRUE;
}

/** Basic validation (prevents misconfig crashes, not full security) */
static BOOL IsValidFilePath(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    if (strchr(filePath, '=') != NULL) return FALSE;
    if (strlen(filePath) >= MAX_PATH) return FALSE;
    return TRUE;
}

static void ResetPlaybackState(void) {
    g_isPlaying = MA_FALSE;
    g_isPaused = MA_FALSE;
    g_audioTimerId = 0;
    g_audioTimerKind = AUDIO_TIMER_NONE;
    g_audioTimerHwnd = NULL;
    
    /* Clean up temporary audio file if exists */
    if (g_tempAudioFile[0] != L'\0') {
        DeleteFileW(g_tempAudioFile);
        g_tempAudioFile[0] = L'\0';
    }
}

static UINT_PTR NextAudioTimerId(void) {
    LONG serial = InterlockedIncrement(&g_audioTimerSerial);
    return AUDIO_TIMER_ID_BASE + ((UINT_PTR)serial & AUDIO_TIMER_ID_MASK);
}

/** One timer at a time (kills previous) */
static BOOL StartPlaybackTimer(HWND hwnd, AudioTimerKind timerKind, UINT interval) {
    if (!IsCurrentProcessAudioWindow(hwnd)) {
        LOG_WARNING("Cannot start audio completion timer without a window handle");
        return FALSE;
    }

    if (g_audioTimerId != 0) {
        HWND timerHwnd = g_audioTimerHwnd ? g_audioTimerHwnd : hwnd;
        if (IsCurrentProcessAudioWindow(timerHwnd)) {
            KillTimer(timerHwnd, g_audioTimerId);
        }
        g_audioTimerId = 0;
        g_audioTimerKind = AUDIO_TIMER_NONE;
        g_audioTimerHwnd = NULL;
    }

    UINT_PTR timerId = NextAudioTimerId();
    if (SetTimer(hwnd, timerId, interval, (TIMERPROC)AudioTimerCallback) == 0) {
        LOG_WARNING("Failed to start audio completion timer (kind: %d, error: %lu)",
                    (int)timerKind, GetLastError());
        return FALSE;
    }

    g_audioTimerId = timerId;
    g_audioTimerKind = timerKind;
    g_audioTimerHwnd = hwnd;
    return TRUE;
}

/* ============================================================================
 * Fallback mechanisms
 * ============================================================================ */

/** Tier 2: PlaySound (WAV only) */
static BOOL FallbackToPlaySound(HWND hwnd, const wchar_t* wFilePath) {
    if (!PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
        return FALSE;
    }

    if (!StartPlaybackTimer(hwnd, AUDIO_TIMER_PLAYSOUND, TIMER_INTERVAL_FALLBACK)) {
        PlaySoundW(NULL, NULL, SND_PURGE);
        ResetPlaybackState();
        return FALSE;
    }

    g_isPlaying = MA_TRUE;
    return TRUE;
}

/** Tier 3: System beep (never fails) */
static BOOL FallbackToSystemBeep(HWND hwnd) {
    MessageBeep(MB_OK);

    if (!StartPlaybackTimer(hwnd, AUDIO_TIMER_BEEP, TIMER_INTERVAL_BEEP)) {
        ResetPlaybackState();
        return FALSE;
    }

    g_isPlaying = MA_TRUE;
    return TRUE;
}

/* ============================================================================
 * Audio engine management
 * ============================================================================ */

static BOOL InitializeAudioEngine(void) {
    if (g_engineInitialized) {
        return TRUE;
    }

    ma_result result = ma_engine_init(NULL, &g_audioEngine);
    if (result != MA_SUCCESS) {
        return FALSE;
    }

    g_engineInitialized = MA_TRUE;
    return TRUE;
}

static void ShutdownAudioEngine(void) {
    if (g_engineInitialized && !g_soundInitialized) {
        ma_engine_uninit(&g_audioEngine);
        g_engineInitialized = MA_FALSE;
    }
}

static void CleanupMiniaudioAttempt(void) {
    if (g_soundInitialized) {
        ma_sound_stop(&g_sound);
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }

    ShutdownAudioEngine();
    ResetPlaybackState();
}


/* ============================================================================
 * Path conversion utilities
 * ============================================================================ */

static const wchar_t* GetFileNameExtensionW(const wchar_t* path) {
    if (!path) {
        return L"";
    }

    const wchar_t* fileName = path;
    for (const wchar_t* p = path; *p; ++p) {
        if (*p == L'\\' || *p == L'/') {
            fileName = p + 1;
        }
    }

    const wchar_t* ext = wcsrchr(fileName, L'.');
    if (!ext || ext == fileName) {
        return L"";
    }

    return ext;
}

static BOOL IsAsciiExtension(const wchar_t* ext) {
    if (!ext || ext[0] != L'.' || ext[1] == L'\0') {
        return FALSE;
    }

    size_t extLen = wcslen(ext);
    if (extLen > 8) {
        return FALSE;
    }

    for (size_t i = 0; i < extLen; ++i) {
        if (ext[i] > 0x7F || ext[i] == L'\\' || ext[i] == L'/') {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * Create temporary copy of audio file with ASCII-safe filename
 * Used when original filename contains non-ASCII characters (e.g., Cyrillic)
 * Returns TRUE if temp file created successfully
 */
static BOOL CreateTempAudioCopy(const wchar_t* originalPath, wchar_t* tempPath, size_t tempPathSize) {
    if (!originalPath || !tempPath || tempPathSize < MAX_PATH) {
        return FALSE;
    }
    tempPath[0] = L'\0';

    /* Get temp directory */
    wchar_t tempDir[MAX_PATH] = {0};
    DWORD tempDirLen = GetTempPathW(MAX_PATH, tempDir);
    if (tempDirLen == 0 || tempDirLen >= MAX_PATH) {
        return FALSE;
    }

    const wchar_t* originalExt = GetFileNameExtensionW(originalPath);
    BOOL preserveExt = IsAsciiExtension(originalExt);

    for (int attempt = 0; attempt < 8; ++attempt) {
        wchar_t reservedPath[MAX_PATH] = {0};
        wchar_t candidatePath[MAX_PATH] = {0};

        /* Reserve a unique base path; avoids timestamp collisions and stale-file overwrite. */
        if (GetTempFileNameW(tempDir, L"ctm", 0, reservedPath) == 0) {
            return FALSE;
        }

        HRESULT copyResult = StringCchCopyW(candidatePath, MAX_PATH, reservedPath);
        if (FAILED(copyResult)) {
            DeleteFileW(reservedPath);
            return FALSE;
        }

        if (preserveExt) {
            wchar_t* reservedExt = (wchar_t*)GetFileNameExtensionW(candidatePath);
            if (reservedExt[0] == L'.') {
                *reservedExt = L'\0';
                if (FAILED(StringCchCatW(candidatePath, MAX_PATH, originalExt))) {
                    DeleteFileW(reservedPath);
                    return FALSE;
                }
            }
        }

        BOOL samePath = (wcscmp(candidatePath, reservedPath) == 0);
        BOOL copied = CopyFileW(originalPath, candidatePath, samePath ? FALSE : TRUE);
        DWORD copyError = copied ? ERROR_SUCCESS : GetLastError();

        if (!samePath) {
            DeleteFileW(reservedPath);
        }

        if (copied) {
            if (FAILED(StringCchCopyW(g_tempAudioFile, MAX_PATH, candidatePath))) {
                DeleteFileW(candidatePath);
                tempPath[0] = L'\0';
                g_tempAudioFile[0] = L'\0';
                return FALSE;
            }
            if (FAILED(StringCchCopyW(tempPath, tempPathSize, candidatePath))) {
                DeleteFileW(candidatePath);
                g_tempAudioFile[0] = L'\0';
                return FALSE;
            }
            return TRUE;
        }

        if (samePath || copyError != ERROR_FILE_EXISTS) {
            DeleteFileW(candidatePath);
            tempPath[0] = L'\0';
            return FALSE;
        }
    }

    tempPath[0] = L'\0';
    return FALSE;
}

/** Wide path → short path (8.3) → ANSI for miniaudio, or create temp copy for non-ASCII names */
static BOOL ConvertPathForMiniaudio(const wchar_t* wFilePath, char* outPath, size_t outPathSize) {
    if (!wFilePath || wFilePath[0] == L'\0' || !outPath || outPathSize == 0) {
        return FALSE;
    }

    /* Try short path for ASCII compatibility */
    wchar_t shortPath[MAX_PATH] = {0};
    DWORD shortPathLen = GetShortPathNameW(wFilePath, shortPath, MAX_PATH);
    
    if (shortPathLen > 0 && shortPathLen < MAX_PATH) {
        /* Convert short path to ANSI - only if conversion succeeds without data loss */
        BOOL usedDefaultChar = FALSE;
        int result = WideCharToMultiByte(CP_ACP, 0, shortPath, -1, outPath, (int)outPathSize, NULL, &usedDefaultChar);
        
        if (result > 0 && !usedDefaultChar) {
            return TRUE;
        }
    }

    /* If the original path is already representable in the active ANSI code page,
     * miniaudio can use it directly; avoid copying the file to temp in this case. */
    BOOL usedDefaultChar = FALSE;
    int directResult = WideCharToMultiByte(CP_ACP, 0, wFilePath, -1, outPath,
                                           (int)outPathSize, NULL, &usedDefaultChar);
    if (directResult > 0 && !usedDefaultChar) {
        return TRUE;
    }
    
    /* 
     * Path contains non-ASCII characters that can't be converted safely.
     * Create a temporary copy with ASCII-safe filename to enable miniaudio playback.
     * This preserves the original file and allows playback of files with Cyrillic,
     * Chinese, or other non-ASCII names on any Windows system.
     */
    wchar_t tempPath[MAX_PATH];
    if (CreateTempAudioCopy(wFilePath, tempPath, MAX_PATH)) {
        /* Try to convert temp path (should always succeed since we use ASCII filename) */
        DWORD tempShortLen = GetShortPathNameW(tempPath, shortPath, MAX_PATH);
        if (tempShortLen > 0 && tempShortLen < MAX_PATH) {
            BOOL shortPathUsedDefaultChar = FALSE;
            int result = WideCharToMultiByte(CP_ACP, 0, shortPath, -1, outPath, (int)outPathSize, NULL, &shortPathUsedDefaultChar);
            if (result > 0 && !shortPathUsedDefaultChar) {
                return TRUE;
            }
        }
        
        /* If even temp path fails (shouldn't happen), try direct conversion */
        if (WideCharToMultiByte(CP_ACP, 0, tempPath, -1, outPath, (int)outPathSize, NULL, NULL) > 0) {
            return TRUE;
        }
        
        /* Cleanup temp file if we can't use it */
        DeleteFileW(tempPath);
        g_tempAudioFile[0] = L'\0';
    }
    
    /* All attempts failed - fallback to PlaySound */
    return FALSE;
}

static BOOL GetWideCharPath(const char* utf8Path, wchar_t* wPath, size_t wPathSize) {
    if (!wPath || wPathSize == 0) return FALSE;
    wPath[0] = L'\0';
    if (!utf8Path || wPathSize > INT_MAX) return FALSE;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, (int)wPathSize) <= 0) {
        wPath[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

/* ============================================================================
 * Audio playback core
 * ============================================================================ */

static ma_result LoadAudioFile(const char* convertedPath) {
    if (g_soundInitialized) {
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }
    
    ma_result result = ma_sound_init_from_file(&g_audioEngine, convertedPath, 0, NULL, NULL, &g_sound);
    if (result == MA_SUCCESS) {
        g_soundInitialized = MA_TRUE;
    }
    return result;
}

static ma_result StartAudioPlayback(void) {
    if (!g_soundInitialized) {
        return MA_ERROR;
    }
    
    ma_result result = ma_sound_start(&g_sound);
    if (result != MA_SUCCESS) {
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }
    return result;
}

/** Tier 1: miniaudio with automatic fallback */
static BOOL PlayAudioWithMiniaudio(HWND hwnd, const char* filePath, const wchar_t* wFilePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    if (!wFilePath || wFilePath[0] == L'\0') return FALSE;
    
    if (!g_engineInitialized && !InitializeAudioEngine()) {
        LOG_WARNING("Failed to initialize miniaudio engine, will try fallback methods");
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    float volume = (float)g_AppConfig.notification.sound.volume / 100.0f;
    ma_engine_set_volume(&g_audioEngine, volume);
    
    char convertedPath[MAX_PATH * 4] = {0};
    if (!ConvertPathForMiniaudio(wFilePath, convertedPath, sizeof(convertedPath))) {
        CleanupMiniaudioAttempt();
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    ma_result result = LoadAudioFile(convertedPath);
    if (result != MA_SUCCESS) {
        LOG_WARNING("miniaudio failed to load audio file (error: %d), falling back to PlaySound", result);
        CleanupMiniaudioAttempt();
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    if (StartAudioPlayback() != MA_SUCCESS) {
        LOG_WARNING("miniaudio playback start failed, falling back to PlaySound");
        CleanupMiniaudioAttempt();
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    if (!StartPlaybackTimer(hwnd, AUDIO_TIMER_MINIAUDIO, TIMER_INTERVAL_AUDIO_CHECK)) {
        ma_sound_stop(&g_sound);
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
        ShutdownAudioEngine();
        ResetPlaybackState();
        return FallbackToPlaySound(hwnd, wFilePath);
    }

    g_isPlaying = MA_TRUE;
    return TRUE;
}

/* ============================================================================
 * Timer callbacks
 * ============================================================================ */

/** Unified completion detector (polls miniaudio, timeouts for others) */
static void CALLBACK AudioTimerCallback(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime) {
    (void)dwTime;

    if (message != WM_TIMER ||
        idEvent != g_audioTimerId ||
        g_audioTimerKind == AUDIO_TIMER_NONE ||
        hwnd != g_audioTimerHwnd ||
        !IsCurrentProcessAudioWindow(hwnd)) {
        return;
    }
    
    BOOL shouldStop = FALSE;
    AudioTimerKind timerKind = g_audioTimerKind;
    
    switch (timerKind) {
        case AUDIO_TIMER_MINIAUDIO:
            if (g_engineInitialized && g_soundInitialized) {
                if (!ma_sound_is_playing(&g_sound) && !g_isPaused) {
                    if (g_soundInitialized) {
                        ma_sound_uninit(&g_sound);
                        g_soundInitialized = MA_FALSE;
                    }
                    ShutdownAudioEngine();
                    shouldStop = TRUE;
                }
            } else {
                shouldStop = TRUE;
            }
            break;
            
        case AUDIO_TIMER_PLAYSOUND:
            PlaySoundW(NULL, NULL, SND_PURGE);
            shouldStop = TRUE;
            break;

        case AUDIO_TIMER_BEEP:
            shouldStop = TRUE;
            break;

        default:
            break;
    }
    
    if (shouldStop) {
        HWND timerHwnd = g_audioTimerHwnd ? g_audioTimerHwnd : hwnd;
        HWND callbackHwnd = g_audioCallbackHwnd;
        AudioPlaybackCompleteCallback callback = g_audioCompleteCallback;
        BOOL shouldNotifyCallback = callback &&
                                    callbackHwnd &&
                                    callbackHwnd == timerHwnd &&
                                    IsCurrentProcessAudioWindow(callbackHwnd);

        if (IsCurrentProcessAudioWindow(timerHwnd)) {
            KillTimer(timerHwnd, g_audioTimerId);
        }
        ResetPlaybackState();
        
        if (shouldNotifyCallback) {
            callback(callbackHwnd);
        }
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void SetAudioVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    if (g_engineInitialized) {
        float volFloat = (float)volume / 100.0f;
        ma_engine_set_volume(&g_audioEngine, volFloat);

        if (g_soundInitialized && g_isPlaying) {
            ma_sound_set_volume(&g_sound, volFloat);
        }
    }
}

void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback) {
    if (callback && IsCurrentProcessAudioWindow(hwnd)) {
        g_audioCallbackHwnd = hwnd;
        g_audioCompleteCallback = callback;
    } else {
        g_audioCallbackHwnd = NULL;
        g_audioCompleteCallback = NULL;
    }
}

void CleanupAudioResources(void) {
    PlaySoundW(NULL, NULL, SND_PURGE);

    if (g_engineInitialized && g_soundInitialized) {
        ma_sound_stop(&g_sound);
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }
    ShutdownAudioEngine();

    if (g_audioTimerId != 0 && IsCurrentProcessAudioWindow(g_audioTimerHwnd)) {
        KillTimer(g_audioTimerHwnd, g_audioTimerId);
    }

    ResetPlaybackState();
}

BOOL PlayNotificationSoundFile(HWND hwnd, const char* soundFile) {
    CleanupAudioResources();

    if (!soundFile || soundFile[0] == '\0') {
        return TRUE;
    }

    if (strcmp(soundFile, "SYSTEM_BEEP") == 0) {
        return FallbackToSystemBeep(hwnd);
    }

    if (!IsValidFilePath(soundFile)) {
        LOG_WARNING("Invalid audio file path (will fallback to system beep): %s", soundFile);
        return FallbackToSystemBeep(hwnd);
    }

    AudioFileInfo fileInfo;
    if (!GetAudioFileInfo(soundFile, &fileInfo)) {
        LOG_WARNING("Cannot find audio file (will fallback to system beep): %s", soundFile);
        return FallbackToSystemBeep(hwnd);
    }

    if (!IsAudioFileSizeAllowed(soundFile, fileInfo.sizeBytes)) {
        return FallbackToSystemBeep(hwnd);
    }

    if (PlayAudioWithMiniaudio(hwnd, soundFile, fileInfo.path)) {
        return TRUE;
    }

    LOG_WARNING("All audio playback methods failed, using system beep as final fallback");
    return FallbackToSystemBeep(hwnd);
}

BOOL PlayNotificationSound(HWND hwnd) {
    return PlayNotificationSoundFile(hwnd, g_AppConfig.notification.sound.sound_file);
}

BOOL PauseNotificationSound(void) {
    if (g_isPlaying && !g_isPaused && g_engineInitialized && g_soundInitialized) {
        if (ma_sound_stop(&g_sound) != MA_SUCCESS) {
            return FALSE;
        }
        g_isPaused = MA_TRUE;

        if (g_audioTimerKind == AUDIO_TIMER_MINIAUDIO && g_audioTimerId != 0) {
            HWND timerHwnd = g_audioTimerHwnd;
            HWND killHwnd = timerHwnd ? timerHwnd : g_audioCallbackHwnd;
            if (IsCurrentProcessAudioWindow(killHwnd)) {
                KillTimer(killHwnd, g_audioTimerId);
            }
            g_audioTimerId = 0;
            g_audioTimerHwnd = timerHwnd;
        }
        return TRUE;
    }
    return FALSE;
}

BOOL ResumeNotificationSound(void) {
    if (g_isPlaying && g_isPaused && g_engineInitialized && g_soundInitialized) {
        HWND timerHwnd = g_audioTimerHwnd ? g_audioTimerHwnd : g_audioCallbackHwnd;
        if (ma_sound_start(&g_sound) != MA_SUCCESS) {
            return FALSE;
        }
        g_isPaused = MA_FALSE;

        if (g_audioTimerKind == AUDIO_TIMER_MINIAUDIO && g_audioTimerId == 0) {
            if (!IsCurrentProcessAudioWindow(timerHwnd) ||
                !StartPlaybackTimer(timerHwnd, AUDIO_TIMER_MINIAUDIO, TIMER_INTERVAL_AUDIO_CHECK)) {
                ma_sound_stop(&g_sound);
                g_isPaused = MA_TRUE;
                return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}

void StopNotificationSound(void) {
    CleanupAudioResources();
}
