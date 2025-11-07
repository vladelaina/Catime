/**
 * @file media.c
 * @brief System-wide media control via virtual keys
 */
#include <windows.h>
#include "media.h"

/* Media key timing for reliable hardware/driver recognition:
 * - 50ms: Minimum duration for key press to register across different media players
 * - 100ms: Final delay ensures command completion before returning
 */
#define MEDIA_KEY_DELAY_MS 50
#define MEDIA_FINAL_DELAY_MS 100

/** Repeated for stubborn players */
#define PLAY_PAUSE_REPEAT_COUNT 2

/** Standard press-release-delay pattern */
static inline void SendMediaKey(BYTE key, DWORD delay) {
    keybd_event(key, 0, 0, 0);
    Sleep(MEDIA_KEY_DELAY_MS);
    keybd_event(key, 0, KEYEVENTF_KEYUP, 0);
    Sleep(delay);
}

/**
 * Two-stage sequence for maximum compatibility:
 * 1. STOP (some players distinguish stop vs pause)
 * 2. Multiple PLAY_PAUSE (handles toggle-based players)
 * 
 * @note Delay-based, may fail on heavily loaded systems
 */
void PauseMediaPlayback(void) {
    SendMediaKey(VK_MEDIA_STOP, MEDIA_KEY_DELAY_MS);
    
    for (int i = 0; i < PLAY_PAUSE_REPEAT_COUNT; i++) {
        DWORD finalDelay = (i == PLAY_PAUSE_REPEAT_COUNT - 1) 
                          ? MEDIA_FINAL_DELAY_MS 
                          : MEDIA_KEY_DELAY_MS;
        SendMediaKey(VK_MEDIA_PLAY_PAUSE, finalDelay);
    }
}