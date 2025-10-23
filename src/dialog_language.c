/**
 * @file dialog_language.c
 * @brief Dialog localization and language support implementation
 * @version 2.0 - Refactored for better modularity and reduced code duplication
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include "../include/dialog_language.h"
#include "../include/language.h"
#include "../resource/resource.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Buffer size constants for text handling */
#define CLASS_NAME_MAX 256
#define CONTROL_TEXT_MAX 512
#define LARGE_TEXT_MAX 1024
#define VERSION_TEXT_MAX 256

/** @brief Special control ID for dialog title requests */
#define DIALOG_TITLE_ID -1

/** @brief Array size calculation macro */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ============================================================================
 * Type definitions
 * ============================================================================ */

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

/* ============================================================================
 * Static lookup tables
 * ============================================================================ */

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

/* ============================================================================
 * Helper functions for text processing
 * ============================================================================ */

/**
 * @brief Convert escaped newlines (\\n) to actual newlines
 * @param src Source string with escaped newlines
 * @param dst Destination buffer for processed string
 * @param dstSize Size of destination buffer in characters
 */
static void ConvertEscapedNewlines(const wchar_t* src, wchar_t* dst, size_t dstSize) {
    size_t j = 0;
    while (*src && j < dstSize - 1) {
        if (src[0] == L'\\' && src[1] == L'n') {
            dst[j++] = L'\n';
            src += 2;
        } else {
            dst[j++] = *src++;
        }
    }
    dst[j] = L'\0';
}

/**
 * @brief Check if control requires newline conversion
 * @param dialogID Dialog resource ID
 * @param controlID Control resource ID
 * @return TRUE if newline conversion needed
 */
static BOOL NeedsNewlineConversion(int dialogID, int controlID) {
    return (dialogID == CLOCK_IDD_POMODORO_COMBO_DIALOG ||
            dialogID == CLOCK_IDD_POMODORO_TIME_DIALOG ||
            dialogID == CLOCK_IDD_WEBSITE_DIALOG ||
            dialogID == CLOCK_IDD_SHORTCUT_DIALOG ||
            dialogID == CLOCK_IDD_DIALOG1) &&
           controlID == CLOCK_IDC_STATIC;
}

/**
 * @brief Process and set version text with formatting
 * @param hwndCtl Control window handle
 * @param localizedText Localized format string
 * @return TRUE if processed successfully
 */
static BOOL ProcessVersionText(HWND hwndCtl, const wchar_t* localizedText) {
    wchar_t versionText[VERSION_TEXT_MAX];
    const wchar_t* format = GetLocalizedString(NULL, L"Version: %hs");
    StringCbPrintfW(versionText, sizeof(versionText), 
                    format ? format : localizedText, CATIME_VERSION);
    SetWindowTextW(hwndCtl, versionText);
    return TRUE;
}

/**
 * @brief Apply special processing to control text (newline conversion, version formatting)
 * @param hwndCtl Handle to control window
 * @param localizedText Localized text to process
 * @param dialogID Dialog resource ID
 * @param controlID Control resource ID
 * @return TRUE if special processing was applied, FALSE otherwise
 */
static BOOL ProcessSpecialControlText(HWND hwndCtl, const wchar_t* localizedText, 
                                     int dialogID, int controlID) {
    if (NeedsNewlineConversion(dialogID, controlID)) {
        wchar_t processedText[LARGE_TEXT_MAX];
        ConvertEscapedNewlines(localizedText, processedText, LARGE_TEXT_MAX);
        SetWindowTextW(hwndCtl, processedText);
        return TRUE;
    }
    
    if (controlID == IDC_VERSION_TEXT && dialogID == IDD_ABOUT_DIALOG) {
        return ProcessVersionText(hwndCtl, localizedText);
    }
    
    return FALSE;
}

/* ============================================================================
 * Lookup functions
 * ============================================================================ */

/**
 * @brief Check if control class name is localizable
 * @param className Windows control class name
 * @return TRUE if control type supports localization
 */
