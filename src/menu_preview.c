#include <windows.h>
#include <string.h>
#include "menu_preview.h"
#include "menu_preview_internal.h"
#include "config.h"
#include "font.h"
#include "color/color.h"
#include "color/color_parser.h"
#include "drawing/drawing_effect.h"
#include "tray/tray_animation_core.h"
#include "window_procedure/window_commands.h"
#include "notification.h"
#include "log.h"
extern char FONT_FILE_NAME[MAX_PATH];
extern char FONT_INTERNAL_NAME[MAX_PATH];
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];
extern BOOL IS_PREVIEWING;
extern char PREVIEW_FONT_NAME[MAX_PATH];
extern char PREVIEW_INTERNAL_NAME[MAX_PATH];
extern void ResetTimerWithInterval(HWND hwnd);
extern BOOL WriteConfigColor(const char* color);
extern BOOL WriteConfigTimeFormat(TimeFormatType format);
extern BOOL WriteConfigShowMilliseconds(BOOL showMilliseconds);
PreviewState g_previewState = {PREVIEW_TYPE_NONE};
static BOOL ShouldCleanupPreviewEffectBuffers(EffectType previousPreviewEffect,
                                              EffectType nextPreviewEffect) {
    return TextEffect_UsesSharedEffectBuffer(previousPreviewEffect) &&
           !TextEffect_UsesSharedEffectBuffer(nextPreviewEffect) &&
           !TextEffect_UsesSharedEffectBuffer(CLOCK_TEXT_EFFECT);
}
BOOL IsPreviewActive(void) {
    return g_previewState.type != PREVIEW_TYPE_NONE;
}
PreviewType GetActivePreviewType(void) {
    return g_previewState.type;
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
    if (FONT_RUNTIME_FILE_NAME[0] == '\0') {
        return FALSE;
    }
    const char* loadName = FONT_RUNTIME_FILE_NAME;
    if (IsFontsFolderPath(FONT_RUNTIME_FILE_NAME)) {
        const char* relativePath = ExtractRelativePath(FONT_RUNTIME_FILE_NAME);
        if (!relativePath || relativePath[0] == '\0') {
            return FALSE;
        }
        loadName = relativePath;
    }
    HINSTANCE hInstance = GetModuleHandle(NULL);
    if (!LoadFontByNameAndGetRealName(hInstance, loadName,
                                      FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
        LOG_WARNING("Preview: failed to restore font after preview: %s",
                    FONT_RUNTIME_FILE_NAME);
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
        EffectType previousEffect = g_previewState.data.effect;
        g_previewState.data.effect = effect;
        if (ShouldCleanupPreviewEffectBuffers(previousEffect, effect)) {
            CleanupDrawingEffects();
        }
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
        RefreshCustomTextDisplayDialogFont();
        if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
        return;
    }
    if (IsPreviewActive()) CancelPreview(hwnd);
    g_previewState.type = type;
    switch (type) {
        case PREVIEW_TYPE_COLOR: {
            const char* colorHex = (const char*)data;
            strncpy_s(g_previewState.data.colorHex, sizeof(g_previewState.data.colorHex),
                     colorHex, _TRUNCATE);
            if (hwnd) ResetTimerWithInterval(hwnd);
            RefreshToastNotificationColors();
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
            RefreshCustomTextDisplayDialogFont();
            break;
        }
        case PREVIEW_TYPE_TIME_FORMAT:
            g_previewState.data.timeFormat = *(TimeFormatType*)data;
            break;
        case PREVIEW_TYPE_MILLISECONDS:
            g_previewState.data.showMilliseconds = *(BOOL*)data;
            if (hwnd) ResetTimerWithInterval(hwnd);
            break;
        case PREVIEW_TYPE_SECONDS:
            g_previewState.data.showSeconds = *(BOOL*)data;
            if (hwnd) ResetTimerWithInterval(hwnd);
            break;
        case PREVIEW_TYPE_24HOUR:
            g_previewState.data.use24Hour = *(BOOL*)data;
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
    if (g_isPreviewActive || g_previewState.type == PREVIEW_TYPE_ANIMATION) {
        CancelAnimationPreview();
    }
    if (!IsPreviewActive()) return;
    BOOL needsRedraw = (g_previewState.type != PREVIEW_TYPE_ANIMATION &&
                        g_previewState.type != PREVIEW_TYPE_NONE);
    BOOL needsTimerReset = (g_previewState.type == PREVIEW_TYPE_MILLISECONDS ||
                            g_previewState.type == PREVIEW_TYPE_SECONDS ||
                            g_previewState.type == PREVIEW_TYPE_COLOR);
    BOOL needsColorRefresh = (g_previewState.type == PREVIEW_TYPE_COLOR);
    BOOL needsFontReload = (g_previewState.type == PREVIEW_TYPE_FONT);
    BOOL needsEffectCleanup = (g_previewState.type == PREVIEW_TYPE_EFFECT &&
                               ShouldCleanupPreviewEffectBuffers(
                                   g_previewState.data.effect,
                                   (EffectType)CLOCK_TEXT_EFFECT));
    if (needsFontReload) {
        RestoreConfiguredFontAfterPreview();
    }
    g_previewState.type = PREVIEW_TYPE_NONE;
    if (needsFontReload) {
        RefreshCustomTextDisplayDialogFont();
    }
    if (needsColorRefresh) {
        RefreshToastNotificationColors();
    }
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
            if (!WriteConfigFont(g_previewState.data.font.fontName, FALSE) ||
                !FlushConfigToDisk()) {
                CancelPreview(hwnd);
                return FALSE;
            }
            strncpy_s(FONT_FILE_NAME, sizeof(FONT_FILE_NAME),
                     g_previewState.data.font.fontName, _TRUNCATE);
            strncpy_s(FONT_RUNTIME_FILE_NAME, sizeof(FONT_RUNTIME_FILE_NAME),
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
        case PREVIEW_TYPE_SECONDS:
            if (!WriteConfigKeyValue("CLOCK_SHOW_SECONDS",
                                     g_previewState.data.showSeconds ? "TRUE" : "FALSE")) {
                return FALSE;
            }
            CLOCK_SHOW_SECONDS = g_previewState.data.showSeconds;
            if (hwnd) ResetTimerWithInterval(hwnd);
            break;
        case PREVIEW_TYPE_24HOUR:
            if (!WriteConfigKeyValue("CLOCK_USE_24HOUR",
                                     g_previewState.data.use24Hour ? "TRUE" : "FALSE")) {
                return FALSE;
            }
            CLOCK_USE_24HOUR = g_previewState.data.use24Hour;
            break;
        case PREVIEW_TYPE_ANIMATION:
            break;
        case PREVIEW_TYPE_EFFECT:
            break;
        default:
            return FALSE;
    }
    g_previewState.type = PREVIEW_TYPE_NONE;
    if (appliedType == PREVIEW_TYPE_FONT) {
        RefreshCustomTextDisplayDialogFont();
    }
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
