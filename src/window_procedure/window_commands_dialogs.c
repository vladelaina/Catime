#include "window_commands_internal.h"

LRESULT CmdOpenWebsite(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; ShowWebsiteDialog(hwnd); return 0;
}

LRESULT CmdNotificationContent(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; ShowNotificationMessagesDialog(hwnd); return 0;
}

LRESULT CmdNotificationDisplay(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; ShowNotificationDisplayDialog(hwnd); return 0;
}

LRESULT CmdNotificationSettings(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; ShowNotificationSettingsDialog(hwnd); return 0;
}

LRESULT CmdCheckUpdate(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (IsRunningPackagedApp()) {
        HINSTANCE result = ShellExecuteW(
            hwnd, L"open", URL_MICROSOFT_STORE,
            NULL, NULL, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32) {
            LOG_WARNING("Failed to open Microsoft Store update page");
        } else {
            LOG_INFO("Opened Microsoft Store update page");
        }
        return 0;
    }
    if (!CheckForUpdateAsync(hwnd, FALSE)) {
        LOG_WARNING("Manual update check was not started");
    }
    return 0;
}

LRESULT CmdHotkeySettings(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; ShowHotkeySettingsDialog(hwnd); return 0;
}

LRESULT CmdHelp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    extern void OpenUserGuide(void);
    OpenUserGuide();
    return 0;
}

LRESULT CmdSupport(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    extern void OpenSupportPage(void);
    OpenSupportPage();
    return 0;
}

LRESULT CmdVlaina(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    extern void OpenVlainaPage(void);
    OpenVlainaPage();
    return 0;
}

LRESULT CmdFeedback(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    extern void OpenFeedbackPage(void);
    OpenFeedbackPage();
    return 0;
}

LRESULT CmdBrowseFile(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    char path[MAX_PATH];
    if (ShowFilePicker(hwnd, path, sizeof(path))) {
        ValidateAndSetTimeoutFile(hwnd, path);
    }
    return 0;
}
