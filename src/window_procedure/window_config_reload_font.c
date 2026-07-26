/**
 * @file window_config_reload_font.c
 * @brief Applies validated font changes during configuration reload.
 */

#include "window_procedure/window_config_handlers_internal.h"
#include "config.h"
#include "font.h"
#include "log.h"

#include <string.h>

static BOOL ResolveFontLoadNameForHotReload(const char* configFont,
                                            char* loadName,
                                            size_t loadNameSize) {
    if (!configFont || !*configFont || !loadName || loadNameSize == 0) {
        return FALSE;
    }

    const char* relativePath = ExtractRelativePath(configFont);
    const char* source = relativePath ? relativePath : configFont;
    if (!*source) {
        return FALSE;
    }

    strncpy_s(loadName, loadNameSize, source, _TRUNCATE);
    return TRUE;
}

BOOL WindowConfigInternal_ApplyFont(const char* configFont) {
    if (!configFont || !*configFont || strcmp(configFont, FONT_FILE_NAME) == 0) {
        return FALSE;
    }

    char loadName[MAX_PATH] = {0};
    if (!ResolveFontLoadNameForHotReload(configFont, loadName, sizeof(loadName))) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Ignoring invalid hot-reload font config value '%s'",
                 configFont);
        return FALSE;
    }

    char internalName[MAX_PATH] = {0};
    if (!LoadFontByNameAndGetRealName(GetModuleHandle(NULL), loadName,
                                      internalName, sizeof(internalName))) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Ignoring hot-reload font '%s' because it could not be loaded",
                 configFont);
        return FALSE;
    }

    strncpy_s(FONT_FILE_NAME, sizeof(FONT_FILE_NAME), configFont, _TRUNCATE);
    strncpy_s(FONT_RUNTIME_FILE_NAME, sizeof(FONT_RUNTIME_FILE_NAME),
              configFont, _TRUNCATE);
    strncpy_s(FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME), internalName, _TRUNCATE);
    return TRUE;
}
