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
    BOOL soundComboRefreshPending;
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