static BOOL IsLocalizableControlType(const wchar_t* className) {
    static const wchar_t* localizableTypes[] = {
        L"Button", L"Static", L"ComboBox", L"Edit"
    };
    
    for (size_t i = 0; i < ARRAY_SIZE(localizableTypes); i++) {
        if (wcscmp(className, localizableTypes[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief Find localized text for special control with fallback support
 * @param dialogID Dialog resource ID
 * @param controlID Control resource ID
 * @return Localized text or fallback text, NULL if not found
 */
static const wchar_t* FindSpecialControlText(int dialogID, int controlID) {
    for (size_t i = 0; i < ARRAY_SIZE(g_specialControls); i++) {
        if (g_specialControls[i].dialogID == dialogID &&
            g_specialControls[i].controlID == controlID) {
            const wchar_t* localizedText = GetLocalizedString(NULL, g_specialControls[i].textKey);
            return localizedText ? localizedText : g_specialControls[i].fallbackText;
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
    for (size_t i = 0; i < ARRAY_SIZE(g_specialButtons); i++) {
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
    for (size_t i = 0; i < ARRAY_SIZE(g_dialogTitles); i++) {
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
    wchar_t className[CLASS_NAME_MAX];
    GetClassNameW(hwndCtl, className, CLASS_NAME_MAX);
    
    if (IsLocalizableControlType(className)) {
        return GetWindowTextW(hwndCtl, buffer, bufferSize) > 0;
    }
    
    return FALSE;
}

/* ============================================================================
 * Control localization
 * ============================================================================ */

/**
 * @brief Localize a single control with appropriate text
 * @param hwndCtl Control window handle
 * @param dialogID Dialog resource ID
 * @param controlID Control resource ID
 * @return TRUE to continue enumeration
 */
static BOOL LocalizeControl(HWND hwndCtl, int dialogID, int controlID) {
    // Check for special controls requiring custom handling
    const wchar_t* specialText = FindSpecialControlText(dialogID, controlID);
    if (specialText) {
        if (ProcessSpecialControlText(hwndCtl, specialText, dialogID, controlID)) {
            return TRUE;
        }
        SetWindowTextW(hwndCtl, specialText);
        return TRUE;
    }
    
    // Check for special button text
    const wchar_t* buttonText = FindSpecialButtonText(dialogID, controlID);
    if (buttonText) {
        SetWindowTextW(hwndCtl, buttonText);
        return TRUE;
    }
    
    // Apply generic localization for standard controls
    wchar_t originalText[CONTROL_TEXT_MAX] = {0};
    if (GetControlOriginalText(hwndCtl, originalText, CONTROL_TEXT_MAX) && originalText[0] != L'\0') {
        const wchar_t* localizedText = GetLocalizedString(NULL, originalText);
        if (localizedText && wcscmp(localizedText, originalText) != 0) {
            SetWindowTextW(hwndCtl, localizedText);
        }
    }
    
    return TRUE;
}

/**
 * @brief EnumChildWindows callback to localize individual dialog controls
 * @param hwndCtl Handle to child control window
 * @param lParam Pointer to EnumChildWindowsData structure
 * @return TRUE to continue enumeration, FALSE to stop
 */
static BOOL CALLBACK EnumChildProc(HWND hwndCtl, LPARAM lParam) {
    EnumChildWindowsData* data = (EnumChildWindowsData*)lParam;
    int controlID = GetDlgCtrlID(hwndCtl);
    
    if (controlID == 0) {
        return TRUE;
    }
    
    return LocalizeControl(hwndCtl, data->dialogID, controlID);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

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
    
    // Set localized dialog title
    const wchar_t* titleText = GetDialogTitleText(dialogID);
    if (titleText) {
        SetWindowTextW(hwndDlg, titleText);
    }
    
    // Enumerate and localize all child controls
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
    // Handle dialog title request
    if (controlID == DIALOG_TITLE_ID) {
        return GetDialogTitleText(dialogID);
    }
    
    // Check for special control text
    const wchar_t* specialText = FindSpecialControlText(dialogID, controlID);
    if (specialText) {
        return specialText;
    }
    
    // Check for special button text
    const wchar_t* buttonText = FindSpecialButtonText(dialogID, controlID);
    if (buttonText) {
        return buttonText;
    }
    
    return NULL;
}
