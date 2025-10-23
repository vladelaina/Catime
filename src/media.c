/**
 * @file media.c
 * @brief System-wide media playback control via virtual key events
 * @version 2.0 - Refactored for better maintainability and reduced code duplication
 */
#include <windows.h>
#include "../include/media.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Standard delay between key press and release (ms) */
#define MEDIA_KEY_DELAY_MS 50

/** @brief Final delay after complete key sequence (ms) */
#define MEDIA_FINAL_DELAY_MS 100

/** @brief Number of PLAY_PAUSE key repeats for maximum compatibility */
#define PLAY_PAUSE_REPEAT_COUNT 2

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Send a media key with standardized press-release-delay sequence
 * @param key Virtual key code (VK_MEDIA_STOP, VK_MEDIA_PLAY_PAUSE, etc.)
 * @param delay Delay after key release in milliseconds
 * 
 * Encapsulates the common pattern of:
 * 1. Key press (down)
 * 2. Short delay (50ms for key registration)
 * 3. Key release (up)
 * 4. Configurable delay (for inter-key spacing)
 */
static inline void SendMediaKey(BYTE key, DWORD delay) {
    keybd_event(key, 0, 0, 0);                    /** Press key */
    Sleep(MEDIA_KEY_DELAY_MS);
    keybd_event(key, 0, KEYEVENTF_KEYUP, 0);      /** Release key */
    Sleep(delay);
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Force pause of any active media playback across the system
 * 
 * Uses a two-stage aggressive sequence for maximum compatibility:
 * 
 * Stage 1: MEDIA_STOP
 *   - Ensures media is fully stopped (not just paused)
 *   - Required for some players that distinguish stop vs pause
 * 
 * Stage 2: Multiple MEDIA_PLAY_PAUSE
 *   - Sent multiple times for stubborn media players
 *   - Some players require repeated signals to respond
 *   - Handles players that toggle play/pause state
 * 
 * @note This uses a delay-based approach which may not be 100% reliable
 *       on heavily loaded systems where key event processing is delayed
 */
void PauseMediaPlayback(void) {
    /** Stage 1: Send STOP to ensure media is fully stopped */
    SendMediaKey(VK_MEDIA_STOP, MEDIA_KEY_DELAY_MS);
    
    /** Stage 2: Send PLAY_PAUSE multiple times for compatibility */
    for (int i = 0; i < PLAY_PAUSE_REPEAT_COUNT; i++) {
        DWORD finalDelay = (i == PLAY_PAUSE_REPEAT_COUNT - 1) 
                          ? MEDIA_FINAL_DELAY_MS 
                          : MEDIA_KEY_DELAY_MS;
        SendMediaKey(VK_MEDIA_PLAY_PAUSE, finalDelay);
    }
}