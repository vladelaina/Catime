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
#include "timer/timer.h"
#include "tray/tray_animation_core.h"
#include "log.h"

/* ============================================================================
 * External Dependencies
 * ============================================================================ */

extern char FONT_FILE_NAME[MAX_PATH];
extern char FONT_INTERNAL_NAME[MAX_PATH];
extern char CLOCK_TEXT_COLOR[10];
/** Time format now in g_AppConfig.display.time_format */

/* External Preview State from font_manager.c (used by Drag & Drop) */
extern BOOL IS_PREVIEWING;
extern char PREVIEW_FONT_NAME[MAX_PATH];
extern char PREVIEW_INTERNAL_NAME[MAX_PATH];

extern void ResetTimerWithInterval(HWND hwnd);
extern void WriteConfigColor(const char* color);
extern void WriteConfigFont(const char* fontName, BOOL isCustom);
extern void WriteConfigTimeFormat(TimeFormatType format);
extern void WriteConfigShowMilliseconds(BOOL showMilliseconds);

/* ============================================================================
 * Preview State
 * ============================================================================ */

/**
 * @brief Global preview state with type-safe union
 */
typedef struct {
    PreviewType type;
    union {
        char colorHex[32];
        struct {
            char fontName[MAX_PATH];
            char internalName[MAX_PATH];
        } font;
        TimeFormatType timeFormat;
        BOOL showMilliseconds;
        char animationPath[MAX_PATH];
    } data;
    BOOL needsTimerReset;
    BOOL wasWindowVisible;
    BOOL didShowForPreview;
    BOOL createdPreviewTimer;
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

void StartPreview(PreviewType type, const void* data, HWND hwnd) {
    if (IsPreviewActive()) CancelPreview(hwnd);
    
    g_previewState.type = type;
    g_previewState.needsTimerReset = FALSE;
    
    switch (type) {
        case PREVIEW_TYPE_COLOR: {
            const char* colorHex = (const char*)data;
            strncpy_s(g_previewState.data.colorHex, sizeof(g_previewState.data.colorHex), 
                     colorHex, _TRUNCATE);
            break;
        }
        
        case PREVIEW_TYPE_FONT: {
            const char* fontName = (const char*)data;
            strncpy_s(g_previewState.data.font.fontName, MAX_PATH, fontName, _TRUNCATE);
            
            HINSTANCE hInstance = GetModuleHandle(NULL);
            LoadFontByNameAndGetRealName(hInstance, fontName, 
                                        g_previewState.data.font.internalName,
                                        sizeof(g_previewState.data.font.internalName));
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
        
        default:
            g_previewState.type = PREVIEW_TYPE_NONE;
            return;
    }
    
    if (hwnd && type != PREVIEW_TYPE_ANIMATION) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void CancelPreview(HWND hwnd) {
    /* Always cancel animation preview if active */
    if (g_isPreviewActive) {
        CancelAnimationPreview();
    }

    if (!IsPreviewActive()) return;

    BOOL needsRedraw = (g_previewState.type != PREVIEW_TYPE_ANIMATION &&
                        g_previewState.type != PREVIEW_TYPE_NONE);
    BOOL needsTimerReset = (g_previewState.type == PREVIEW_TYPE_MILLISECONDS);
    BOOL needsFontReload = (g_previewState.type == PREVIEW_TYPE_FONT);

    g_previewState.type = PREVIEW_TYPE_NONE;

    if (needsFontReload) {
        HINSTANCE hInstance = GetModuleHandle(NULL);
        if (FONT_FILE_NAME[0] != '\0') {
            LoadFontByNameAndGetRealName(hInstance, FONT_FILE_NAME,
                                        FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        }
    }

    if (needsTimerReset && hwnd) ResetTimerWithInterval(hwnd);
    if (needsRedraw && hwnd) InvalidateRect(hwnd, NULL, TRUE);
}

BOOL ApplyPreview(HWND hwnd) {
    if (!IsPreviewActive()) return FALSE;
    
    PreviewType appliedType = g_previewState.type;
    
    switch (appliedType) {
        case PREVIEW_TYPE_COLOR:
            WriteConfigColor(g_previewState.data.colorHex);
            strncpy_s(CLOCK_TEXT_COLOR, sizeof(CLOCK_TEXT_COLOR),
                     g_previewState.data.colorHex, _TRUNCATE);
            break;
            
        case PREVIEW_TYPE_FONT:
            strncpy_s(FONT_FILE_NAME, sizeof(FONT_FILE_NAME), 
                     g_previewState.data.font.fontName, _TRUNCATE);
            strncpy_s(FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME),
                     g_previewState.data.font.internalName, _TRUNCATE);
            WriteConfigFont(g_previewState.data.font.fontName, FALSE);
            break;
            
        case PREVIEW_TYPE_TIME_FORMAT:
            WriteConfigTimeFormat(g_previewState.data.timeFormat);
            break;
            
        case PREVIEW_TYPE_MILLISECONDS:
            WriteConfigShowMilliseconds(g_previewState.data.showMilliseconds);
            break;
            
        case PREVIEW_TYPE_ANIMATION:
            /* Animation preview applies itself */
            break;
            
        default:
            return FALSE;
    }
    
    g_previewState.type = PREVIEW_TYPE_NONE;
    if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
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

/* ============================================================================
 * Window Visibility Management for Preview
 * ============================================================================ */

void ShowWindowForPreview(HWND hwnd) {
    if (!hwnd) return;

    BOOL isVisible = IsWindowVisible(hwnd);

    extern BOOL CLOCK_SHOW_CURRENT_TIME;
    extern BOOL CLOCK_COUNT_UP;
    extern int CLOCK_TOTAL_TIME;
    extern int countdown_elapsed_time;

    BOOL hasActiveContent = CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP ||
                           (CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME);

    WriteLog(LOG_LEVEL_INFO, "ShowWindowForPreview: visible=%d, showTime=%d, countUp=%d, total=%d, elapsed=%d, hasContent=%d, didShow=%d",
             isVisible, CLOCK_SHOW_CURRENT_TIME, CLOCK_COUNT_UP, CLOCK_TOTAL_TIME, countdown_elapsed_time, hasActiveContent,
             g_previewState.didShowForPreview);

    if (g_previewState.didShowForPreview) {
        WriteLog(LOG_LEVEL_INFO, "Already in preview mode, refreshing display");
        InvalidateRect(hwnd, NULL, TRUE);
        return;
    }

    if (!isVisible || !hasActiveContent) {
        extern BOOL CLOCK_IS_PAUSED;

        g_previewState.wasWindowVisible = isVisible;
        g_previewState.didShowForPreview = TRUE;

        if (!hasActiveContent) {
            WriteLog(LOG_LEVEL_INFO, "No active content, creating preview timer: %d",
                     g_AppConfig.timer.default_start_time > 0 ? g_AppConfig.timer.default_start_time : 300);

            g_previewState.createdPreviewTimer = TRUE;

            CLOCK_SHOW_CURRENT_TIME = FALSE;
            CLOCK_COUNT_UP = FALSE;
            CLOCK_IS_PAUSED = TRUE;

            if (g_AppConfig.timer.default_start_time > 0) {
                CLOCK_TOTAL_TIME = g_AppConfig.timer.default_start_time;
                countdown_elapsed_time = 0;
            } else {
                CLOCK_TOTAL_TIME = 300;
                countdown_elapsed_time = 0;
            }
        } else {
            WriteLog(LOG_LEVEL_INFO, "Window hidden but has active timer, just showing it");
            g_previewState.createdPreviewTimer = FALSE;
        }

        if (!isVisible) {
            ShowWindow(hwnd, SW_SHOW);
        }
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        g_previewState.wasWindowVisible = TRUE;
        g_previewState.didShowForPreview = FALSE;
        g_previewState.createdPreviewTimer = FALSE;
    }
}

void RestoreWindowVisibility(HWND hwnd) {
    if (!hwnd || !g_previewState.didShowForPreview) return;

    WriteLog(LOG_LEVEL_INFO, "RestoreWindowVisibility: was visible=%d, created preview timer=%d",
             g_previewState.wasWindowVisible, g_previewState.createdPreviewTimer);

    extern BOOL CLOCK_SHOW_CURRENT_TIME;
    extern BOOL CLOCK_COUNT_UP;
    extern int CLOCK_TOTAL_TIME;
    extern int countdown_elapsed_time;

    if (g_previewState.createdPreviewTimer) {
        WriteLog(LOG_LEVEL_INFO, "Clearing preview timer that we created");
        CLOCK_TOTAL_TIME = 0;
        countdown_elapsed_time = 0;
        CLOCK_SHOW_CURRENT_TIME = FALSE;
        CLOCK_COUNT_UP = FALSE;
    } else {
        WriteLog(LOG_LEVEL_INFO, "Not clearing timer - was showing existing active timer");
    }

    if (!g_previewState.wasWindowVisible) {
        ShowWindow(hwnd, SW_HIDE);
    } else {
        InvalidateRect(hwnd, NULL, TRUE);
    }

    g_previewState.didShowForPreview = FALSE;
    g_previewState.createdPreviewTimer = FALSE;
}

/* ============================================================================
 * Preview Time Text Generation
 * ============================================================================ */

BOOL GetPreviewTimeText(wchar_t* outText, size_t bufferSize) {
    if (!outText || bufferSize == 0) return FALSE;

    extern BOOL CLOCK_EDIT_MODE;

    if (!CLOCK_EDIT_MODE) {
        return FALSE;
    }

    int previewTime = (g_AppConfig.timer.default_start_time > 0) ?
                      g_AppConfig.timer.default_start_time : 1500;

    int hours = previewTime / 3600;
    int minutes = (previewTime % 3600) / 60;
    int seconds = previewTime % 60;

    TimeFormatType format = GetActiveTimeFormat();
    BOOL showMs = GetActiveShowMilliseconds();

    if (showMs) {
        if (format == TIME_FORMAT_FULL_PADDED) {
            _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d:%02d.00", hours, minutes, seconds);
        } else if (format == TIME_FORMAT_ZERO_PADDED) {
            if (hours > 0) {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d:%02d.00", hours, minutes, seconds);
            } else {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d.00", minutes, seconds);
            }
        } else {
            if (hours > 0) {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d:%02d:%02d.00", hours, minutes, seconds);
            } else if (minutes > 0) {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d:%02d.00", minutes, seconds);
            } else {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d.00", seconds);
            }
        }
    } else {
        if (format == TIME_FORMAT_FULL_PADDED) {
            _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d:%02d", hours, minutes, seconds);
        } else if (format == TIME_FORMAT_ZERO_PADDED) {
            if (hours > 0) {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d:%02d", hours, minutes, seconds);
            } else {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d", minutes, seconds);
            }
        } else {
            if (hours > 0) {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d:%02d:%02d", hours, minutes, seconds);
            } else if (minutes > 0) {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d:%02d", minutes, seconds);
            } else {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d", seconds);
            }
        }
    }

    return TRUE;
}

