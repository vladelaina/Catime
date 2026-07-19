/**
 * @file dialog_notification_settings.c
 * @brief Comprehensive notification settings dialog (all-in-one)
 */

#include "dialog/dialog_notification.h"
#include "dialog/dialog_notification_audio.h"
#include "dialog/dialog_procedure.h"
#include "dialog/dialog_language.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "config.h"
#include "config/config_defaults.h"
#include "audio_player.h"
#include "utils/string_convert.h"
#include "notification.h"
#include "log.h"
#include "../resource/resource.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

/* ============================================================================
 * Global State
 * ============================================================================ */

static HWND g_hwndNotificationSettingsDialog = NULL;
static HWND g_hwndPreviewNotification = NULL;

typedef struct {
    BOOL isPlaying;
    BOOL isInitializing;
    int originalVolume;
} NotificationSettingsState;

static NotificationSettingsState* GetNotificationSettingsState(HWND hwndDlg) {
    return hwndDlg ? (NotificationSettingsState*)GetWindowLongPtrW(
                         hwndDlg, GWLP_USERDATA)
                   : NULL;
}

static BOOL IsCurrentProcessWindow(HWND hwnd) {
    DWORD processId = 0;
    if (!hwnd) return FALSE;
    GetWindowThreadProcessId(hwnd, &processId);
    return processId == GetCurrentProcessId();
}

static BOOL IsWindowOfClass(HWND hwnd, const wchar_t* className) {
    if (!hwnd || !IsWindow(hwnd) || !className) {
        return FALSE;
    }

    wchar_t actualClass[64] = {0};
    if (GetClassNameW(hwnd, actualClass, _countof(actualClass)) == 0) {
        return FALSE;
    }

    return wcscmp(actualClass, className) == 0;
}

static BOOL IsValidNotificationSettingsParent(HWND hwnd) {
    return IsCurrentProcessWindow(hwnd) &&
           IsWindowOfClass(hwnd, CATIME_MAIN_WINDOW_CLASS_NAME);
}

static HWND GetNotificationSettingsParent(HWND hwndDlg) {
    HWND hwndParent = hwndDlg ? GetParent(hwndDlg) : NULL;
    return IsValidNotificationSettingsParent(hwndParent) ? hwndParent : NULL;
}

static BOOL IsCurrentNotificationSettingsDialog(HWND hwnd) {
    return hwnd &&
           hwnd == g_hwndNotificationSettingsDialog &&
           IsWindow(hwnd) &&
           Dialog_IsOpen(DIALOG_INSTANCE_NOTIFICATION_FULL);
}

static int MeasureNotificationControlText96(HWND hwndDlg, int controlId,
                                            HFONT font, UINT dpi,
                                            int padding96) {
    HWND control = GetDlgItem(hwndDlg, controlId);
    wchar_t text[512] = {0};
    SIZE size = {0};
    if (!control || !GetWindowTextW(control, text, _countof(text)) ||
        !DialogModern_MeasureText96(hwndDlg, font, text, dpi, &size)) {
        return padding96;
    }
    return size.cx + padding96;
}

static int NotificationMaxWidth(int current, int desired) {
    return desired > current ? desired : current;
}

