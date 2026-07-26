/**
 * @file dialog_notification_layout_display.c
 * @brief Lays out notification display controls.
 */

#include "dialog/dialog_notification_settings_internal.h"
#include "dialog/dialog_modern.h"
#include "../resource/resource.h"

BOOL DialogNotificationInternal_LayoutDisplayControls(
    const NotificationLayoutContext* layout, int* right) {
    if (!layout || !right) return FALSE;

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
    if (!DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_TIME_LABEL,
                                     layout->dpi, &timeLabel) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_RADIUS_LABEL,
                                     layout->dpi, &radiusLabel) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_OPACITY_LABEL,
                                     layout->dpi, &opacityLabel) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_FONT_SIZE_LABEL,
                                     layout->dpi, &fontLabel) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_TIME_EDIT,
                                     layout->dpi, &timeEdit) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_DISABLE_NOTIFICATION_CHECK,
                                     layout->dpi, &check) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_RADIUS_SLIDER,
                                     layout->dpi, &radiusSlider) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_OPACITY_EDIT,
                                     layout->dpi, &opacitySlider) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_FONT_SIZE_SLIDER,
                                     layout->dpi, &fontSlider) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_RADIUS_TEXT,
                                     layout->dpi, &radiusText) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_OPACITY_TEXT,
                                     layout->dpi, &opacityText) ||
        !DialogModern_GetChildRect96(layout->hwndDlg,
                                     IDC_NOTIFICATION_FONT_SIZE_TEXT,
                                     layout->dpi, &fontText)) {
        return FALSE;
    }

    int displayLabelWidth = timeLabel.right - timeLabel.left;
    const int labelIds[] = {
        IDC_NOTIFICATION_TIME_LABEL,
        IDC_NOTIFICATION_RADIUS_LABEL,
        IDC_NOTIFICATION_OPACITY_LABEL,
        IDC_NOTIFICATION_FONT_SIZE_LABEL
    };
    for (size_t i = 0; i < _countof(labelIds); i++) {
        displayLabelWidth = DialogNotificationInternal_MaxWidth(
            displayLabelWidth,
            DialogNotificationInternal_MeasureControlText96(
                layout, labelIds[i], 12));
    }
    if (displayLabelWidth > 360) displayLabelWidth = 360;

    int sliderX = timeLabel.left + displayLabelWidth + 12;
    int sliderWidth = radiusSlider.right - radiusSlider.left;
    int valueX = sliderX + sliderWidth + 10;
    int timeWidth = timeEdit.right - timeEdit.left;
    int checkX = sliderX + timeWidth + 16;
    int checkWidth = DialogNotificationInternal_MaxWidth(
        check.right - check.left,
        DialogNotificationInternal_MeasureControlText96(
            layout, IDC_DISABLE_NOTIFICATION_CHECK, 28));

    const int displayLabelIds[] = {
        IDC_NOTIFICATION_TIME_LABEL,
        IDC_NOTIFICATION_RADIUS_LABEL,
        IDC_NOTIFICATION_OPACITY_LABEL,
        IDC_NOTIFICATION_FONT_SIZE_LABEL
    };
    const RECT displayLabelRects[] = {
        timeLabel, radiusLabel, opacityLabel, fontLabel
    };
    for (size_t i = 0; i < _countof(displayLabelIds); i++) {
        DialogModern_SetChildRect96(
            layout->hwndDlg, displayLabelIds[i], layout->dpi,
            displayLabelRects[i].left, displayLabelRects[i].top,
            displayLabelWidth,
            displayLabelRects[i].bottom - displayLabelRects[i].top);
    }

    DialogModern_SetChildRect96(layout->hwndDlg,
                                IDC_NOTIFICATION_TIME_EDIT, layout->dpi,
                                sliderX, timeEdit.top, timeWidth,
                                timeEdit.bottom - timeEdit.top);
    DialogModern_SetChildRect96(layout->hwndDlg,
                                IDC_DISABLE_NOTIFICATION_CHECK, layout->dpi,
                                checkX, check.top, checkWidth,
                                check.bottom - check.top);

    const int sliderIds[] = {
        IDC_NOTIFICATION_RADIUS_SLIDER,
        IDC_NOTIFICATION_OPACITY_EDIT,
        IDC_NOTIFICATION_FONT_SIZE_SLIDER
    };
    const RECT sliderRects[] = {radiusSlider, opacitySlider, fontSlider};
    for (size_t i = 0; i < _countof(sliderIds); i++) {
        DialogModern_SetChildRect96(
            layout->hwndDlg, sliderIds[i], layout->dpi,
            sliderX, sliderRects[i].top, sliderWidth,
            sliderRects[i].bottom - sliderRects[i].top);
    }

    const int valueIds[] = {
        IDC_NOTIFICATION_RADIUS_TEXT,
        IDC_NOTIFICATION_OPACITY_TEXT,
        IDC_NOTIFICATION_FONT_SIZE_TEXT
    };
    const RECT valueRects[] = {radiusText, opacityText, fontText};
    for (size_t i = 0; i < _countof(valueIds); i++) {
        DialogModern_SetChildRect96(
            layout->hwndDlg, valueIds[i], layout->dpi,
            valueX, valueRects[i].top,
            valueRects[i].right - valueRects[i].left,
            valueRects[i].bottom - valueRects[i].top);
    }

    *right = layout->displayGroup.right;
    *right = DialogNotificationInternal_MaxWidth(
        *right, checkX + checkWidth + 10);
    *right = DialogNotificationInternal_MaxWidth(
        *right, valueX + (radiusText.right - radiusText.left) + 10);
    return TRUE;
}
