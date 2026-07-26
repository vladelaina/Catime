/**
 * @file dialog_notification_layout_audio.c
 * @brief Lays out notification sound and volume controls.
 */

#include "dialog/dialog_notification_settings_internal.h"
#include "dialog/dialog_modern.h"
#include "../resource/resource.h"

int DialogNotificationInternal_LayoutAudioControls(
    const NotificationLayoutContext* layout) {
    if (!layout) return 0;

    RECT soundLabel = {0};
    RECT soundCombo = {0};
    RECT testButton = {0};
    RECT soundDirButton = {0};
    RECT volumeLabel = {0};
    RECT volumeSlider = {0};
    RECT volumeText = {0};
    int audioRight = layout->audioGroup.right;
    if (!DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_SOUND_LABEL,
                                     layout->dpi, &soundLabel) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_SOUND_COMBO,
                                     layout->dpi, &soundCombo) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_TEST_SOUND_BUTTON,
                                     layout->dpi, &testButton) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_OPEN_SOUND_DIR_BUTTON,
                                     layout->dpi, &soundDirButton) ||
        !DialogModern_GetChildRect96(layout->hwndDlg, IDC_VOLUME_LABEL,
                                     layout->dpi, &volumeLabel) ||
        !DialogModern_GetChildRect96(layout->hwndDlg, IDC_VOLUME_SLIDER,
                                     layout->dpi, &volumeSlider) ||
        !DialogModern_GetChildRect96(layout->hwndDlg, IDC_VOLUME_TEXT,
                                     layout->dpi, &volumeText)) {
        return audioRight;
    }

    int soundLabelWidth = DialogNotificationInternal_MaxWidth(
        soundLabel.right - soundLabel.left,
        DialogNotificationInternal_MeasureControlText96(
            layout, IDC_NOTIFICATION_SOUND_LABEL, 12));
    int comboX = soundLabel.left + soundLabelWidth + 10;
    int comboWidth = DialogNotificationInternal_MaxWidth(
        soundCombo.right - soundCombo.left, 150);
    int testWidth = DialogNotificationInternal_MaxWidth(
        testButton.right - testButton.left,
        DialogNotificationInternal_MeasureControlText96(
            layout, IDC_TEST_SOUND_BUTTON, 24));
    int dirWidth = DialogNotificationInternal_MaxWidth(
        soundDirButton.right - soundDirButton.left,
        DialogNotificationInternal_MeasureControlText96(
            layout, IDC_OPEN_SOUND_DIR_BUTTON, 24));
    int testX = comboX + comboWidth + 10;
    int dirX = testX + testWidth + 10;

    DialogModern_SetChildRect96(layout->hwndDlg,
                                IDC_NOTIFICATION_SOUND_LABEL, layout->dpi,
                                soundLabel.left, soundLabel.top,
                                soundLabelWidth,
                                soundLabel.bottom - soundLabel.top);
    DialogModern_SetChildRect96(layout->hwndDlg,
                                IDC_NOTIFICATION_SOUND_COMBO, layout->dpi,
                                comboX, soundCombo.top, comboWidth,
                                soundCombo.bottom - soundCombo.top);
    DialogModern_SetChildRect96(layout->hwndDlg,
                                IDC_TEST_SOUND_BUTTON, layout->dpi,
                                testX, testButton.top, testWidth,
                                testButton.bottom - testButton.top);
    DialogModern_SetChildRect96(layout->hwndDlg,
                                IDC_OPEN_SOUND_DIR_BUTTON, layout->dpi,
                                dirX, soundDirButton.top, dirWidth,
                                soundDirButton.bottom - soundDirButton.top);

    int volumeLabelWidth = DialogNotificationInternal_MaxWidth(
        volumeLabel.right - volumeLabel.left,
        DialogNotificationInternal_MeasureControlText96(
            layout, IDC_VOLUME_LABEL, 12));
    int volumeSliderX = volumeLabel.left + volumeLabelWidth + 10;
    int volumeSliderWidth = volumeSlider.right - volumeSlider.left;
    int volumeTextX = volumeSliderX + volumeSliderWidth + 10;
    DialogModern_SetChildRect96(layout->hwndDlg, IDC_VOLUME_LABEL,
                                layout->dpi, volumeLabel.left,
                                volumeLabel.top, volumeLabelWidth,
                                volumeLabel.bottom - volumeLabel.top);
    DialogModern_SetChildRect96(layout->hwndDlg, IDC_VOLUME_SLIDER,
                                layout->dpi, volumeSliderX,
                                volumeSlider.top, volumeSliderWidth,
                                volumeSlider.bottom - volumeSlider.top);
    DialogModern_SetChildRect96(layout->hwndDlg, IDC_VOLUME_TEXT,
                                layout->dpi, volumeTextX, volumeText.top,
                                volumeText.right - volumeText.left,
                                volumeText.bottom - volumeText.top);

    audioRight = DialogNotificationInternal_MaxWidth(
        audioRight, dirX + dirWidth + 10);
    audioRight = DialogNotificationInternal_MaxWidth(
        audioRight,
        volumeTextX + (volumeText.right - volumeText.left) + 10);
    return audioRight;
}
