#include "menu_preview_internal.h"
#include "config.h"
#include "font.h"
#include "text_effect.h"

extern char FONT_INTERNAL_NAME[MAX_PATH];
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];
extern BOOL IS_PREVIEWING;
extern char PREVIEW_FONT_NAME[MAX_PATH];
extern char PREVIEW_INTERNAL_NAME[MAX_PATH];

void GetActiveColor(char* outColor, size_t bufferSize) {
    if (!outColor || !bufferSize) return;
    const char* value = g_previewState.type == PREVIEW_TYPE_COLOR
        ? g_previewState.data.colorHex : CLOCK_TEXT_COLOR;
    strncpy_s(outColor, bufferSize, value, _TRUNCATE);
}

void GetActiveFont(char* outFontName, char* outInternalName, size_t bufferSize) {
    if (!outFontName || !outInternalName || !bufferSize) return;
    if (g_previewState.type == PREVIEW_TYPE_FONT) {
        strncpy_s(outFontName, bufferSize, g_previewState.data.font.fontName, _TRUNCATE);
        strncpy_s(outInternalName, bufferSize, g_previewState.data.font.internalName, _TRUNCATE);
    } else if (IS_PREVIEWING) {
        strncpy_s(outFontName, bufferSize, PREVIEW_FONT_NAME, _TRUNCATE);
        strncpy_s(outInternalName, bufferSize, PREVIEW_INTERNAL_NAME, _TRUNCATE);
    } else {
        strncpy_s(outFontName, bufferSize, FONT_RUNTIME_FILE_NAME, _TRUNCATE);
        strncpy_s(outInternalName, bufferSize, FONT_INTERNAL_NAME, _TRUNCATE);
    }
}

TimeFormatType GetActiveTimeFormat(void) {
    return g_previewState.type == PREVIEW_TYPE_TIME_FORMAT
        ? g_previewState.data.timeFormat : g_AppConfig.display.time_format.format;
}
BOOL GetActiveShowMilliseconds(void) {
    return g_previewState.type == PREVIEW_TYPE_MILLISECONDS
        ? g_previewState.data.showMilliseconds
        : g_AppConfig.display.time_format.show_milliseconds;
}
BOOL GetActiveShowSeconds(void) {
    return g_previewState.type == PREVIEW_TYPE_SECONDS
        ? g_previewState.data.showSeconds : CLOCK_SHOW_SECONDS;
}
BOOL GetActiveUse24Hour(void) {
    return g_previewState.type == PREVIEW_TYPE_24HOUR
        ? g_previewState.data.use24Hour : CLOCK_USE_24HOUR;
}
EffectType GetActiveEffect(void) {
    if (GetActiveShowMilliseconds()) return EFFECT_TYPE_NONE;
    if (g_previewState.type == PREVIEW_TYPE_EFFECT)
        return g_previewState.data.effect;
    return TextEffect_IsSelectable(CLOCK_TEXT_EFFECT)
        ? (EffectType)CLOCK_TEXT_EFFECT : EFFECT_TYPE_NONE;
}