static void LayoutNotificationSettingsDialog(HWND hwndDlg) {
    UINT dpi = DialogModern_GetDpi(hwndDlg);
    HFONT measureFont = DialogModern_CreateFont(dpi, 12, FW_NORMAL);
    RECT contentGroup = {0};
    RECT displayGroup = {0};
    RECT audioGroup = {0};
    RECT methodGroup = {0};
    RECT labelRect = {0};
    if (!measureFont ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_CONTENT_GROUP,
                                     dpi, &contentGroup) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_DISPLAY_GROUP,
                                     dpi, &displayGroup) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_AUDIO_GROUP,
                                     dpi, &audioGroup) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_METHOD_GROUP,
                                     dpi, &methodGroup) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_LABEL1,
                                     dpi, &labelRect)) {
        if (measureFont) DeleteObject(measureFont);
        return;
    }

    RECT timeLabel = {0};
    RECT radiusLabel = {0};
    RECT opacityLabel = {0};
    RECT fontLabel = {0};
    RECT timeEdit = {0};
    RECT check = {0};
    RECT radiusSlider = {0};
    RECT opacitySlider = {0};
    RECT fontSlider = {0};
    RECT radiusText = {0};
    RECT opacityText = {0};
    RECT fontText = {0};
    if (!DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_TIME_LABEL,
                                     dpi, &timeLabel) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_RADIUS_LABEL,
                                     dpi, &radiusLabel) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_OPACITY_LABEL,
                                     dpi, &opacityLabel) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_LABEL,
                                     dpi, &fontLabel) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_TIME_EDIT,
                                     dpi, &timeEdit) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK,
                                     dpi, &check) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER,
                                     dpi, &radiusSlider) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT,
                                     dpi, &opacitySlider) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER,
                                     dpi, &fontSlider) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_RADIUS_TEXT,
                                     dpi, &radiusText) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT,
                                     dpi, &opacityText) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_TEXT,
                                     dpi, &fontText)) {
        DeleteObject(measureFont);
        return;
    }

    int displayLabelWidth = timeLabel.right - timeLabel.left;
    displayLabelWidth = NotificationMaxWidth(
        displayLabelWidth,
        MeasureNotificationControlText96(hwndDlg, IDC_NOTIFICATION_TIME_LABEL,
                                          measureFont, dpi, 12));
    displayLabelWidth = NotificationMaxWidth(
        displayLabelWidth,
        MeasureNotificationControlText96(hwndDlg, IDC_NOTIFICATION_RADIUS_LABEL,
                                          measureFont, dpi, 12));
    displayLabelWidth = NotificationMaxWidth(
        displayLabelWidth,
        MeasureNotificationControlText96(hwndDlg, IDC_NOTIFICATION_OPACITY_LABEL,
                                          measureFont, dpi, 12));
    displayLabelWidth = NotificationMaxWidth(
        displayLabelWidth,
        MeasureNotificationControlText96(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_LABEL,
                                          measureFont, dpi, 12));
    if (displayLabelWidth > 360) displayLabelWidth = 360;

    int sliderX = timeLabel.left + displayLabelWidth + 12;
    int sliderWidth = radiusSlider.right - radiusSlider.left;
    int valueX = sliderX + sliderWidth + 10;
    int timeWidth = timeEdit.right - timeEdit.left;
    int checkX = sliderX + timeWidth + 16;
    int checkWidth = NotificationMaxWidth(
        check.right - check.left,
        MeasureNotificationControlText96(hwndDlg,
                                          IDC_DISABLE_NOTIFICATION_CHECK,
                                          measureFont, dpi, 28));

    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_TIME_LABEL, dpi,
                                timeLabel.left, timeLabel.top,
                                displayLabelWidth,
                                timeLabel.bottom - timeLabel.top);
    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_RADIUS_LABEL, dpi,
                                radiusLabel.left, radiusLabel.top,
                                displayLabelWidth,
                                radiusLabel.bottom - radiusLabel.top);
    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_OPACITY_LABEL, dpi,
                                opacityLabel.left, opacityLabel.top,
                                displayLabelWidth,
                                opacityLabel.bottom - opacityLabel.top);
    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_LABEL, dpi,
                                fontLabel.left, fontLabel.top,
                                displayLabelWidth,
                                fontLabel.bottom - fontLabel.top);
    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, dpi,
                                sliderX, timeEdit.top, timeWidth,
                                timeEdit.bottom - timeEdit.top);
    DialogModern_SetChildRect96(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK, dpi,
                                checkX, check.top, checkWidth,
                                check.bottom - check.top);

    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER, dpi,
                                sliderX, radiusSlider.top, sliderWidth,
                                radiusSlider.bottom - radiusSlider.top);
    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, dpi,
                                sliderX, opacitySlider.top, sliderWidth,
                                opacitySlider.bottom - opacitySlider.top);
    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER, dpi,
                                sliderX, fontSlider.top, sliderWidth,
                                fontSlider.bottom - fontSlider.top);
    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_RADIUS_TEXT, dpi,
                                valueX, radiusText.top,
                                radiusText.right - radiusText.left,
                                radiusText.bottom - radiusText.top);
    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, dpi,
                                valueX, opacityText.top,
                                opacityText.right - opacityText.left,
                                opacityText.bottom - opacityText.top);
    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_TEXT, dpi,
                                valueX, fontText.top,
                                fontText.right - fontText.left,
                                fontText.bottom - fontText.top);

    RECT soundLabel = {0};
    RECT soundCombo = {0};
    RECT testButton = {0};
    RECT soundDirButton = {0};
    RECT volumeLabel = {0};
    RECT volumeSlider = {0};
    RECT volumeText = {0};
    if (DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_SOUND_LABEL,
                                    dpi, &soundLabel) &&
        DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO,
                                    dpi, &soundCombo) &&
        DialogModern_GetChildRect96(hwndDlg, IDC_TEST_SOUND_BUTTON,
                                    dpi, &testButton) &&
        DialogModern_GetChildRect96(hwndDlg, IDC_OPEN_SOUND_DIR_BUTTON,
                                    dpi, &soundDirButton) &&
        DialogModern_GetChildRect96(hwndDlg, IDC_VOLUME_LABEL,
                                    dpi, &volumeLabel) &&
        DialogModern_GetChildRect96(hwndDlg, IDC_VOLUME_SLIDER,
                                    dpi, &volumeSlider) &&
        DialogModern_GetChildRect96(hwndDlg, IDC_VOLUME_TEXT,
                                    dpi, &volumeText)) {
        int soundLabelWidth = NotificationMaxWidth(
            soundLabel.right - soundLabel.left,
            MeasureNotificationControlText96(hwndDlg, IDC_NOTIFICATION_SOUND_LABEL,
                                              measureFont, dpi, 12));
        int comboX = soundLabel.left + soundLabelWidth + 10;
        int comboWidth = NotificationMaxWidth(
            soundCombo.right - soundCombo.left, 150);
        int testWidth = NotificationMaxWidth(
            testButton.right - testButton.left,
            MeasureNotificationControlText96(hwndDlg, IDC_TEST_SOUND_BUTTON,
                                              measureFont, dpi, 24));
        int dirWidth = NotificationMaxWidth(
            soundDirButton.right - soundDirButton.left,
            MeasureNotificationControlText96(hwndDlg, IDC_OPEN_SOUND_DIR_BUTTON,
                                              measureFont, dpi, 24));
        int testX = comboX + comboWidth + 10;
        int dirX = testX + testWidth + 10;

        DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_SOUND_LABEL, dpi,
                                    soundLabel.left, soundLabel.top,
                                    soundLabelWidth,
                                    soundLabel.bottom - soundLabel.top);
        DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO, dpi,
                                    comboX, soundCombo.top, comboWidth,
                                    soundCombo.bottom - soundCombo.top);
        DialogModern_SetChildRect96(hwndDlg, IDC_TEST_SOUND_BUTTON, dpi,
                                    testX, testButton.top, testWidth,
                                    testButton.bottom - testButton.top);
        DialogModern_SetChildRect96(hwndDlg, IDC_OPEN_SOUND_DIR_BUTTON, dpi,
                                    dirX, soundDirButton.top, dirWidth,
                                    soundDirButton.bottom - soundDirButton.top);

        int volumeLabelWidth = NotificationMaxWidth(
            volumeLabel.right - volumeLabel.left,
            MeasureNotificationControlText96(hwndDlg, IDC_VOLUME_LABEL,
                                              measureFont, dpi, 12));
        int volumeSliderX = volumeLabel.left + volumeLabelWidth + 10;
        int volumeSliderWidth = volumeSlider.right - volumeSlider.left;
        int volumeTextX = volumeSliderX + volumeSliderWidth + 10;
        DialogModern_SetChildRect96(hwndDlg, IDC_VOLUME_LABEL, dpi,
                                    volumeLabel.left, volumeLabel.top,
                                    volumeLabelWidth,
                                    volumeLabel.bottom - volumeLabel.top);
        DialogModern_SetChildRect96(hwndDlg, IDC_VOLUME_SLIDER, dpi,
                                    volumeSliderX, volumeSlider.top,
                                    volumeSliderWidth,
                                    volumeSlider.bottom - volumeSlider.top);
        DialogModern_SetChildRect96(hwndDlg, IDC_VOLUME_TEXT, dpi,
                                    volumeTextX, volumeText.top,
                                    volumeText.right - volumeText.left,
                                    volumeText.bottom - volumeText.top);
    }

    int methodX = methodGroup.left + 10;
    int methodRight = methodX;
    const int radioIds[] = {
        IDC_NOTIFICATION_TYPE_CATIME,
        IDC_NOTIFICATION_TYPE_OS,
        IDC_NOTIFICATION_TYPE_SYSTEM_MODAL
    };
    for (size_t i = 0; i < _countof(radioIds); i++) {
        RECT radio = {0};
        if (!DialogModern_GetChildRect96(hwndDlg, radioIds[i], dpi, &radio)) {
            continue;
        }
        int width = NotificationMaxWidth(
            radio.right - radio.left,
            MeasureNotificationControlText96(hwndDlg, radioIds[i],
                                              measureFont, dpi, 28));
        DialogModern_SetChildRect96(hwndDlg, radioIds[i], dpi,
                                    methodX, radio.top, width,
                                    radio.bottom - radio.top);
        methodX += width + 16;
        methodRight = methodX - 16;
    }

    int contentLabelWidth = NotificationMaxWidth(
        labelRect.right - labelRect.left,
        MeasureNotificationControlText96(hwndDlg, IDC_NOTIFICATION_LABEL1,
                                          measureFont, dpi, 12));
    DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_LABEL1, dpi,
                                labelRect.left, labelRect.top,
                                contentLabelWidth,
                                labelRect.bottom - labelRect.top);

    int contentRight = labelRect.left + contentLabelWidth + 10;
    RECT contentEdit = {0};
    if (DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_EDIT1,
                                    dpi, &contentEdit)) {
        contentRight = NotificationMaxWidth(contentRight, contentEdit.right + 10);
    }

    int audioRight = audioGroup.right;
    if (DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_SOUND_LABEL,
                                    dpi, &soundLabel) &&
        DialogModern_GetChildRect96(hwndDlg, IDC_OPEN_SOUND_DIR_BUTTON,
                                    dpi, &soundDirButton) &&
        DialogModern_GetChildRect96(hwndDlg, IDC_VOLUME_TEXT,
                                    dpi, &volumeText)) {
        int soundLabelWidth = NotificationMaxWidth(
            soundLabel.right - soundLabel.left,
            MeasureNotificationControlText96(hwndDlg,
                                              IDC_NOTIFICATION_SOUND_LABEL,
                                              measureFont, dpi, 12));
        int comboWidth = NotificationMaxWidth(
            soundCombo.right - soundCombo.left, 150);
        int testWidth = NotificationMaxWidth(
            testButton.right - testButton.left,
            MeasureNotificationControlText96(hwndDlg, IDC_TEST_SOUND_BUTTON,
                                              measureFont, dpi, 24));
        int dirWidth = NotificationMaxWidth(
            soundDirButton.right - soundDirButton.left,
            MeasureNotificationControlText96(hwndDlg,
                                              IDC_OPEN_SOUND_DIR_BUTTON,
                                              measureFont, dpi, 24));
        int comboX = soundLabel.left + soundLabelWidth + 10;
        int testX = comboX + comboWidth + 10;
        int dirX = testX + testWidth + 10;
        audioRight = NotificationMaxWidth(audioRight, dirX + dirWidth + 10);

        int volumeLabelWidth = NotificationMaxWidth(
            volumeLabel.right - volumeLabel.left,
            MeasureNotificationControlText96(hwndDlg, IDC_VOLUME_LABEL,
                                              measureFont, dpi, 12));
        int volumeSliderX = volumeLabel.left + volumeLabelWidth + 10;
        int volumeTextX = volumeSliderX + (volumeSlider.right - volumeSlider.left) + 10;
        audioRight = NotificationMaxWidth(
            audioRight, volumeTextX + (volumeText.right - volumeText.left) + 10);
    }

    int right = contentRight;
    if (displayGroup.right > right) right = displayGroup.right;
    if (audioRight > right) right = audioRight;
    if (methodRight + 10 > right) right = methodRight + 10;
    if (checkX + checkWidth + 10 > right) right = checkX + checkWidth + 10;
    int valueRight = valueX + (radiusText.right - radiusText.left) + 10;
    if (valueRight > right) {
        right = valueRight;
    }

    if (DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_EDIT1,
                                    dpi, &contentEdit)) {
        int editWidth = right - contentEdit.left - 10;
        if (editWidth < contentEdit.right - contentEdit.left) {
            editWidth = contentEdit.right - contentEdit.left;
        }
        DialogModern_SetChildRect96(hwndDlg, IDC_NOTIFICATION_EDIT1, dpi,
                                    contentEdit.left, contentEdit.top,
                                    editWidth, contentEdit.bottom - contentEdit.top);
    }

    const int groupIds[] = {
        IDC_NOTIFICATION_CONTENT_GROUP,
        IDC_NOTIFICATION_DISPLAY_GROUP,
        IDC_NOTIFICATION_AUDIO_GROUP,
        IDC_NOTIFICATION_METHOD_GROUP
    };
    for (size_t i = 0; i < _countof(groupIds); i++) {
        RECT group = {0};
        if (DialogModern_GetChildRect96(hwndDlg, groupIds[i], dpi, &group)) {
            DialogModern_SetChildRect96(hwndDlg, groupIds[i], dpi,
                                        group.left, group.top,
                                        right - group.left,
                                        group.bottom - group.top);
        }
    }

    DeleteObject(measureFont);
}

