/**
 * @file tray_menu_submenus.c
 * @brief Timeout and preset-management submenu builders.
 */

#include "tray_menu_submenus_internal.h"

static BOOL ReadTimeoutStringExact(const char* configPath, const char* key,
                                   char* target, DWORD targetSize) {
    if (!configPath || !key || !target || targetSize == 0) return FALSE;

    char value[MAX_PATH] = {0};
    if (!ReadIniStringExact(INI_SECTION_TIMER, key, "", value,
                            sizeof(value), configPath)) {
        LOG_WARNING("Ignoring %s because the config value is too long", key);
        return FALSE;
    }

    strncpy_s(target, targetSize, value, _TRUNCATE);
    return TRUE;
}

/* Function to read timeout action (extracted from tray_menu.c) */
void ReadTimeoutActionFromConfig() {
    /* Preserve one-time actions: don't override them from config */
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN ||
        CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART ||
        CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SLEEP) {
        return;
    }

    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);

    char value[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE",
                  value, sizeof(value), configPath);

    TimeoutActionType parsedAction = TimeoutActionType_FromStr(value);
    if (parsedAction == TIMEOUT_ACTION_SHUTDOWN ||
        parsedAction == TIMEOUT_ACTION_RESTART ||
        parsedAction == TIMEOUT_ACTION_SLEEP) {
        parsedAction = TIMEOUT_ACTION_MESSAGE;
    }
    CLOCK_TIMEOUT_ACTION = parsedAction;

    /* Hot-reload file path and website URL */
    ReadTimeoutStringExact(configPath, "CLOCK_TIMEOUT_FILE", CLOCK_TIMEOUT_FILE_PATH,
                           MAX_PATH);
    ReadTimeoutStringExact(configPath, "CLOCK_TIMEOUT_WEBSITE", CLOCK_TIMEOUT_WEBSITE_URL,
                           MAX_PATH);
}

/**
 * @brief Build timeout action submenu
 * @param hMenu Parent menu handle
 */
void BuildTimeoutActionSubmenu(HMENU hMenu) {
    HMENU hTimeoutMenu = CreatePopupMenu();
    if (!hTimeoutMenu) return;

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHOW_MESSAGE,
               GetLocalizedString(NULL, L"Show Message"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHOW_TIME ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_TIMEOUT_SHOW_TIME,
               GetLocalizedString(NULL, L"Show Current Time"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_COUNT_UP ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_TIMEOUT_COUNT_UP,
               GetLocalizedString(NULL, L"Count Up"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_LOCK ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_LOCK_SCREEN,
               GetLocalizedString(NULL, L"Lock Screen"));

    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);

    HMENU hFileMenu = CreatePopupMenu();
    if (hFileMenu) {
        int recentFilesCount = g_AppConfig.recent_files.count;
        if (recentFilesCount < 0) recentFilesCount = 0;
        if (recentFilesCount > MAX_RECENT_FILES) recentFilesCount = MAX_RECENT_FILES;
        for (int i = 0; i < recentFilesCount; i++) {
            wchar_t wFileName[MAX_PATH];
            Utf8ToWide(g_AppConfig.recent_files.files[i].name, wFileName, MAX_PATH);

            wchar_t truncatedName[MAX_PATH];
            TruncateFileName(wFileName, truncatedName, _countof(truncatedName), 25);

            BOOL isCurrentFile = (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE &&
                                 strlen(CLOCK_TIMEOUT_FILE_PATH) > 0 &&
                                 strcmp(g_AppConfig.recent_files.files[i].path, CLOCK_TIMEOUT_FILE_PATH) == 0);

            AppendMenuW(hFileMenu, MF_STRING | (isCurrentFile ? MF_CHECKED : 0),
                       CLOCK_IDM_RECENT_FILE_1 + i, truncatedName);
        }

        if (recentFilesCount > 0) {
            AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
        }

        AppendMenuW(hFileMenu, MF_STRING, CLOCK_IDM_BROWSE_FILE,
                   GetLocalizedString(NULL, L"Browse..."));

        if (!AppendMenuW(hTimeoutMenu, MF_POPUP | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE ? MF_CHECKED : MF_UNCHECKED),
                         (UINT_PTR)hFileMenu,
                         GetLocalizedString(NULL, L"Open File/Software"))) {
            DestroyMenu(hFileMenu);
        }
    }

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_WEBSITE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_OPEN_WEBSITE,
               GetLocalizedString(NULL, L"Open Website"));

    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hTimeoutMenu, MF_STRING | MF_GRAYED | MF_DISABLED,
               0,
               GetLocalizedString(NULL, L"Following actions are one-time only"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHUTDOWN,
               GetLocalizedString(NULL, L"Shutdown"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_RESTART,
               GetLocalizedString(NULL, L"Restart"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SLEEP ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SLEEP,
               GetLocalizedString(NULL, L"Sleep"));

    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeoutMenu,
                     GetLocalizedString(NULL, L"Timeout Action"))) {
        DestroyMenu(hTimeoutMenu);
    }
}

