/**
 * @file dialog_notification_preview.c
 * @brief Owns notification preview lifetime and synchronizes preview controls.
 */

#include "dialog/dialog_notification_settings_internal.h"
#include "config.h"
#include "config/config_defaults.h"
#include "notification.h"
#include "../resource/resource.h"

#include <stdio.h>
#include <wchar.h>

static HWND g_previewNotification = NULL;

static HWND FindPreviewNotificationWindow(void) {
    HWND hwnd = NULL;

    while ((hwnd = FindWindowExW(NULL, hwnd, NOTIFICATION_CLASS_NAME, L"Catime Notification")) != NULL) {
        if (DialogNotificationInternal_IsCurrentProcessWindow(hwnd) && IsToastNotificationPreviewWindow(hwnd)) {
            return hwnd;
        }
    }

    return NULL;
}

static HWND ResolvePreviewNotificationWindow(void) {
    if (!g_previewNotification ||
        !IsWindow(g_previewNotification) ||
        !IsToastNotificationPreviewWindow(g_previewNotification)) {
        g_previewNotification = FindPreviewNotificationWindow();
    }

    if (g_previewNotification &&
        IsWindow(g_previewNotification) &&
        IsToastNotificationPreviewWindow(g_previewNotification)) {
        return g_previewNotification;
    }
    return NULL;
}

static int GetTrackbarPosition(HWND hwndDlg, int controlId, int fallback) {
    HWND hwndTrackbar = GetDlgItem(hwndDlg, controlId);
    if (!hwndTrackbar) {
        return fallback;
    }

    return (int)SendMessage(hwndTrackbar, TBM_GETPOS, 0, 0);
}

int DialogNotificationInternal_ClampOpacity(int opacity) {
    if (opacity < MIN_VISIBLE_OPACITY) {
        return MIN_VISIBLE_OPACITY;
    }
    if (opacity > MAX_OPACITY) {
        return MAX_OPACITY;
    }
    return opacity;
}

static void EnsurePreviewNotification(HWND hwndDlg, const wchar_t* message) {
    HWND hwndParent = DialogNotificationInternal_GetParent(hwndDlg);
    if (!DialogNotificationInternal_IsValidParent(hwndParent)) {
        return;
    }

    /* Reuse the existing preview window when it is still open. */
    if (g_previewNotification &&
        IsWindow(g_previewNotification) &&
        IsToastNotificationPreviewWindow(g_previewNotification)) {
        return;
    }

    /* Also check by finding window in case handle became stale */
    HWND existingPreview = FindPreviewNotificationWindow();
    if (existingPreview && IsWindow(existingPreview)) {
        g_previewNotification = existingPreview;
        return;
    }

    wchar_t previewMessage[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE] = {0};
    if (message && message[0] != L'\0') {
        wcsncpy(previewMessage, message, sizeof(previewMessage)/sizeof(wchar_t) - 1);
        previewMessage[sizeof(previewMessage)/sizeof(wchar_t) - 1] = L'\0';
    } else {
        MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.timeout_message, -1,
                           previewMessage, sizeof(previewMessage)/sizeof(wchar_t));
    }

    int initialOpacity = DialogNotificationInternal_ClampOpacity(
        GetTrackbarPosition(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT,
                            g_AppConfig.notification.display.max_opacity));
    ShowToastNotificationPreview(hwndParent, previewMessage, initialOpacity);

    g_previewNotification = FindPreviewNotificationWindow();
}

static void UpdatePreviewAppearanceFromControls(HWND hwndDlg) {
    HWND hwndPreview = ResolvePreviewNotificationWindow();
    if (!hwndPreview) return;
    int opacity = DialogNotificationInternal_ClampOpacity(
        GetTrackbarPosition(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT,
                            g_AppConfig.notification.display.max_opacity));
    int cornerRadius = GetTrackbarPosition(
        hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER,
        g_AppConfig.notification.display.corner_radius);
    int fontPercent = GetTrackbarPosition(
        hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER,
        g_AppConfig.notification.display.font_size);
    SetToastNotificationAppearance(hwndPreview, opacity, cornerRadius,
                                   fontPercent);
}

void DialogNotificationInternal_RefreshPreview(HWND hwndDlg) {
    wchar_t currentMessage[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE];
    GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, currentMessage,
                    sizeof(currentMessage) / sizeof(wchar_t));

    EnsurePreviewNotification(hwndDlg, currentMessage[0] != L'\0' ? currentMessage : NULL);
    UpdatePreviewAppearanceFromControls(hwndDlg);
}

void DialogNotificationInternal_UpdatePreviewText(HWND hwndDlg, const wchar_t* newText) {
    if (!g_previewNotification ||
        !IsWindow(g_previewNotification) ||
        !IsToastNotificationPreviewWindow(g_previewNotification)) {
        EnsurePreviewNotification(hwndDlg, newText);
        UpdatePreviewAppearanceFromControls(hwndDlg);
        return;
    }

    SetToastNotificationMessage(g_previewNotification, newText);
}

void DialogNotificationInternal_ClosePreview(void) {
    HWND preview = g_previewNotification;
    g_previewNotification = NULL;
    if (preview && IsWindow(preview) &&
        IsToastNotificationPreviewWindow(preview)) {
        DestroyWindow(preview);
    }
}

BOOL DialogNotificationInternal_SavePreviewPlacement(void) {
    if (!g_previewNotification || !IsWindow(g_previewNotification) ||
        !IsToastNotificationPreviewWindow(g_previewNotification)) {
        return TRUE;
    }

    RECT rect = {0};
    if (!GetWindowRect(g_previewNotification, &rect)) {
        return TRUE;
    }
    return WriteConfigNotificationWindow(
        rect.left, rect.top, rect.right - rect.left,
        rect.bottom - rect.top);
}

void UpdateNotificationOpacityControls(int opacity) {
    if (!DialogNotificationInternal_IsCurrentDialog(DialogNotificationInternal_GetDialog())) {
        return;
    }

    opacity = DialogNotificationInternal_ClampOpacity(opacity);

    HWND hwndOpacitySlider = GetDlgItem(DialogNotificationInternal_GetDialog(), IDC_NOTIFICATION_OPACITY_EDIT);
    if (hwndOpacitySlider) {
        SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, opacity);

        wchar_t opacityText[16];
        _snwprintf_s(opacityText, 16, _TRUNCATE, L"%d%%", opacity);
        SetDlgItemTextW(DialogNotificationInternal_GetDialog(), IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
    }
}

void UpdateNotificationFontPercentControls(int fontPercent) {
    if (!DialogNotificationInternal_IsCurrentDialog(DialogNotificationInternal_GetDialog())) {
        return;
    }

    HWND hwndFontSizeSlider = GetDlgItem(DialogNotificationInternal_GetDialog(), IDC_NOTIFICATION_FONT_SIZE_SLIDER);
    if (hwndFontSizeSlider) {
        SendMessage(hwndFontSizeSlider, TBM_SETPOS, TRUE, fontPercent);

        wchar_t fontSizeText[16];
        _snwprintf_s(fontSizeText, 16, _TRUNCATE, L"%d%%", fontPercent);
        SetDlgItemTextW(DialogNotificationInternal_GetDialog(), IDC_NOTIFICATION_FONT_SIZE_TEXT, fontSizeText);
    }
}