/* ============================================================================
 * Preview Notification Helper
 * ============================================================================ */

static HWND FindPreviewNotificationWindow(void) {
    HWND hwnd = NULL;

    while ((hwnd = FindWindowExW(NULL, hwnd, NOTIFICATION_CLASS_NAME, L"Catime Notification")) != NULL) {
        if (IsCurrentProcessWindow(hwnd) && IsToastNotificationPreviewWindow(hwnd)) {
            return hwnd;
        }
    }

    return NULL;
}

static void UpdatePreviewOpacity(int opacity) {
    if (!g_hwndPreviewNotification ||
        !IsWindow(g_hwndPreviewNotification) ||
        !IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        g_hwndPreviewNotification = FindPreviewNotificationWindow();
    }

    if (g_hwndPreviewNotification &&
        IsWindow(g_hwndPreviewNotification) &&
        IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        SetToastNotificationOpacity(g_hwndPreviewNotification, opacity);
    }
}

static void UpdatePreviewCornerRadius(int cornerRadius) {
    if (!g_hwndPreviewNotification ||
        !IsWindow(g_hwndPreviewNotification) ||
        !IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        g_hwndPreviewNotification = FindPreviewNotificationWindow();
    }

    if (g_hwndPreviewNotification &&
        IsWindow(g_hwndPreviewNotification) &&
        IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        SetToastNotificationCornerRadius(g_hwndPreviewNotification, cornerRadius);
    }
}

