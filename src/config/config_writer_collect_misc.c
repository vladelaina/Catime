/**
 * @file config_writer_collect_misc.c
 * @brief Hotkey, recent-file, and color option collection
 */
#include "config_writer_internal.h"

#include "color/color.h"
#include "config.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

static BOOL CollectHotkeys(ConfigItemBuilder* builder) {
    static const char* const names[] = {
        "HOTKEY_SHOW_TIME", "HOTKEY_COUNT_UP", "HOTKEY_COUNTDOWN",
        "HOTKEY_QUICK_COUNTDOWN1", "HOTKEY_QUICK_COUNTDOWN2",
        "HOTKEY_QUICK_COUNTDOWN3", "HOTKEY_POMODORO",
        "HOTKEY_TOGGLE_VISIBILITY", "HOTKEY_EDIT_MODE",
        "HOTKEY_PAUSE_RESUME", "HOTKEY_RESTART_TIMER",
        "HOTKEY_CUSTOM_COUNTDOWN", "HOTKEY_TOGGLE_MILLISECONDS",
        "HOTKEY_TOPMOST"
    };
    WORD hotkeys[_countof(names)] = {0};

    ReadConfigHotkeys(&hotkeys[0], &hotkeys[1], &hotkeys[2], &hotkeys[3],
                      &hotkeys[4], &hotkeys[5], &hotkeys[6], &hotkeys[7],
                      &hotkeys[8], &hotkeys[9], &hotkeys[10], &hotkeys[12],
                      &hotkeys[13]);
    ReadCustomCountdownHotkey(&hotkeys[11]);
    for (size_t i = 0; i < _countof(names); ++i) {
        ConfigWriteItem* item = ConfigWriter_ReserveItem(
            builder, INI_SECTION_HOTKEYS, names[i]);
        if (!item) return FALSE;
        HotkeyToString(hotkeys[i], item->value, sizeof(item->value));
    }
    return TRUE;
}

static BOOL CollectRecentFiles(ConfigItemBuilder* builder) {
    int count = g_AppConfig.recent_files.count;

    if (count < 0) count = 0;
    if (count > MAX_RECENT_FILES) count = MAX_RECENT_FILES;
    for (int i = 0; i < MAX_RECENT_FILES; ++i) {
        char key[64];
        const char* path = i < count
            ? g_AppConfig.recent_files.files[i].path
            : "";
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        if (!ConfigWriter_AppendString(builder, INI_SECTION_RECENTFILES,
                                       key, path)) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL IsGradientColor(const char* color) {
    return color && strchr(color, '_') != NULL;
}

static BOOL AppendColorsByType(ConfigWriteItem* item, BOOL gradients,
                               BOOL* first) {
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; ++i) {
        const char* color = COLOR_OPTIONS[i].hexColor;
        if (IsGradientColor(color) != gradients) continue;
        if (!ConfigWriter_AppendListToken(item->value,
                                          sizeof(item->value), color,
                                          first, "COLOR_OPTIONS")) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL CollectColors(ConfigItemBuilder* builder) {
    ConfigWriteItem* item = ConfigWriter_ReserveItem(
        builder, INI_SECTION_COLORS, "COLOR_OPTIONS");
    BOOL first = TRUE;

    if (!item || !AppendColorsByType(item, FALSE, &first) ||
        !AppendColorsByType(item, TRUE, &first) || first) {
        LOG_ERROR("Failed to collect COLOR_OPTIONS for full config write");
        return FALSE;
    }
    return TRUE;
}

BOOL ConfigWriter_CollectHotkeysRecentColors(ConfigItemBuilder* builder) {
    return CollectHotkeys(builder) &&
           CollectRecentFiles(builder) &&
           CollectColors(builder);
}
