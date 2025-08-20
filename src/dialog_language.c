/**
 * @file dialog_language.c
 * @brief Dialog localization and language support implementation
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include "../include/dialog_language.h"
#include "../include/language.h"
#include "../resource/resource.h"

/** @brief Maps dialog IDs to their localized title keys */
typedef struct {
    int dialogID;
    wchar_t* titleKey;
} DialogTitleEntry;

/** @brief Maps dialog controls to their localized text with fallback */
typedef struct {
    int dialogID;
    int controlID;
    wchar_t* textKey;
    wchar_t* fallbackText;
} SpecialControlEntry;

/** @brief Data structure for EnumChildWindows callback */
typedef struct {
    HWND hwndDlg;
    int dialogID;
} EnumChildWindowsData;

/** @brief Static mapping of dialog IDs to localization keys for titles */
static DialogTitleEntry g_dialogTitles[] = {
    {IDD_ABOUT_DIALOG, L"About"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, L"Notification Settings"},
    {CLOCK_IDD_POMODORO_LOOP_DIALOG, L"Set Pomodoro Loop Count"},
    {CLOCK_IDD_POMODORO_COMBO_DIALOG, L"Set Pomodoro Time Combination"},
    {CLOCK_IDD_POMODORO_TIME_DIALOG, L"Set Pomodoro Time"},
    {CLOCK_IDD_SHORTCUT_DIALOG, L"Countdown Presets"},
    {CLOCK_IDD_WEBSITE_DIALOG, L"Open Website"},
    {CLOCK_IDD_DIALOG1, L"Set Countdown"},
    {IDD_NO_UPDATE_DIALOG, L"Update Check"},
    {IDD_UPDATE_DIALOG, L"Update Available"}
};

