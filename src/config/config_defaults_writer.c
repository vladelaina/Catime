/**
 * @file config_defaults_writer.c
 * @brief Atomic creation of the default configuration file.
 */

#include "config_defaults_internal.h"

static BOOL WidePathToUtf8Local(const wchar_t* wPath, char* utf8Path, size_t utf8PathSize) {
    if (!utf8Path || utf8PathSize == 0) return FALSE;

    utf8Path[0] = '\0';
    if (!wPath || utf8PathSize > INT_MAX) return FALSE;

    if (WideCharToMultiByte(CP_UTF8, 0, wPath, -1,
                            utf8Path, (int)utf8PathSize, NULL, NULL) <= 0) {
        utf8Path[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL CreateDefaultConfigTempPath(const wchar_t* wConfigPath,
                                        wchar_t* wTempPath,
                                        size_t wTempPathSize) {
    if (!wConfigPath || !wTempPath || wTempPathSize < MAX_PATH) return FALSE;
    wTempPath[0] = L'\0';

    wchar_t wConfigDir[MAX_PATH];
    if (!ExtractDirectoryW(wConfigPath, wConfigDir, MAX_PATH)) return FALSE;

    return GetTempFileNameW(wConfigDir, L"ctd", 0, wTempPath) != 0;
}

int DetectSystemLanguage(void) {
    return (int)GetSystemDefaultLanguage();
}

const char* GetDetectedSystemLanguageConfigKey(void) {
    int detectedLang = DetectSystemLanguage();
    if (detectedLang < 0 || detectedLang >= APP_LANG_COUNT) {
        return GetLanguageConfigKey(APP_LANG_ENGLISH);
    }

    return GetLanguageConfigKey((AppLanguage)detectedLang);
}

BOOL WriteDefaultsToConfig(const char* config_path) {
    if (!config_path) return FALSE;

    /* Convert path to wide char for _wfopen */
    wchar_t wconfig_path[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH) == 0) {
        LOG_ERROR("Failed to convert default config path: %s", config_path);
        return FALSE;
    }

    wchar_t wtemp_path[MAX_PATH] = {0};
    if (!CreateDefaultConfigTempPath(wconfig_path, wtemp_path, MAX_PATH)) {
        LOG_ERROR("Failed to create default config temp file: %s", config_path);
        return FALSE;
    }

    char temp_path[MAX_PATH] = {0};
    WidePathToUtf8Local(wtemp_path, temp_path, sizeof(temp_path));

    /* Open temp file for writing in UTF-8 mode (no BOM needed) */
    FILE* f = _wfopen(wtemp_path, L"wb");
    if (!f) {
        DeleteFileW(wtemp_path);
        LOG_ERROR("Failed to open default config temp file for writing: %s",
                  temp_path[0] ? temp_path : config_path);
        return FALSE;
    }

    /* Track current section to insert help docs */
    const char* lastSection = "";

    /* Write all metadata-defined defaults */
    for (int i = 0; i < CONFIG_METADATA_COUNT; i++) {
        const ConfigItemMeta* item = &CONFIG_METADATA[i];

        /* Write section header if section changed */
        if (strcmp(item->section, lastSection) != 0) {
            fprintf(f, "[%s]\n", item->section);
            lastSection = item->section;
        }

        /* Write key=value */
        switch (item->type) {
            case CONFIG_TYPE_INT:
                fprintf(f, "%s=%s\n", item->key, item->defaultValue);
                break;

            case CONFIG_TYPE_BOOL:
            case CONFIG_TYPE_STRING:
            case CONFIG_TYPE_ENUM:
            default:
                fprintf(f, "%s=%s\n", item->key, item->defaultValue);
                break;
        }

        /* Check if we just finished writing Display section */
        BOOL isLastDisplayItem = (i + 1 >= CONFIG_METADATA_COUNT ||
                                   strcmp(CONFIG_METADATA[i + 1].section, INI_SECTION_DISPLAY) != 0);
        if (strcmp(item->section, INI_SECTION_DISPLAY) == 0 && isLastDisplayItem) {
            fputs(";========================================================\n", f);
            fputs("; Display section help (hot reload supported)\n", f);
            fputs(";========================================================\n", f);
            fputs("; MOVE_STEP_SMALL: arrow key move step in edit mode (unit: pixels).\n", f);
            fputs(";   Controls how far the window moves with each arrow key press.\n", f);
            fputs(";   Range: 1-500 pixels\n", f);
            fputs(";   Default: 10 pixels\n", f);
            fputs(";\n", f);
            fputs("; MOVE_STEP_LARGE: Ctrl+arrow key move step in edit mode (unit: pixels).\n", f);
            fputs(";   Controls how far the window moves with Ctrl+arrow key.\n", f);
            fputs(";   Range: 1-500 pixels\n", f);
            fputs(";   Default: 50 pixels\n", f);
            fputs(";\n", f);
            fputs("; OPACITY_STEP_NORMAL: mouse wheel opacity adjustment step (unit: percent).\n", f);
            fputs(";   Controls opacity change when scrolling over tray icon.\n", f);
            fputs(";   Range: 1-100%\n", f);
            fputs(";   Default: 1%\n", f);
            fputs(";\n", f);
            fputs("; OPACITY_STEP_FAST: Ctrl+mouse wheel opacity adjustment step (unit: percent).\n", f);
            fputs(";   Controls opacity change when scrolling with Ctrl held.\n", f);
            fputs(";   Range: 1-100%\n", f);
            fputs(";   Default: 5%\n", f);
            fputs(";\n", f);
            fputs("; SCALE_STEP_NORMAL: mouse wheel scale adjustment step (unit: percent).\n", f);
            fputs(";   Controls window scale change when scrolling over window.\n", f);
            fputs(";   Range: 1-100%\n", f);
            fputs(";   Default: 10%\n", f);
            fputs(";\n", f);
            fputs("; SCALE_STEP_FAST: Ctrl+mouse wheel scale adjustment step (unit: percent).\n", f);
            fputs(";   Controls window scale change when scrolling with Ctrl held.\n", f);
            fputs(";   Range: 1-100%\n", f);
            fputs(";   Default: 15%\n", f);
            fputs(";========================================================\n", f);
        }

        /* Check if we just finished writing Animation section */
        BOOL isLastAnimationItem = (i + 1 >= CONFIG_METADATA_COUNT ||
                                     strcmp(CONFIG_METADATA[i + 1].section, "Animation") != 0);
        if (strcmp(item->section, "Animation") == 0 && isLastAnimationItem) {
            fputs(";========================================================\n", f);
            fputs("; Animation options help (hot reload supported)\n", f);
            fputs(";========================================================\n", f);
            fputs("; ANIMATION_SPEED_METRIC: ORIGINAL, MEMORY, CPU, TIMER, or FIXED.\n", f);
            fputs("; ANIMATION_FIXED_SPEED_PERCENT: playback speed used by FIXED mode.\n", f);
            fputs(";   Range: 10-3000 (0.1x-30x); default: 200 (2x).\n", f);
            fputs(";\n", f);
            fputs("; ANIMATION_SPEED_DEFAULT: base speed scale at 0% (unit: percent).\n", f);
            fputs(";   100 = 1x speed, 200 = 2x, 50 = 0.5x.\n", f);
            fputs(";   Works with ANIMATION_SPEED_MAP_* breakpoints via linear interpolation.\n", f);
            fputs(";\n", f);
            fputs("; PERCENT_ICON_TEXT_COLOR: generated tray icon text color (CPU/MEM/Battery/Caps Lock).\n", f);
            fputs(";   Format: auto (theme-aware), #RRGGBB, or R,G,B (0-255).\n", f);
            fputs(";   'auto' = automatic text color based on Windows theme (Win10 1607+)\n", f);
            fputs(";            On older systems or Win7, 'auto' defaults to black.\n", f);
            fputs(";   Example: auto, #000000 (black), #FFFFFF (white), 255,0,0 (red)\n", f);
            fputs(";\n", f);
            fputs("; PERCENT_ICON_BG_COLOR: generated tray icon background color (CPU/MEM/Battery/Caps Lock).\n", f);
            fputs(";   Format: transparent, #RRGGBB, or R,G,B (0-255).\n", f);
            fputs(";   'transparent' = no background, blends with taskbar\n", f);
            fputs(";   Example: transparent, #FFFFFF (white bg), #000000 (black bg)\n", f);
            fputs(";\n", f);
            fputs("; ANIMATION_FOLDER_INTERVAL_MS: base animation playback speed (unit: milliseconds).\n", f);
            fputs(";   Controls how fast the animation plays (higher = slower, lower = faster).\n", f);
            fputs(";   Affects folder sequences and static images (.ico/.png/.bmp/.jpg/.jpeg/.gif/.webp/.tif/.tiff).\n", f);
            fputs(";   Standalone GIF/WebP/ANI files honor embedded per-frame delays before speed scaling.\n", f);
            fputs(";   Default: 150ms (~6.7 fps)\n", f);
            fputs(";   Suggested range: 50-500ms\n", f);
            fputs(";\n", f);
            fputs("; ANIMATION_MIN_INTERVAL_MS: optional minimum speed limit (unit: milliseconds).\n", f);
            fputs(";   Adds an extra lower speed limit on top of system optimizations.\n", f);
            fputs(";   0     => use system default (recommended for most users)\n", f);
            fputs(";   N>0   => enforce minimum N ms per frame (e.g., 100 = max 10 fps)\n", f);
            fputs(";   Note: System already uses high-precision timing with fixed 50ms tray updates\n", f);
            fputs(";         to eliminate flicker/stutter. This setting is optional.\n", f);
            fputs(";   Use case: Set to 100+ on very low-end devices to reduce CPU usage.\n", f);
            fputs(";========================================================\n", f);
        }

        /* Check if we just finished writing Hotkeys section */
        BOOL isLastHotkeyItem = (i + 1 >= CONFIG_METADATA_COUNT ||
                                 strcmp(CONFIG_METADATA[i + 1].section, INI_SECTION_HOTKEYS) != 0);
        if (strcmp(item->section, INI_SECTION_HOTKEYS) == 0 && isLastHotkeyItem) {
            fputs(";========================================================\n", f);
            fputs("; Hotkeys section help (hot reload supported)\n", f);
            fputs(";========================================================\n", f);
            fputs("; Value examples: A, F8, Ctrl+Shift+Alt+F5, None, 0xNN (hex VK)\n", f);
            fputs(";  - Modifiers: Ctrl, Shift, Alt (optional; combine with '+')\n", f);
            fputs(";  - Keys: A-Z, 0-9, F1..F24, Backspace, Tab, Enter, Esc, Space,\n", f);
            fputs(";           PageUp, PageDown, End, Home, Left, Up, Right, Down, Insert, Delete,\n", f);
            fputs(";           Num0..Num9, Num*, Num+, Num-, Num., Num/\n", f);
            fputs(";  - Examples: A  |  F8  |  Ctrl+Shift+K  |  Alt+F12  |  None  |  0x5B\n", f);
            fputs(";  - Note: Some combinations may be reserved by the system or other apps.\n", f);
            fputs(";\n", f);
            fputs("; Keys in [Hotkeys]:\n", f);
            fputs(";   HOTKEY_SHOW_TIME           - Toggle show current time\n", f);
            fputs(";   HOTKEY_COUNT_UP            - Start count-up timer\n", f);
            fputs(";   HOTKEY_COUNTDOWN           - Start countdown timer\n", f);
            fputs(";   HOTKEY_QUICK_COUNTDOWN1    - Quick countdown slot 1\n", f);
            fputs(";   HOTKEY_QUICK_COUNTDOWN2    - Quick countdown slot 2\n", f);
            fputs(";   HOTKEY_QUICK_COUNTDOWN3    - Quick countdown slot 3\n", f);
            fputs(";   HOTKEY_POMODORO            - Start Pomodoro\n", f);
            fputs(";   HOTKEY_TOGGLE_VISIBILITY   - Toggle window visibility\n", f);
            fputs(";   HOTKEY_EDIT_MODE           - Toggle edit mode\n", f);
            fputs(";   HOTKEY_PAUSE_RESUME        - Pause/Resume timer\n", f);
            fputs(";   HOTKEY_RESTART_TIMER       - Restart current timer\n", f);
            fputs(";   HOTKEY_CUSTOM_COUNTDOWN    - Custom countdown\n", f);
            fputs(";   HOTKEY_TOGGLE_MILLISECONDS - Toggle milliseconds display\n", f);
            fputs(";   HOTKEY_TOPMOST             - Toggle topmost\n", f);
            fputs(";========================================================\n", f);
        }

        /* Check if we just finished writing Colors section */
        BOOL isLastColorItem = (i + 1 >= CONFIG_METADATA_COUNT ||
                                strcmp(CONFIG_METADATA[i + 1].section, INI_SECTION_COLORS) != 0);
        if (strcmp(item->section, INI_SECTION_COLORS) == 0 && isLastColorItem) {
            fputs(";========================================================\n", f);
            fputs("; Colors section help (hot reload supported)\n", f);
            fputs(";========================================================\n", f);
            fputs("; COLOR_OPTIONS: comma-separated quick color list used by dialogs/menus.\n", f);
            fputs(";   Token format: #RRGGBB or gradient #RRGGBB_#RRGGBB.\n", f);
            fputs(";   Whitespace is allowed around commas; duplicates are ignored.\n", f);
            fputs(";   Example: COLOR_OPTIONS=#FFFFFF,#E3E3E5,#000000,#FFFFFF_#00FFFF\n", f);
            fputs(";========================================================\n", f);
        }
    }

    /* Write RecentFiles section */
    fprintf(f, "[%s]\n", INI_SECTION_RECENTFILES);
    for (int i = 1; i <= MAX_RECENT_FILES; i++) {
        fprintf(f, "CLOCK_RECENT_FILE_%d=\n", i);
    }

    BOOL writeOk = !ferror(f);
    if (fclose(f) != 0) {
        writeOk = FALSE;
    }
    if (!writeOk) {
        DeleteFileW(wtemp_path);
        LOG_ERROR("Failed to write default config: %s", config_path);
        return FALSE;
    }

    if (!MoveFileExW(wtemp_path, wconfig_path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(wtemp_path);
        LOG_ERROR("Failed to replace default config: %s (error=%lu)",
                  config_path, GetLastError());
        return FALSE;
    }

    InvalidateIniCache();
    return TRUE;
}

BOOL CreateDefaultConfig(const char* configPath) {
    if (!configPath) return FALSE;
    const char* detectedLangName = GetDetectedSystemLanguageConfigKey();

    /* Write all defaults */
    if (!WriteDefaultsToConfig(configPath)) {
        LOG_ERROR("Failed to create default config: %s", configPath);
        return FALSE;
    }

    /* Override language with detected value */
    BOOL result = TRUE;
    if (!WriteIniString(INI_SECTION_GENERAL, "LANGUAGE", detectedLangName, configPath)) {
        LOG_ERROR("Failed to write detected language to default config: %s", detectedLangName);
        result = FALSE;
    }
    return result;
}
