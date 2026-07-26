/**
 * @file window_message_dialogs.c
 * @brief Handles modeless dialog and update result messages.
 */

#include "window_procedure/window_message_handlers_internal.h"
#include "config.h"
#include "menu_preview.h"
#include "update/update_internal.h"
#include "window_procedure/window_procedure.h"

LRESULT HandleDialogCountdown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    int seconds = (int)wp;
    if (seconds > 0) {
        CleanupBeforeTimerAction(hwnd);
        StartCountdownWithTime(hwnd, seconds);
    }
    return 0;
}

LRESULT HandleDialogShortcut(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    /* Shortcut time options already saved by dialog */
    return 0;
}

LRESULT HandleDialogColor(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == 0) {
        /* Color cancelled - restore preview if needed */
        CancelPreview(hwnd);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

LRESULT HandleDialogUpdate(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == IDYES) {
        /* User chose to update - trigger download and exit */
        TriggerUpdateDownload(hwnd);
    }
    return 0;
}

LRESULT HandleUpdateCheckResult(HWND hwnd, WPARAM wp, LPARAM lp) {
    if (wp == 1) {
        if (lp == 0) {
            ShowStoredUpdateDialog(hwnd);
        }
    } else {
        if (lp == 0) {
            ShowStoredNoUpdateDialog(hwnd);
        }
    }
    return 0;
}

LRESULT HandleDialogFontLicense(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == IDOK) {
        SetFontLicenseAccepted(TRUE);
        SetFontLicenseVersionAccepted(GetCurrentFontLicenseVersion());
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}