/** @brief Static mapping of special controls requiring custom localization handling */
static SpecialControlEntry g_specialControls[] = {
    {IDD_ABOUT_DIALOG, IDC_ABOUT_TITLE, L"关于", L"About"},
    {IDD_ABOUT_DIALOG, IDC_VERSION_TEXT, L"Version: %hs", L"Version: %hs"},
    {IDD_ABOUT_DIALOG, IDC_BUILD_DATE, L"构建日期:", L"Build Date:"},
    {IDD_ABOUT_DIALOG, IDC_COPYRIGHT, L"COPYRIGHT_TEXT", L"COPYRIGHT_TEXT"},
    {IDD_ABOUT_DIALOG, IDC_CREDITS, L"鸣谢", L"Credits"},

    {IDD_NO_UPDATE_DIALOG, IDC_NO_UPDATE_TEXT, L"NoUpdateRequired", L"You are already using the latest version!"},

    {IDD_UPDATE_DIALOG, IDC_UPDATE_TEXT, L"CurrentVersion: %s\nNewVersion: %s", L"Current Version: %s\nNew Version: %s"},
    {IDD_UPDATE_DIALOG, IDC_UPDATE_EXIT_TEXT, L"The application will exit now", L"The application will exit now"},

    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_CONTENT_GROUP, L"Notification Content", L"Notification Content"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_LABEL1, L"Countdown timeout message:", L"Countdown timeout message:"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_LABEL2, L"Pomodoro timeout message:", L"Pomodoro timeout message:"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_LABEL3, L"Pomodoro cycle complete message:", L"Pomodoro cycle complete message:"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_DISPLAY_GROUP, L"Notification Display", L"Notification Display"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TIME_LABEL, L"Notification display time:", L"Notification display time:"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_OPACITY_LABEL, L"Maximum notification opacity (1-100%):", L"Maximum notification opacity (1-100%):"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_DISABLE_NOTIFICATION_CHECK, L"Disable notifications", L"Disable notifications"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_METHOD_GROUP, L"Notification Method", L"Notification Method"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TYPE_CATIME, L"Catime notification window", L"Catime notification window"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TYPE_OS, L"System notification", L"System notification"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL, L"System modal window", L"System modal window"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_SOUND_LABEL, L"Sound (supports .mp3/.wav/.flac):", L"Sound (supports .mp3/.wav/.flac):"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_VOLUME_LABEL, L"Volume (0-100%):", L"Volume (0-100%):"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_VOLUME_TEXT, L"100%", L"100%"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_NOTIFICATION_OPACITY_TEXT, L"100%", L"100%"},

    {CLOCK_IDD_POMODORO_TIME_DIALOG, CLOCK_IDC_STATIC,
     L"25=25 minutes\\n25h=25 hours\\n25s=25 seconds\\n25 30=25 minutes 30 seconds\\n25 30m=25 hours 30 minutes\\n1 30 20=1 hour 30 minutes 20 seconds",
     L"25=25 minutes\n25h=25 hours\n25s=25 seconds\n25 30=25 minutes 30 seconds\n25 30m=25 hours 30 minutes\n1 30 20=1 hour 30 minutes 20 seconds"},

    {CLOCK_IDD_POMODORO_COMBO_DIALOG, CLOCK_IDC_STATIC,
     L"Enter pomodoro time sequence, separated by spaces:\\n\\n25m = 25 minutes\\n30s = 30 seconds\\n1h30m = 1 hour 30 minutes\\nExample: 25m 5m 25m 10m - work 25min, short break 5min, work 25min, long break 10min",
     L"Enter pomodoro time sequence, separated by spaces:\n\n25m = 25 minutes\n30s = 30 seconds\n1h30m = 1 hour 30 minutes\nExample: 25m 5m 25m 10m - work 25min, short break 5min, work 25min, long break 10min"},

    {CLOCK_IDD_WEBSITE_DIALOG, CLOCK_IDC_STATIC,
     L"Enter the website URL to open when the countdown ends:\\nExample: https://github.com/vladelaina/Catime",
     L"Enter the website URL to open when the countdown ends:\nExample: https://github.com/vladelaina/Catime"},

    {CLOCK_IDD_SHORTCUT_DIALOG, CLOCK_IDC_STATIC,
     L"CountdownPresetDialogStaticText",
     L"Enter numbers (minutes), separated by spaces\n\n25 10 5\n\nThis will create options for 25 minutes, 10 minutes, and 5 minutes"},

    {CLOCK_IDD_DIALOG1, CLOCK_IDC_STATIC,
     L"CountdownDialogStaticText",
     L"25=25 minutes\n25h=25 hours\n25s=25 seconds\n25 30=25 minutes 30 seconds\n25 30m=25 hours 30 minutes\n1 30 20=1 hour 30 minutes 20 seconds\n17 20t=Countdown to 17:20\n9 9 9t=Countdown to 09:09:09"}
};