static void UpdatePreviewFontPercent(int fontPercent) {
    if (!g_hwndPreviewNotification ||
        !IsWindow(g_hwndPreviewNotification) ||
        !IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        g_hwndPreviewNotification = FindPreviewNotificationWindow();
    }

    if (g_hwndPreviewNotification &&
        IsWindow(g_hwndPreviewNotification) &&
        IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        SetToastNotificationFontPercent(g_hwndPreviewNotification, fontPercent);
    }
}

static int GetTrackbarPosition(HWND hwndDlg, int controlId, int fallback) {
    HWND hwndTrackbar = GetDlgItem(hwndDlg, controlId);
    if (!hwndTrackbar) {
        return fallback;
    }

    return (int)SendMessage(hwndTrackbar, TBM_GETPOS, 0, 0);
}

static int ClampNotificationOpacityForDialog(int opacity) {
    if (opacity < MIN_VISIBLE_OPACITY) {
        return MIN_VISIBLE_OPACITY;
    }
    if (opacity > MAX_OPACITY) {
        return MAX_OPACITY;
    }
    return opacity;
}

static void EnsurePreviewNotification(HWND hwndDlg, const wchar_t* message) {
    HWND hwndParent = GetNotificationSettingsParent(hwndDlg);
    if (!IsValidNotificationSettingsParent(hwndParent)) {
        return;
    }

    /* Reuse the existing preview window when it is still open. */
    if (g_hwndPreviewNotification &&
        IsWindow(g_hwndPreviewNotification) &&
        IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        return;
    }

    /* Also check by finding window in case handle became stale */
    HWND existingPreview = FindPreviewNotificationWindow();
    if (existingPreview && IsWindow(existingPreview)) {
        g_hwndPreviewNotification = existingPreview;
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

    int initialOpacity = ClampNotificationOpacityForDialog(
        GetTrackbarPosition(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT,
                            g_AppConfig.notification.display.max_opacity));
    ShowToastNotificationPreview(hwndParent, previewMessage, initialOpacity);

    g_hwndPreviewNotification = FindPreviewNotificationWindow();
}

static void UpdatePreviewAppearanceFromControls(HWND hwndDlg) {
    UpdatePreviewOpacity(ClampNotificationOpacityForDialog(
        GetTrackbarPosition(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT,
                            g_AppConfig.notification.display.max_opacity)));
    UpdatePreviewCornerRadius(GetTrackbarPosition(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER,
                                                  g_AppConfig.notification.display.corner_radius));
    UpdatePreviewFontPercent(GetTrackbarPosition(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER,
                                                 g_AppConfig.notification.display.font_size));
}

static void RefreshPreviewFromControls(HWND hwndDlg) {
    wchar_t currentMessage[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE];
    GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, currentMessage,
                    sizeof(currentMessage) / sizeof(wchar_t));

    EnsurePreviewNotification(hwndDlg, currentMessage[0] != L'\0' ? currentMessage : NULL);
    UpdatePreviewAppearanceFromControls(hwndDlg);
}

