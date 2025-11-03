/**
 * @file menu_preview.c
 * @brief Menu option live preview system implementation
 * 
 * Extracted from window_procedure.c for better modularity and reusability.
 */
#include <windows.h>
#include <string.h>
#include "../include/menu_preview.h"
#include "../include/config.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/timer.h"

/* ============================================================================
 * External Dependencies
 * ============================================================================ */

extern char FONT_FILE_NAME[100];
extern char FONT_INTERNAL_NAME[100];
extern char CLOCK_TEXT_COLOR[10];
extern TimeFormatType CLOCK_TIME_FORMAT;
extern BOOL CLOCK_SHOW_MILLISECONDS;

extern void ResetTimerWithInterval(HWND hwnd);
extern void WriteConfigColor(const char* color);
extern void WriteConfigFont(const char* fontName, BOOL isCustom);
extern void WriteConfigTimeFormat(TimeFormatType format);
extern void WriteConfigShowMilliseconds(BOOL showMilliseconds);

/* Forward declarations from tray_animation */
extern void StartAnimationPreview(const char*);
extern void CancelAnimationPreview(void);

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
    if (!IsPreviewActive()) return;
    
    BOOL needsRedraw = (g_previewState.type != PREVIEW_TYPE_ANIMATION && 
                        g_previewState.type != PREVIEW_TYPE_NONE);
    BOOL needsTimerReset = (g_previewState.type == PREVIEW_TYPE_MILLISECONDS);
    
    if (g_previewState.type == PREVIEW_TYPE_ANIMATION) {
        CancelAnimationPreview();
    }
    
    g_previewState.type = PREVIEW_TYPE_NONE;
    
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
    } else {
        strncpy_s(outFontName, bufferSize, FONT_FILE_NAME, _TRUNCATE);
        strncpy_s(outInternalName, bufferSize, FONT_INTERNAL_NAME, _TRUNCATE);
    }
}

TimeFormatType GetActiveTimeFormat(void) {
    return (g_previewState.type == PREVIEW_TYPE_TIME_FORMAT) ?
           g_previewState.data.timeFormat : CLOCK_TIME_FORMAT;
}

BOOL GetActiveShowMilliseconds(void) {
    return (g_previewState.type == PREVIEW_TYPE_MILLISECONDS) ?
           g_previewState.data.showMilliseconds : CLOCK_SHOW_MILLISECONDS;
}