/**
 * @brief Build preset management submenu (time options, startup settings, notifications)
 * @param hMenu Parent menu handle
 */
void BuildPresetManagementSubmenu(HMENU hMenu) {

    HMENU hTimeOptionsMenu = CreatePopupMenu();
    if (!hTimeOptionsMenu) return;
    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDC_MODIFY_TIME_OPTIONS,
                GetLocalizedString(NULL, L"Modify Quick Countdown Options"));

    HMENU hStartupSettingsMenu = CreatePopupMenu();
    if (hStartupSettingsMenu) {
        /* Use in-memory variable instead of reading config file each time */
        AppendMenuW(hStartupSettingsMenu, MF_STRING |
                    (strcmp(CLOCK_STARTUP_MODE, "DEFAULT") == 0 ? MF_CHECKED : 0),
                    CLOCK_IDC_SET_COUNTDOWN_TIME,
                    GetLocalizedString(NULL, L"Countdown"));

        AppendMenuW(hStartupSettingsMenu, MF_STRING |
                    (strcmp(CLOCK_STARTUP_MODE, "COUNT_UP") == 0 ? MF_CHECKED : 0),
                    CLOCK_IDC_START_COUNT_UP,
                    GetLocalizedString(NULL, L"Stopwatch"));

        AppendMenuW(hStartupSettingsMenu, MF_STRING |
                    (strcmp(CLOCK_STARTUP_MODE, "POMODORO") == 0 ? MF_CHECKED : 0),
                    CLOCK_IDC_START_POMODORO,
                    GetLocalizedString(NULL, L"Pomodoro"));

        AppendMenuW(hStartupSettingsMenu, MF_STRING |
                    (strcmp(CLOCK_STARTUP_MODE, "SHOW_TIME") == 0 ? MF_CHECKED : 0),
                    CLOCK_IDC_START_SHOW_TIME,
                    GetLocalizedString(NULL, L"Show Current Time"));

        AppendMenuW(hStartupSettingsMenu, MF_STRING |
                    (strcmp(CLOCK_STARTUP_MODE, "NO_DISPLAY") == 0 ? MF_CHECKED : 0),
                    CLOCK_IDC_START_NO_DISPLAY,
                    GetLocalizedString(NULL, L"No Display"));

        AppendMenuW(hStartupSettingsMenu, MF_SEPARATOR, 0, NULL);

        AppendMenuW(hStartupSettingsMenu, MF_STRING |
                (!IsRunningPackagedApp() && IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDC_AUTO_START,
                GetLocalizedString(NULL, L"Start with Windows"));

        if (!AppendMenuW(hTimeOptionsMenu, MF_POPUP, (UINT_PTR)hStartupSettingsMenu,
                         GetLocalizedString(NULL, L"Startup Settings"))) {
            DestroyMenu(hStartupSettingsMenu);
        }
    }

    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDM_NOTIFICATION_SETTINGS,
                GetLocalizedString(NULL, L"Notification Settings"));

    AppendMenuW(hTimeOptionsMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hTimeOptionsMenu, MF_STRING | (CLOCK_WINDOW_TOPMOST ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TOPMOST,
                GetLocalizedString(NULL, L"Always on Top"));

    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeOptionsMenu,
                     GetLocalizedString(NULL, L"Preset Management"))) {
        DestroyMenu(hTimeOptionsMenu);
    }
}

/**
 * @brief Build format submenu (time format options)
 * @param hMenu Parent menu handle
 */