/** @brief Static mapping for button controls with localized text */
static SpecialControlEntry g_specialButtons[] = {
    {IDD_UPDATE_DIALOG, IDYES, L"Yes", L"Yes"},
    {IDD_UPDATE_DIALOG, IDNO, L"No", L"No"},
    {IDD_UPDATE_DIALOG, IDOK, L"OK", L"OK"},

    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_TEST_SOUND_BUTTON, L"Test", L"Test"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDC_OPEN_SOUND_DIR_BUTTON, L"Audio folder", L"Audio folder"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDOK, L"OK", L"OK"},
    {CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG, IDCANCEL, L"Cancel", L"Cancel"},

    {CLOCK_IDD_POMODORO_LOOP_DIALOG, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"},
    {CLOCK_IDD_POMODORO_COMBO_DIALOG, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"},
    {CLOCK_IDD_POMODORO_TIME_DIALOG, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"},
    {CLOCK_IDD_WEBSITE_DIALOG, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"},
    {CLOCK_IDD_SHORTCUT_DIALOG, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"},
    {CLOCK_IDD_DIALOG1, CLOCK_IDC_BUTTON_OK, L"OK", L"OK"}
};

#define DIALOG_TITLES_COUNT (sizeof(g_dialogTitles) / sizeof(g_dialogTitles[0]))
#define SPECIAL_CONTROLS_COUNT (sizeof(g_specialControls) / sizeof(g_specialControls[0]))
#define SPECIAL_BUTTONS_COUNT (sizeof(g_specialButtons) / sizeof(g_specialButtons[0]))

/**
 * @brief Find localized text for special control with fallback support
 * @param dialogID Dialog resource ID
 * @param controlID Control resource ID
 * @return Localized text or fallback text, NULL if not found
 */
static const wchar_t* FindSpecialControlText(int dialogID, int controlID) {
    for (int i = 0; i < SPECIAL_CONTROLS_COUNT; i++) {
        if (g_specialControls[i].dialogID == dialogID &&
            g_specialControls[i].controlID == controlID) {
            const wchar_t* localizedText = GetLocalizedString(NULL, g_specialControls[i].textKey);
            if (localizedText) {
                return localizedText;
            } else {
                return g_specialControls[i].fallbackText;
            }
        }
    }
    return NULL;
}

/**
 * @brief Find localized text for special button control
 * @param dialogID Dialog resource ID
 * @param controlID Button control resource ID
 * @return Localized button text or NULL if not found
 */
static const wchar_t* FindSpecialButtonText(int dialogID, int controlID) {
    for (int i = 0; i < SPECIAL_BUTTONS_COUNT; i++) {
        if (g_specialButtons[i].dialogID == dialogID &&
            g_specialButtons[i].controlID == controlID) {
            return GetLocalizedString(NULL, g_specialButtons[i].textKey);
        }
    }
    return NULL;
}

/**
 * @brief Get localized title text for dialog
 * @param dialogID Dialog resource ID
 * @return Localized dialog title or NULL if not found
 */
static const wchar_t* GetDialogTitleText(int dialogID) {
    for (int i = 0; i < DIALOG_TITLES_COUNT; i++) {
        if (g_dialogTitles[i].dialogID == dialogID) {
            return GetLocalizedString(NULL, g_dialogTitles[i].titleKey);
        }
    }
    return NULL;
}

/**
 * @brief Extract original text from supported control types
 * @param hwndCtl Handle to control window
 * @param buffer Buffer to store extracted text
 * @param bufferSize Size of buffer in characters
 * @return TRUE if text extracted successfully, FALSE otherwise
 */
static BOOL GetControlOriginalText(HWND hwndCtl, wchar_t* buffer, int bufferSize) {
    wchar_t className[256];
    GetClassNameW(hwndCtl, className, 256);

    /** Only extract text from supported control types */
    if (wcscmp(className, L"Button") == 0 ||
        wcscmp(className, L"Static") == 0 ||
        wcscmp(className, L"ComboBox") == 0 ||
        wcscmp(className, L"Edit") == 0) {
        return GetWindowTextW(hwndCtl, buffer, bufferSize) > 0;
    }

    return FALSE;
}

/**
 * @brief Apply special processing to control text (newline conversion, version formatting)
 * @param hwndCtl Handle to control window
 * @param localizedText Localized text to process
 * @param dialogID Dialog resource ID
 * @param controlID Control resource ID
 * @return TRUE if special processing was applied, FALSE otherwise
 */
static BOOL ProcessSpecialControlText(HWND hwndCtl, const wchar_t* localizedText, int dialogID, int controlID) {
    /** Handle newline conversion for static text controls in specific dialogs */
    if ((dialogID == CLOCK_IDD_POMODORO_COMBO_DIALOG ||
         dialogID == CLOCK_IDD_POMODORO_TIME_DIALOG ||
         dialogID == CLOCK_IDD_WEBSITE_DIALOG ||
         dialogID == CLOCK_IDD_SHORTCUT_DIALOG ||
         dialogID == CLOCK_IDD_DIALOG1) &&
        controlID == CLOCK_IDC_STATIC) {
        wchar_t processedText[1024];
        const wchar_t* src = localizedText;
        wchar_t* dst = processedText;

        /** Convert escaped newlines and preserve actual newlines */
        while (*src) {
            if (src[0] == L'\\' && src[1] == L'n') {
                *dst++ = L'\n';
                src += 2;
            }
            else if (src[0] == L'\n') {
                *dst++ = L'\n';
                src++;
            }
            else {
                *dst++ = *src++;
            }
        }
        *dst = L'\0';

        SetWindowTextW(hwndCtl, processedText);
        return TRUE;
    }

    /** Handle version text formatting with current version */
    if (controlID == IDC_VERSION_TEXT && dialogID == IDD_ABOUT_DIALOG) {
        wchar_t versionText[256];
        const wchar_t* localizedVersionFormat = GetLocalizedString(NULL, L"Version: %hs");
        if (localizedVersionFormat) {
            StringCbPrintfW(versionText, sizeof(versionText), localizedVersionFormat, CATIME_VERSION);
        } else {
            StringCbPrintfW(versionText, sizeof(versionText), localizedText, CATIME_VERSION);
        }
        SetWindowTextW(hwndCtl, versionText);
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief EnumChildWindows callback to localize individual dialog controls
 * @param hwndCtl Handle to child control window
 * @param lParam Pointer to EnumChildWindowsData structure
 * @return TRUE to continue enumeration, FALSE to stop
 */
static BOOL CALLBACK EnumChildProc(HWND hwndCtl, LPARAM lParam) {
    EnumChildWindowsData* data = (EnumChildWindowsData*)lParam;
    HWND hwndDlg = data->hwndDlg;
    int dialogID = data->dialogID;

    int controlID = GetDlgCtrlID(hwndCtl);
    if (controlID == 0) {
        return TRUE;
    }

    /** Check for special controls requiring custom handling */
    const wchar_t* specialText = FindSpecialControlText(dialogID, controlID);
    if (specialText) {
        if (ProcessSpecialControlText(hwndCtl, specialText, dialogID, controlID)) {
            return TRUE;
        }
        SetWindowTextW(hwndCtl, specialText);
        return TRUE;
    }

    /** Check for special button text */
    const wchar_t* buttonText = FindSpecialButtonText(dialogID, controlID);
    if (buttonText) {
        SetWindowTextW(hwndCtl, buttonText);
        return TRUE;
    }

    /** Apply generic localization for standard controls */
    wchar_t originalText[512] = {0};
    if (GetControlOriginalText(hwndCtl, originalText, 512) && originalText[0] != L'\0') {
        const wchar_t* localizedText = GetLocalizedString(NULL, originalText);
        if (localizedText && wcscmp(localizedText, originalText) != 0) {
            SetWindowTextW(hwndCtl, localizedText);
        }
    }

    return TRUE;
}

/**
 * @brief Initialize dialog language support system
 * @return TRUE if initialization successful
 */
BOOL InitDialogLanguageSupport(void) {
    return TRUE;
}

/**
 * @brief Apply localization to dialog and all its child controls
 * @param hwndDlg Handle to dialog window
 * @param dialogID Dialog resource ID for lookup
 * @return TRUE if localization applied successfully, FALSE on error
 */
BOOL ApplyDialogLanguage(HWND hwndDlg, int dialogID) {
    if (!hwndDlg) return FALSE;

    /** Set localized dialog title */
    const wchar_t* titleText = GetDialogTitleText(dialogID);
    if (titleText) {
        SetWindowTextW(hwndDlg, titleText);
    }

    /** Enumerate and localize all child controls */
    EnumChildWindowsData data = {
        .hwndDlg = hwndDlg,
        .dialogID = dialogID
    };

    EnumChildWindows(hwndDlg, EnumChildProc, (LPARAM)&data);

    return TRUE;
}

/**
 * @brief Get localized string for specific dialog control
 * @param dialogID Dialog resource ID
 * @param controlID Control resource ID (-1 for dialog title)
 * @return Localized string or NULL if not found
 */
const wchar_t* GetDialogLocalizedString(int dialogID, int controlID) {
    /** Check for special control text first */
    const wchar_t* specialText = FindSpecialControlText(dialogID, controlID);
    if (specialText) {
        return specialText;
    }

    /** Check for special button text */
    const wchar_t* buttonText = FindSpecialButtonText(dialogID, controlID);
    if (buttonText) {
        return buttonText;
    }

    /** Handle dialog title request */
    if (controlID == -1) {
        return GetDialogTitleText(dialogID);
    }

    return NULL;
}