static void UpdatePreviewNotificationText(HWND hwndDlg, const wchar_t* newText) {
    if (!g_hwndPreviewNotification ||
        !IsWindow(g_hwndPreviewNotification) ||
        !IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        EnsurePreviewNotification(hwndDlg, newText);
        UpdatePreviewAppearanceFromControls(hwndDlg);
        return;
    }

    SetToastNotificationMessage(g_hwndPreviewNotification, newText);
}

static void ClosePreviewNotification(void) {
    if (g_hwndPreviewNotification &&
        IsWindow(g_hwndPreviewNotification) &&
        IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        DestroyWindow(g_hwndPreviewNotification);
        g_hwndPreviewNotification = NULL;
    }
}

/**
 * @brief Update opacity slider and text in settings dialog
 * @param opacity New opacity value (10-100)
 *
 * @details Called from preview window when opacity changes via mouse wheel
 */
void UpdateNotificationOpacityControls(int opacity) {
    if (!IsCurrentNotificationSettingsDialog(g_hwndNotificationSettingsDialog)) {
        return;
    }

    opacity = ClampNotificationOpacityForDialog(opacity);

    HWND hwndOpacitySlider = GetDlgItem(g_hwndNotificationSettingsDialog, IDC_NOTIFICATION_OPACITY_EDIT);
    if (hwndOpacitySlider) {
        SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, opacity);

        wchar_t opacityText[16];
        _snwprintf_s(opacityText, 16, _TRUNCATE, L"%d%%", opacity);
        SetDlgItemTextW(g_hwndNotificationSettingsDialog, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
    }
}

