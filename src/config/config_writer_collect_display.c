/**
 * @file config_writer_collect_display.c
 * @brief General and display configuration collection
 */
#include "config_writer_internal.h"

#include "config.h"
#include "config/config_defaults.h"
#include "font.h"
#include "language.h"
#include "text_effect.h"
#include "timer/timer.h"
#include "window.h"
#include "../../resource/resource.h"

BOOL ConfigWriter_CollectGeneralDisplay(ConfigItemBuilder* builder) {
    if (!ConfigWriter_AppendString(builder, INI_SECTION_GENERAL,
                                   "CONFIG_VERSION", CATIME_VERSION) ||
        !ConfigWriter_AppendString(builder, INI_SECTION_GENERAL, "LANGUAGE",
                                   GetLanguageConfigKey(
                                       GetCurrentLanguage())) ||
        !ConfigWriter_AppendBool(builder, INI_SECTION_GENERAL,
                                 "SHORTCUT_CHECK_DONE",
                                 IsShortcutCheckDone())) {
        return FALSE;
    }

    if (!ConfigWriter_AppendString(builder, INI_SECTION_DISPLAY,
                                   "CLOCK_TEXT_COLOR", CLOCK_TEXT_COLOR) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_DISPLAY,
                                "CLOCK_BASE_FONT_SIZE",
                                CLOCK_BASE_FONT_SIZE) ||
        !ConfigWriter_AppendString(builder, INI_SECTION_DISPLAY,
                                   "FONT_FILE_NAME", FONT_FILE_NAME) ||
        !ConfigWriter_AppendFloat(builder, INI_SECTION_DISPLAY,
                                  "WINDOW_SCALE", CLOCK_WINDOW_SCALE) ||
        !ConfigWriter_AppendFloat(builder, INI_SECTION_DISPLAY,
                                  "PLUGIN_SCALE",
                                  PLUGIN_FONT_SCALE_FACTOR) ||
        !ConfigWriter_AppendBool(builder, INI_SECTION_DISPLAY,
                                 "WINDOW_TOPMOST", CLOCK_WINDOW_TOPMOST) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_DISPLAY,
                                "WINDOW_OPACITY", CLOCK_WINDOW_OPACITY)) {
        return FALSE;
    }

    if (!ConfigWriter_AppendInt(builder, INI_SECTION_DISPLAY,
                                "MOVE_STEP_SMALL",
                                g_AppConfig.display.move_step_small) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_DISPLAY,
                                "MOVE_STEP_LARGE",
                                g_AppConfig.display.move_step_large) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_DISPLAY,
                                "OPACITY_STEP_NORMAL",
                                g_AppConfig.display.opacity_step_normal) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_DISPLAY,
                                "OPACITY_STEP_FAST",
                                g_AppConfig.display.opacity_step_fast) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_DISPLAY,
                                "SCALE_STEP_NORMAL",
                                g_AppConfig.display.scale_step_normal) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_DISPLAY,
                                "SCALE_STEP_FAST",
                                g_AppConfig.display.scale_step_fast) ||
        !ConfigWriter_AppendString(
            builder, INI_SECTION_DISPLAY, "TEXT_EFFECT",
            TextEffect_ToConfigString(CLOCK_TEXT_EFFECT))) {
        return FALSE;
    }
    return TRUE;
}
