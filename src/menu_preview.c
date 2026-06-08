/**
 * @file menu_preview.c
 * @brief Menu option live preview system implementation
 *
 * Extracted from window_procedure.c for better modularity and reusability.
 */
#include <windows.h>
#include <string.h>
#include "menu_preview.h"
#include "config.h"
#include "font.h"
#include "color/color.h"
#include "color/color_parser.h"
#include "drawing/drawing_effect.h"
#include "timer/timer.h"
#include "tray/tray_animation_core.h"
#include "window/window_core.h"
#include "window/window_desktop_integration.h"
#include "log.h"

/* ============================================================================
 * External Dependencies
 * ============================================================================ */

extern char FONT_FILE_NAME[MAX_PATH];
extern char FONT_INTERNAL_NAME[MAX_PATH];
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];
/** Time format now in g_AppConfig.display.time_format */

/* External Preview State from font_manager.c (used by Drag & Drop) */
extern BOOL IS_PREVIEWING;
extern char PREVIEW_FONT_NAME[MAX_PATH];
extern char PREVIEW_INTERNAL_NAME[MAX_PATH];

extern void ResetTimerWithInterval(HWND hwnd);
extern BOOL WriteConfigColor(const char* color);
extern BOOL WriteConfigTimeFormat(TimeFormatType format);
extern BOOL WriteConfigShowMilliseconds(BOOL showMilliseconds);
extern void WriteConfigEffect(EffectType effect); /* To be implemented if needed, or use existing logic */

/* ============================================================================
 * Preview State
 * ============================================================================ */

/**
 * @brief Global preview state with type-safe union
 */
typedef struct {
    PreviewType type;
    union {
        char colorHex[COLOR_HEX_BUFFER];
        struct {
            char fontName[MAX_PATH];
            char internalName[MAX_PATH];
        } font;
        TimeFormatType timeFormat;
        BOOL showMilliseconds;
        char animationPath[MAX_PATH];
        EffectType effect;
    } data;
    BOOL needsTimerReset;
    BOOL wasWindowVisible;
    BOOL didShowForPreview;
    BOOL createdPreviewTimer;
    BOOL savedTimerState;
    BOOL previousShowCurrentTime;
    BOOL previousCountUp;
    BOOL previousIsPaused;
    int32_t previousTotalTime;
    int32_t previousCountdownElapsed;
    int64_t previousTargetEndTime;
    int64_t previousPauseStartTime;
} PreviewState;

static PreviewState g_previewState = {PREVIEW_TYPE_NONE};

/* ============================================================================
 * Core Preview Functions
 * ============================================================================ */

BOOL IsPreviewActive(void) {
    return g_previewState.type != PREVIEW_TYPE_NONE;
}

PreviewType GetActivePreviewType(void) {
    return g_previewState.type;
}

static BOOL IsConfiguredTextEffectActive(void) {
    return CLOCK_TEXT_EFFECT != TEXT_EFFECT_NONE;
}

static BOOL LoadPreviewFontName(const char* fontName, char* internalName, size_t internalNameSize) {
    if (!fontName || !fontName[0] || !internalName || internalNameSize == 0) {
        return FALSE;
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);
    if (!LoadFontByNameAndGetRealName(hInstance, fontName, internalName, internalNameSize)) {
        LOG_WARNING("Preview: failed to load font preview: %s", fontName);
        return FALSE;
    }

    return TRUE;
}