/**
 * @brief Update font percentage slider and text in settings dialog
 * @param fontPercent Text height as a percentage of the notification window height
 *
 * @details Called from preview window when font percentage changes via Ctrl + mouse wheel
 */
void UpdateNotificationFontPercentControls(int fontPercent) {
    if (!IsCurrentNotificationSettingsDialog(g_hwndNotificationSettingsDialog)) {
        return;
    }

    HWND hwndFontSizeSlider = GetDlgItem(g_hwndNotificationSettingsDialog, IDC_NOTIFICATION_FONT_SIZE_SLIDER);
    if (hwndFontSizeSlider) {
        SendMessage(hwndFontSizeSlider, TBM_SETPOS, TRUE, fontPercent);

        wchar_t fontSizeText[16];
        _snwprintf_s(fontSizeText, 16, _TRUNCATE, L"%d%%", fontPercent);
        SetDlgItemTextW(g_hwndNotificationSettingsDialog, IDC_NOTIFICATION_FONT_SIZE_TEXT, fontSizeText);
    }
}

/* ============================================================================
 * Full Notification Settings Dialog
 * ============================================================================ */

void ShowNotificationSettingsDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_NOTIFICATION_FULL)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_NOTIFICATION_FULL);
        SetForegroundWindow(existing);
        return;
    }

    if (!IsValidNotificationSettingsParent(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateDialogW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG),
              hwndParent,
              NotificationSettingsDlgProc);

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

