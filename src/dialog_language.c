/**
 * @file dialog_language.c
 * @brief Dialog localization with three-tier lookup
 * 
 * 1. Special controls (custom handling)
 * 2. Standard controls (generic text)
 * 3. Fallback text (missing translations)
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include "../include/dialog_language.h"
#include "../include/language.h"
#include "../resource/resource.h"

#define CLASS_NAME_MAX 256
#define CONTROL_TEXT_MAX 512
#define LARGE_TEXT_MAX 1024
#define VERSION_TEXT_MAX 256

#define DIALOG_TITLE_ID -1

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct {
    int dialogID;
    wchar_t* titleKey;
} DialogTitleEntry;

typedef struct {
    int dialogID;
    int controlID;
    wchar_t* textKey;
    wchar_t* fallbackText;
} SpecialControlEntry;

typedef struct {
    HWND hwndDlg;
    int dialogID;
} EnumChildWindowsData;

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

/** Convert \\n to \n */
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

static BOOL NeedsNewlineConversion(int dialogID, int controlID) {
    return (dialogID == CLOCK_IDD_POMODORO_COMBO_DIALOG ||
            dialogID == CLOCK_IDD_POMODORO_TIME_DIALOG ||
            dialogID == CLOCK_IDD_WEBSITE_DIALOG ||
            dialogID == CLOCK_IDD_SHORTCUT_DIALOG ||
            dialogID == CLOCK_IDD_DIALOG1) &&
           controlID == CLOCK_IDC_STATIC;
}

static BOOL ProcessVersionText(HWND hwndCtl, const wchar_t* localizedText) {
    wchar_t versionText[VERSION_TEXT_MAX];
    const wchar_t* format = GetLocalizedString(NULL, L"Version: %hs");
    StringCbPrintfW(versionText, sizeof(versionText), 
                    format ? format : localizedText, CATIME_VERSION);
    SetWindowTextW(hwndCtl, versionText);
    return TRUE;
}

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

static const wchar_t* FindSpecialButtonText(int dialogID, int controlID) {
    for (size_t i = 0; i < ARRAY_SIZE(g_specialButtons); i++) {
        if (g_specialButtons[i].dialogID == dialogID &&
            g_specialButtons[i].controlID == controlID) {
            return GetLocalizedString(NULL, g_specialButtons[i].textKey);
        }
    }
    return NULL;
}

static const wchar_t* GetDialogTitleText(int dialogID) {
    for (size_t i = 0; i < ARRAY_SIZE(g_dialogTitles); i++) {
        if (g_dialogTitles[i].dialogID == dialogID) {
            return GetLocalizedString(NULL, g_dialogTitles[i].titleKey);
        }
    }
    return NULL;
}

static BOOL GetControlOriginalText(HWND hwndCtl, wchar_t* buffer, int bufferSize) {
    wchar_t className[CLASS_NAME_MAX];
    GetClassNameW(hwndCtl, className, CLASS_NAME_MAX);
    
    if (IsLocalizableControlType(className)) {
        return GetWindowTextW(hwndCtl, buffer, bufferSize) > 0;
    }
    
    return FALSE;
}

static BOOL LocalizeControl(HWND hwndCtl, int dialogID, int controlID) {
    const wchar_t* specialText = FindSpecialControlText(dialogID, controlID);
    if (specialText) {
        if (ProcessSpecialControlText(hwndCtl, specialText, dialogID, controlID)) {
            return TRUE;
        }
        SetWindowTextW(hwndCtl, specialText);
        return TRUE;
    }
    
    const wchar_t* buttonText = FindSpecialButtonText(dialogID, controlID);
    if (buttonText) {
        SetWindowTextW(hwndCtl, buttonText);
        return TRUE;
    }
    
    wchar_t originalText[CONTROL_TEXT_MAX] = {0};
    if (GetControlOriginalText(hwndCtl, originalText, CONTROL_TEXT_MAX) && originalText[0] != L'\0') {
        const wchar_t* localizedText = GetLocalizedString(NULL, originalText);
        if (localizedText && wcscmp(localizedText, originalText) != 0) {
            SetWindowTextW(hwndCtl, localizedText);
        }
    }
    
    return TRUE;
}

static BOOL CALLBACK EnumChildProc(HWND hwndCtl, LPARAM lParam) {
    EnumChildWindowsData* data = (EnumChildWindowsData*)lParam;
    int controlID = GetDlgCtrlID(hwndCtl);
    
    if (controlID == 0) {
        return TRUE;
    }
    
    return LocalizeControl(hwndCtl, data->dialogID, controlID);
}

BOOL InitDialogLanguageSupport(void) {
    return TRUE;
}

BOOL ApplyDialogLanguage(HWND hwndDlg, int dialogID) {
    if (!hwndDlg) return FALSE;
    
    const wchar_t* titleText = GetDialogTitleText(dialogID);
    if (titleText) {
        SetWindowTextW(hwndDlg, titleText);
    }
    
    EnumChildWindowsData data = {
        .hwndDlg = hwndDlg,
        .dialogID = dialogID
    };
    
    EnumChildWindows(hwndDlg, EnumChildProc, (LPARAM)&data);
    
    return TRUE;
}

const wchar_t* GetDialogLocalizedString(int dialogID, int controlID) {
    if (controlID == DIALOG_TITLE_ID) {
        return GetDialogTitleText(dialogID);
    }
    
    const wchar_t* specialText = FindSpecialControlText(dialogID, controlID);
    if (specialText) {
        return specialText;
    }
    
    const wchar_t* buttonText = FindSpecialButtonText(dialogID, controlID);
    if (buttonText) {
        return buttonText;
    }
    
    return NULL;
}