static BOOL RestoreConfiguredFontAfterPreview(void) {
    if (FONT_FILE_NAME[0] == '\0') {
        return FALSE;
    }

    const char* loadName = FONT_FILE_NAME;
    if (IsFontsFolderPath(FONT_FILE_NAME)) {
        const char* relativePath = ExtractRelativePath(FONT_FILE_NAME);
        if (!relativePath || relativePath[0] == '\0') {
            return FALSE;
        }
        loadName = relativePath;
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);
    if (!LoadFontByNameAndGetRealName(hInstance, loadName,
                                      FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
        LOG_WARNING("Preview: failed to restore font after preview: %s", FONT_FILE_NAME);
        return FALSE;
    }

    return TRUE;
}

void StartPreview(PreviewType type, const void* data, HWND hwnd) {
    if (type == PREVIEW_TYPE_EFFECT && g_previewState.type == PREVIEW_TYPE_EFFECT) {
        if (!data) {
            return;
        }

        EffectType effect = *(const EffectType*)data;
        if (g_previewState.data.effect == effect) {
            return;
        }

        g_previewState.data.effect = effect;
        if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
        return;
    }

    if (type == PREVIEW_TYPE_FONT && g_previewState.type == PREVIEW_TYPE_FONT) {
        const char* fontName = (const char*)data;
        if (!fontName || !fontName[0]) {
            return;
        }
        if (_stricmp(g_previewState.data.font.fontName, fontName) == 0) {
            return;
        }

        char internalName[MAX_PATH] = {0};
        if (!LoadPreviewFontName(fontName, internalName, sizeof(internalName))) {
            return;
        }

        strncpy_s(g_previewState.data.font.fontName, MAX_PATH, fontName, _TRUNCATE);
        strncpy_s(g_previewState.data.font.internalName, MAX_PATH, internalName, _TRUNCATE);
        if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
        return;
    }

    if (IsPreviewActive()) CancelPreview(hwnd);
    
    g_previewState.type = type;
    g_previewState.needsTimerReset = FALSE;
    
    switch (type) {
        case PREVIEW_TYPE_COLOR: {
            const char* colorHex = (const char*)data;
            strncpy_s(g_previewState.data.colorHex, sizeof(g_previewState.data.colorHex), 
                     colorHex, _TRUNCATE);
            if (hwnd) ResetTimerWithInterval(hwnd);
            break;
        }
        
        case PREVIEW_TYPE_FONT: {
            const char* fontName = (const char*)data;
            char internalName[MAX_PATH] = {0};
            if (!LoadPreviewFontName(fontName, internalName, sizeof(internalName))) {
                g_previewState.type = PREVIEW_TYPE_NONE;
                return;
            }

            strncpy_s(g_previewState.data.font.fontName, MAX_PATH, fontName, _TRUNCATE);
            strncpy_s(g_previewState.data.font.internalName, MAX_PATH, internalName, _TRUNCATE);
            break;
        }
        
        case PREVIEW_TYPE_TIME_FORMAT:
            g_previewState.data.timeFormat = *(TimeFormatType*)data;
            break;
        
        case PREVIEW_TYPE_MILLISECONDS:
            g_previewState.data.showMilliseconds = *(BOOL*)data;
            g_previewState.needsTimerReset = TRUE;
            if (hwnd) ResetTimerWithInterval(hwnd);
            break;
        
        case PREVIEW_TYPE_ANIMATION: {
            const char* animPath = (const char*)data;
            strncpy_s(g_previewState.data.animationPath, MAX_PATH, animPath, _TRUNCATE);
            StartAnimationPreview(animPath);
            break;
        }
        
        case PREVIEW_TYPE_EFFECT:
            g_previewState.data.effect = *(EffectType*)data;
            break;
        
        default:
            g_previewState.type = PREVIEW_TYPE_NONE;
            return;
    }
    
    if (hwnd && type != PREVIEW_TYPE_ANIMATION) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void CancelPreview(HWND hwnd) {
    /* Cancel active or queued animation previews; queued loads may not have
       flipped g_isPreviewActive yet. */
    if (g_isPreviewActive || g_previewState.type == PREVIEW_TYPE_ANIMATION) {
        CancelAnimationPreview();
    }

    if (!IsPreviewActive()) return;

    BOOL needsRedraw = (g_previewState.type != PREVIEW_TYPE_ANIMATION &&
                        g_previewState.type != PREVIEW_TYPE_NONE);
    BOOL needsTimerReset = (g_previewState.type == PREVIEW_TYPE_MILLISECONDS || 
                            g_previewState.type == PREVIEW_TYPE_COLOR);
    BOOL needsFontReload = (g_previewState.type == PREVIEW_TYPE_FONT);
    BOOL needsEffectCleanup = (g_previewState.type == PREVIEW_TYPE_EFFECT &&
                               !IsConfiguredTextEffectActive());

    if (needsFontReload) {
        RestoreConfiguredFontAfterPreview();
    }

    /* Reset preview state */
    g_previewState.type = PREVIEW_TYPE_NONE;

    if (needsEffectCleanup) {
        CleanupDrawingEffects();
    }

    if (needsTimerReset && hwnd) ResetTimerWithInterval(hwnd);
    if (needsRedraw && hwnd) InvalidateRect(hwnd, NULL, TRUE);
}

BOOL ApplyPreview(HWND hwnd) {
    if (!IsPreviewActive()) return FALSE;
    
    PreviewType appliedType = g_previewState.type;
    
    switch (appliedType) {
        case PREVIEW_TYPE_COLOR:
            if (!WriteConfigColor(g_previewState.data.colorHex)) {
                return FALSE;
            }
            if (hwnd) ResetTimerWithInterval(hwnd);
            break;
            
        case PREVIEW_TYPE_FONT:
            if (!WriteConfigFont(g_previewState.data.font.fontName, FALSE)) {
                CancelPreview(hwnd);
                return FALSE;
            }
            strncpy_s(FONT_FILE_NAME, sizeof(FONT_FILE_NAME), 
                     g_previewState.data.font.fontName, _TRUNCATE);
            strncpy_s(FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME),
                     g_previewState.data.font.internalName, _TRUNCATE);
            break;
            
        case PREVIEW_TYPE_TIME_FORMAT:
            if (!WriteConfigTimeFormat(g_previewState.data.timeFormat)) {
                return FALSE;
            }
            break;
            
        case PREVIEW_TYPE_MILLISECONDS:
            if (!WriteConfigShowMilliseconds(g_previewState.data.showMilliseconds)) {
                return FALSE;
            }
            break;
            
        case PREVIEW_TYPE_ANIMATION:
            /* Animation preview applies itself */
            break;
            
        case PREVIEW_TYPE_EFFECT:
            /* Effect changes are applied via WM_COMMAND handlers */
            break;
            
        default:
            return FALSE;
    }
    
    g_previewState.type = PREVIEW_TYPE_NONE;
    if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

void MarkAnimationPreviewApplied(HWND hwnd) {
    if (g_previewState.type != PREVIEW_TYPE_ANIMATION) {
        return;
    }

    g_previewState.type = PREVIEW_TYPE_NONE;
    g_previewState.data.animationPath[0] = '\0';
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/* ============================================================================
 * Type-Safe Accessors
 * ============================================================================ */

void GetActiveColor(char* outColor, size_t bufferSize) {
    if (!outColor || bufferSize == 0) return;
    
    const char* color = (g_previewState.type == PREVIEW_TYPE_COLOR) ?
                        g_previewState.data.colorHex : CLOCK_TEXT_COLOR;
    strncpy_s(outColor, bufferSize, color, _TRUNCATE);
}

void GetActiveFont(char* outFontName, char* outInternalName, size_t bufferSize) {
    if (!outFontName || !outInternalName || bufferSize == 0) return;
    
    if (g_previewState.type == PREVIEW_TYPE_FONT) {
        strncpy_s(outFontName, bufferSize, g_previewState.data.font.fontName, _TRUNCATE);
        strncpy_s(outInternalName, bufferSize, g_previewState.data.font.internalName, _TRUNCATE);
    } else if (IS_PREVIEWING) {
        /* Support for Drag & Drop live preview */
        strncpy_s(outFontName, bufferSize, PREVIEW_FONT_NAME, _TRUNCATE);
        strncpy_s(outInternalName, bufferSize, PREVIEW_INTERNAL_NAME, _TRUNCATE);
    } else {
        strncpy_s(outFontName, bufferSize, FONT_FILE_NAME, _TRUNCATE);
        strncpy_s(outInternalName, bufferSize, FONT_INTERNAL_NAME, _TRUNCATE);
    }
}

TimeFormatType GetActiveTimeFormat(void) {
    return (g_previewState.type == PREVIEW_TYPE_TIME_FORMAT) ?
           g_previewState.data.timeFormat : g_AppConfig.display.time_format.format;
}

BOOL GetActiveShowMilliseconds(void) {
    return (g_previewState.type == PREVIEW_TYPE_MILLISECONDS) ?
           g_previewState.data.showMilliseconds : g_AppConfig.display.time_format.show_milliseconds;
}

EffectType GetActiveEffect(void) {
    /* Performance optimization: Disable visual effects in milliseconds mode.
     * Effects require expensive per-frame rendering (blur, lighting, etc.)
     * At 50 FPS (20ms interval for milliseconds), this causes high CPU usage.
     * Silently disable effects when milliseconds are shown, auto-restore when disabled. */
    if (GetActiveShowMilliseconds()) {
        return EFFECT_TYPE_NONE;
    }
    
    if (g_previewState.type == PREVIEW_TYPE_EFFECT) {
        return g_previewState.data.effect;
    }
    
    /* Priority matches drawing_text_stb.c original logic */
    if (CLOCK_LIQUID_EFFECT) return EFFECT_TYPE_LIQUID;
    if (CLOCK_HOLOGRAPHIC_EFFECT) return EFFECT_TYPE_HOLOGRAPHIC;
    if (CLOCK_NEON_EFFECT) return EFFECT_TYPE_NEON;
    if (CLOCK_GLASS_EFFECT) return EFFECT_TYPE_GLASS;
    if (CLOCK_GLOW_EFFECT) return EFFECT_TYPE_GLOW;
    
    return EFFECT_TYPE_NONE;
}

/* ============================================================================
 * Window Visibility Management for Preview
 * ============================================================================ */

void ShowWindowForPreview(HWND hwnd) {
    if (!hwnd) return;

    BOOL isVisible = IsWindowVisible(hwnd);

    BOOL hasActiveContent = CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP ||
                           (CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME);

    LOG_DEBUG("ShowWindowForPreview: visible=%d, showTime=%d, countUp=%d, total=%d, elapsed=%d, hasContent=%d, didShow=%d",
             isVisible, CLOCK_SHOW_CURRENT_TIME, CLOCK_COUNT_UP, CLOCK_TOTAL_TIME, countdown_elapsed_time, hasActiveContent,
             g_previewState.didShowForPreview);

    if (g_previewState.didShowForPreview) {
        LOG_DEBUG("Already in preview mode, refreshing display");
        InvalidateRect(hwnd, NULL, TRUE);
        return;
    }

    if (!isVisible || !hasActiveContent) {
        g_previewState.wasWindowVisible = isVisible;
        g_previewState.didShowForPreview = TRUE;

        if (!hasActiveContent) {
            /* Use 25 minutes (1500 seconds) as preview time for better visual */
            int previewTime = 1500;
            LOG_DEBUG("No active content, creating preview timer: %d", previewTime);

            g_previewState.createdPreviewTimer = TRUE;
            g_previewState.savedTimerState = TRUE;
            g_previewState.previousShowCurrentTime = CLOCK_SHOW_CURRENT_TIME;
            g_previewState.previousCountUp = CLOCK_COUNT_UP;
            g_previewState.previousIsPaused = CLOCK_IS_PAUSED;
            g_previewState.previousTotalTime = CLOCK_TOTAL_TIME;
            g_previewState.previousCountdownElapsed = countdown_elapsed_time;
            g_previewState.previousTargetEndTime = g_target_end_time;
            g_previewState.previousPauseStartTime = g_pause_start_time;

            CLOCK_SHOW_CURRENT_TIME = false;
            CLOCK_COUNT_UP = false;
            CLOCK_IS_PAUSED = true;

            CLOCK_TOTAL_TIME = previewTime;
            countdown_elapsed_time = 0;
            
            /* CRITICAL: Also set g_target_end_time for GetCountDownComponents() */
            int64_t now = GetAbsoluteTimeMs();
            g_pause_start_time = now;
            g_target_end_time = now + ((int64_t)previewTime * 1000);
        } else {
            LOG_DEBUG("Window hidden but has active timer, just showing it");
            g_previewState.createdPreviewTimer = FALSE;
            g_previewState.savedTimerState = FALSE;
        }

        if (!isVisible) {
            EnsureWindowVisibleWithTopmostState(hwnd);
        }
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        g_previewState.wasWindowVisible = TRUE;
        g_previewState.didShowForPreview = FALSE;
        g_previewState.createdPreviewTimer = FALSE;
        g_previewState.savedTimerState = FALSE;
    }
}

void RestoreWindowVisibility(HWND hwnd) {
    if (!hwnd || !g_previewState.didShowForPreview) return;

    LOG_DEBUG("RestoreWindowVisibility: was visible=%d, created preview timer=%d",
             g_previewState.wasWindowVisible, g_previewState.createdPreviewTimer);

    if (g_previewState.createdPreviewTimer) {
        LOG_DEBUG("Clearing preview timer that we created");
        if (g_previewState.savedTimerState) {
            CLOCK_SHOW_CURRENT_TIME = g_previewState.previousShowCurrentTime;
            CLOCK_COUNT_UP = g_previewState.previousCountUp;
            CLOCK_IS_PAUSED = g_previewState.previousIsPaused;
            CLOCK_TOTAL_TIME = g_previewState.previousTotalTime;
            countdown_elapsed_time = g_previewState.previousCountdownElapsed;
            g_target_end_time = g_previewState.previousTargetEndTime;
            g_pause_start_time = g_previewState.previousPauseStartTime;
        } else {
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            CLOCK_SHOW_CURRENT_TIME = false;
            CLOCK_COUNT_UP = false;
            CLOCK_IS_PAUSED = false;
            g_target_end_time = 0;
            g_pause_start_time = 0;
        }
        ResetTimerWithInterval(hwnd);
    } else {
        LOG_DEBUG("Not clearing timer - was showing existing active timer");
    }

    if (!g_previewState.wasWindowVisible) {
        HideWindowIntentionally(hwnd);
    } else {
        InvalidateRect(hwnd, NULL, TRUE);
    }

    g_previewState.didShowForPreview = FALSE;
    g_previewState.createdPreviewTimer = FALSE;
    g_previewState.savedTimerState = FALSE;
}

/* ============================================================================
 * Preview Time Text Generation
 * ============================================================================ */

BOOL GetPreviewTimeText(wchar_t* outText, size_t bufferSize) {
    if (!outText || bufferSize == 0) return FALSE;

    if (!CLOCK_EDIT_MODE) {
        return FALSE;
    }

    /* Show current time in edit mode when no active content */
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    int hours = CLOCK_USE_24HOUR ? st.wHour : (st.wHour % 12 == 0 ? 12 : st.wHour % 12);
    int minutes = st.wMinute;
    int seconds = st.wSecond;

    TimeFormatType format = GetActiveTimeFormat();
    BOOL showMs = GetActiveShowMilliseconds();

    if (showMs) {
        int centiseconds = st.wMilliseconds / 10;
        if (format == TIME_FORMAT_FULL_PADDED || format == TIME_FORMAT_ZERO_PADDED) {
            _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d:%02d.%02d", 
                        hours, minutes, seconds, centiseconds);
        } else {
            _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d:%02d:%02d.%02d", 
                        hours, minutes, seconds, centiseconds);
        }
    } else {
        if (CLOCK_SHOW_SECONDS) {
            if (format == TIME_FORMAT_FULL_PADDED || format == TIME_FORMAT_ZERO_PADDED) {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d:%02d", hours, minutes, seconds);
            } else {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d:%02d:%02d", hours, minutes, seconds);
            }
        } else {
            if (format == TIME_FORMAT_FULL_PADDED || format == TIME_FORMAT_ZERO_PADDED) {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d", hours, minutes);
            } else {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d:%02d", hours, minutes);
            }
        }
    }

    return TRUE;
}