INT_PTR CALLBACK NotificationSettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    NotificationSettingsState* state = GetNotificationSettingsState(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            state = (NotificationSettingsState*)calloc(1, sizeof(*state));
            if (!state) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            state->isInitializing = TRUE;
            state->originalVolume = g_AppConfig.notification.sound.volume;
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)state);

            Dialog_InitializeInstance(DIALOG_INSTANCE_NOTIFICATION_FULL, hwndDlg);
            g_hwndNotificationSettingsDialog = hwndDlg;

            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG);

            wchar_t wideText[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE] = {0};

            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.timeout_message, -1,
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);

            SYSTEMTIME st = {0};
            GetLocalTime(&st);

            CheckDlgButton(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK,
                          g_AppConfig.notification.display.disabled ? BST_CHECKED : BST_UNCHECKED);

            EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT),
                        !g_AppConfig.notification.display.disabled);

            int totalSeconds = g_AppConfig.notification.display.timeout_ms / 1000;
            st.wHour = (WORD)(totalSeconds / 3600);
            st.wMinute = (WORD)((totalSeconds % 3600) / 60);
            st.wSecond = (WORD)(totalSeconds % 60);

            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_SETSYSTEMTIME,
                              GDT_VALID, (LPARAM)&st);

            HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            int notificationOpacity = ClampNotificationOpacityForDialog(
                g_AppConfig.notification.display.max_opacity);
            SendMessage(hwndOpacitySlider, TBM_SETRANGE, TRUE, MAKELONG(MIN_VISIBLE_OPACITY, 100));
            SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, notificationOpacity);

            wchar_t opacityText[16];
            _snwprintf_s(opacityText, 16, _TRUNCATE, L"%d%%", notificationOpacity);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);

            HWND hwndRadiusSlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER);
            SendMessage(hwndRadiusSlider, TBM_SETRANGE, TRUE,
                        MAKELONG(MIN_NOTIFICATION_CORNER_RADIUS, MAX_NOTIFICATION_CORNER_RADIUS));
            SendMessage(hwndRadiusSlider, TBM_SETPOS, TRUE, g_AppConfig.notification.display.corner_radius);

            wchar_t radiusText[16];
            _snwprintf_s(radiusText, 16, _TRUNCATE, L"%dpx", g_AppConfig.notification.display.corner_radius);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_RADIUS_TEXT, radiusText);

            int notificationFontPercent = g_AppConfig.notification.display.font_size;
            if (notificationFontPercent <= 0) {
                notificationFontPercent = DEFAULT_NOTIFICATION_FONT_SIZE;
            }
            if (notificationFontPercent < MIN_NOTIFICATION_FONT_SIZE) {
                notificationFontPercent = MIN_NOTIFICATION_FONT_SIZE;
            }
            if (notificationFontPercent > MAX_NOTIFICATION_FONT_SIZE) {
                notificationFontPercent = MAX_NOTIFICATION_FONT_SIZE;
            }

            HWND hwndFontSizeSlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER);
            SendMessage(hwndFontSizeSlider, TBM_SETRANGE, TRUE,
                        MAKELONG(MIN_NOTIFICATION_FONT_SIZE, MAX_NOTIFICATION_FONT_SIZE));
            SendMessage(hwndFontSizeSlider, TBM_SETPOS, TRUE, notificationFontPercent);

            wchar_t fontSizeText[16];
            _snwprintf_s(fontSizeText, 16, _TRUNCATE, L"%d%%", notificationFontPercent);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_TEXT, fontSizeText);

            switch (g_AppConfig.notification.display.type) {
                case NOTIFICATION_TYPE_CATIME:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME, BST_CHECKED);
                    break;
                case NOTIFICATION_TYPE_OS:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_OS, BST_CHECKED);
                    break;
                case NOTIFICATION_TYPE_SYSTEM_MODAL:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL, BST_CHECKED);
                    break;
            }

            HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
            PopulateNotificationSoundComboBox(hwndCombo, g_AppConfig.notification.sound.sound_file);

            HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
            SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
            SendMessage(hwndSlider, TBM_SETPOS, TRUE, g_AppConfig.notification.sound.volume);

            wchar_t volumeText[16];
            _snwprintf_s(volumeText, 16, _TRUNCATE, L"%d%%", g_AppConfig.notification.sound.volume);
            SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);

            state->isPlaying = FALSE;

            SetupAudioPlaybackCallback(hwndDlg);
            NotificationSoundCache_SetNotifyWindow(hwndDlg);

            g_hwndNotificationSettingsDialog = hwndDlg;

            LayoutNotificationSettingsDialog(hwndDlg);

            MoveDialogToPrimaryScreen(hwndDlg);

            state->isInitializing = FALSE;

            RefreshPreviewFromControls(hwndDlg);

            return TRUE;
        }

        case WM_HSCROLL: {
            if (GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER) == (HWND)lParam) {
                int volume = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

                wchar_t volumeText[16];
                _snwprintf_s(volumeText, 16, _TRUNCATE, L"%d%%", volume);
                SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);

                SetAudioVolume(volume);

                if (state && !state->isInitializing && !state->isPlaying) {
                    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                    char soundFile[MAX_PATH] = {0};
                    if (!GetSelectedNotificationSoundFile(hwndCombo, soundFile, sizeof(soundFile))) {
                        return TRUE;
                    }

                    if (soundFile[0] != '\0') {
                        if (PreviewNotificationSoundFile(hwndDlg, soundFile)) {
                            SetAudioVolume(volume);
                            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON,
                                            GetLocalizedString(NULL, L"Stop"));
                            state->isPlaying = TRUE;
                        }
                    }
                }

                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT) == (HWND)lParam) {
                int opacity = ClampNotificationOpacityForDialog(
                    (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0));

                wchar_t opacityText[16];
                _snwprintf_s(opacityText, 16, _TRUNCATE, L"%d%%", opacity);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);

                RefreshPreviewFromControls(hwndDlg);

                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER) == (HWND)lParam) {
                int cornerRadius = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

                wchar_t radiusText[16];
                _snwprintf_s(radiusText, 16, _TRUNCATE, L"%dpx", cornerRadius);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_RADIUS_TEXT, radiusText);

                RefreshPreviewFromControls(hwndDlg);

                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER) == (HWND)lParam) {
                int fontPercent = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

                wchar_t fontSizeText[16];
                _snwprintf_s(fontSizeText, 16, _TRUNCATE, L"%d%%", fontPercent);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_TEXT, fontSizeText);

                RefreshPreviewFromControls(hwndDlg);

                return TRUE;
            }
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_NOTIFICATION_EDIT1 && HIWORD(wParam) == EN_CHANGE) {
                if (state && !state->isInitializing) {
                    wchar_t newText[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE];
                    GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, newText, sizeof(newText)/sizeof(wchar_t));

                    if (newText[0] == L'\0') {
                        wcscpy_s(newText, NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE, L" ");
                    }
                    UpdatePreviewNotificationText(hwndDlg, newText);
                }
                return TRUE;
            }
            else if (LOWORD(wParam) == IDC_DISABLE_NOTIFICATION_CHECK && HIWORD(wParam) == BN_CLICKED) {
                BOOL isChecked = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !isChecked);
                return TRUE;
            }
            else if (LOWORD(wParam) == IDOK) {
                if (g_hwndPreviewNotification &&
                    IsWindow(g_hwndPreviewNotification) &&
                    IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
                    RECT rect;
                    if (GetWindowRect(g_hwndPreviewNotification, &rect)) {
                        if (!WriteConfigNotificationWindow(rect.left, rect.top,
                                                           rect.right - rect.left,
                                                           rect.bottom - rect.top)) {
                            LOG_WARNING("Failed to save notification preview window placement");
                            Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_EDIT1);
                            return TRUE;
                        }
                    }
                }

                wchar_t wTimeout[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE] = {0};

                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));

                char timeout_msg[NOTIFICATION_MESSAGE_BUFFER_SIZE] = {0};

                if (!WideToUtf8(wTimeout, timeout_msg, sizeof(timeout_msg))) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_EDIT1);
                    return TRUE;
                }

                SYSTEMTIME st = {0};

                BOOL isDisabled = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                int timeoutMs = g_AppConfig.notification.display.timeout_ms;
                if (SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID) {
                    int totalSeconds = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;

                    if (totalSeconds == 0) {
                        timeoutMs = 0;
                    } else if (!isDisabled) {
                        timeoutMs = totalSeconds * 1000;
                    }
                }

                HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
                int opacity = ClampNotificationOpacityForDialog(
                    (int)SendMessage(hwndOpacitySlider, TBM_GETPOS, 0, 0));

                HWND hwndRadiusSlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER);
                int cornerRadius = (int)SendMessage(hwndRadiusSlider, TBM_GETPOS, 0, 0);

                HWND hwndFontSizeSlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER);
                int fontPercent = (int)SendMessage(hwndFontSizeSlider, TBM_GETPOS, 0, 0);

                NotificationType notifType = NOTIFICATION_TYPE_CATIME;
                if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME)) {
                    notifType = NOTIFICATION_TYPE_CATIME;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_OS)) {
                    notifType = NOTIFICATION_TYPE_OS;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL)) {
                    notifType = NOTIFICATION_TYPE_SYSTEM_MODAL;
                }

                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                char soundFile[MAX_PATH] = {0};
                if (!GetSelectedNotificationSoundFile(hwndCombo, soundFile, sizeof(soundFile))) {
                    return TRUE;
                }

                HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);

                if (!WriteConfigNotificationSettings(timeout_msg, timeoutMs, opacity,
                                                     notifType, cornerRadius, fontPercent, isDisabled,
                                                     soundFile, volume)) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_EDIT1);
                    return TRUE;
                }

                ClosePreviewNotification();
                CleanupAudioPlayback(state && state->isPlaying);
                if (state) state->isPlaying = FALSE;

                if (state) state->isInitializing = TRUE;

                DestroyWindow(hwndDlg);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                ClosePreviewNotification();

                if (state) SetAudioVolume(state->originalVolume);

                CleanupAudioPlayback(state && state->isPlaying);
                if (state) state->isPlaying = FALSE;

                if (state) state->isInitializing = TRUE;

                DestroyWindow(hwndDlg);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_TEST_SOUND_BUTTON) {
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                if (state) {
                    HandleSoundTestButton(hwndDlg, hwndCombo, hwndSlider,
                                          &state->isPlaying);
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDC_OPEN_SOUND_DIR_BUTTON) {
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                HandleSoundDirButton(hwndDlg, hwndCombo);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_NOTIFICATION_SOUND_COMBO && HIWORD(wParam) == CBN_DROPDOWN) {
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                HandleSoundComboDropdown(hwndCombo);
                return TRUE;
            }
            break;

        case WM_NOTIFICATION_SOUND_PLAYBACK_COMPLETE:
            if (state) state->isPlaying = FALSE;
            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(NULL, L"Test"));
            return TRUE;

        case WM_NOTIFICATION_SOUND_CACHE_UPDATED: {
            HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
            if (hwndCombo && !SendMessage(hwndCombo, CB_GETDROPPEDSTATE, 0, 0)) {
                RefreshNotificationSoundComboBox(hwndCombo);
            }
            return TRUE;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                ClosePreviewNotification();
                if (state) SetAudioVolume(state->originalVolume);
                CleanupAudioPlayback(state && state->isPlaying);
                if (state) {
                    state->isPlaying = FALSE;
                    state->isInitializing = TRUE;
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            ClosePreviewNotification();
            if (state) SetAudioVolume(state->originalVolume);
            CleanupAudioPlayback(state && state->isPlaying);

            if (state) state->isInitializing = TRUE;

            DestroyWindow(hwndDlg);
            return TRUE;

        case WM_DESTROY:
            CleanupAudioPlayback(state && state->isPlaying);
            if (state) state->isPlaying = FALSE;
            ClosePreviewNotification();
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            NotificationSoundCache_SetNotifyWindow(NULL);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_NOTIFICATION_FULL, hwndDlg);
            g_hwndNotificationSettingsDialog = NULL;
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, 0);
            free(state);
            break;
    }
    return FALSE;
}
