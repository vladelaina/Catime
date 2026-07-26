/**
 * @file dialog_notification_layout.c
 * @brief Coordinates responsive layout for notification settings controls.
 */

#include "dialog/dialog_notification_settings_internal.h"
#include "dialog/dialog_modern.h"
#include "../resource/resource.h"

static BOOL InitializeNotificationLayout(HWND hwndDlg,
                                         NotificationLayoutContext* layout) {
    if (!hwndDlg || !layout) return FALSE;

    *layout = (NotificationLayoutContext){0};
    layout->hwndDlg = hwndDlg;
    layout->dpi = DialogModern_GetDpi(hwndDlg);
    layout->measureFont = DialogModern_CreateFont(layout->dpi, 12, FW_NORMAL);
    if (!layout->measureFont ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_CONTENT_GROUP,
                                     layout->dpi, &layout->contentGroup) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_DISPLAY_GROUP,
                                     layout->dpi, &layout->displayGroup) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_AUDIO_GROUP,
                                     layout->dpi, &layout->audioGroup) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_METHOD_GROUP,
                                     layout->dpi, &layout->methodGroup) ||
        !DialogModern_GetChildRect96(hwndDlg, IDC_NOTIFICATION_LABEL1,
                                     layout->dpi, &layout->labelRect)) {
        if (layout->measureFont) DeleteObject(layout->measureFont);
        layout->measureFont = NULL;
        return FALSE;
    }
    return TRUE;
}

int DialogNotificationInternal_MeasureControlText96(
    const NotificationLayoutContext* layout, int controlId, int padding96) {
    if (!layout) return padding96;

    HWND control = GetDlgItem(layout->hwndDlg, controlId);
    wchar_t text[512] = {0};
    SIZE size = {0};
    if (!control || !GetWindowTextW(control, text, _countof(text)) ||
        !DialogModern_MeasureText96(layout->hwndDlg, layout->measureFont,
                                    text, layout->dpi, &size)) {
        return padding96;
    }
    return size.cx + padding96;
}

int DialogNotificationInternal_MaxWidth(int current, int desired) {
    return desired > current ? desired : current;
}

static int LayoutNotificationMethodControls(
    const NotificationLayoutContext* layout) {
    int methodX = layout->methodGroup.left + 10;
    int methodRight = methodX;
    const int radioIds[] = {
        IDC_NOTIFICATION_TYPE_CATIME,
        IDC_NOTIFICATION_TYPE_OS,
        IDC_NOTIFICATION_TYPE_SYSTEM_MODAL
    };
    for (size_t i = 0; i < _countof(radioIds); i++) {
        RECT radio = {0};
        if (!DialogModern_GetChildRect96(layout->hwndDlg, radioIds[i],
                                         layout->dpi, &radio)) {
            continue;
        }
        int width = DialogNotificationInternal_MaxWidth(
            radio.right - radio.left,
            DialogNotificationInternal_MeasureControlText96(
                layout, radioIds[i], 28));
        DialogModern_SetChildRect96(layout->hwndDlg, radioIds[i], layout->dpi,
                                    methodX, radio.top, width,
                                    radio.bottom - radio.top);
        methodX += width + 16;
        methodRight = methodX - 16;
    }
    return methodRight + 10;
}

static int LayoutNotificationContentControls(
    const NotificationLayoutContext* layout) {
    int contentLabelWidth = DialogNotificationInternal_MaxWidth(
        layout->labelRect.right - layout->labelRect.left,
        DialogNotificationInternal_MeasureControlText96(
            layout, IDC_NOTIFICATION_LABEL1, 12));
    DialogModern_SetChildRect96(
        layout->hwndDlg, IDC_NOTIFICATION_LABEL1, layout->dpi,
        layout->labelRect.left, layout->labelRect.top, contentLabelWidth,
        layout->labelRect.bottom - layout->labelRect.top);

    int contentRight = layout->labelRect.left + contentLabelWidth + 10;
    RECT contentEdit = {0};
    if (DialogModern_GetChildRect96(layout->hwndDlg, IDC_NOTIFICATION_EDIT1,
                                    layout->dpi, &contentEdit)) {
        contentRight = DialogNotificationInternal_MaxWidth(
            contentRight, contentEdit.right + 10);
    }
    return contentRight;
}

static void ExpandNotificationContentAndGroups(
    const NotificationLayoutContext* layout, int right) {
    RECT contentEdit = {0};
    if (DialogModern_GetChildRect96(layout->hwndDlg, IDC_NOTIFICATION_EDIT1,
                                    layout->dpi, &contentEdit)) {
        int editWidth = right - contentEdit.left - 10;
        if (editWidth < contentEdit.right - contentEdit.left) {
            editWidth = contentEdit.right - contentEdit.left;
        }
        DialogModern_SetChildRect96(layout->hwndDlg, IDC_NOTIFICATION_EDIT1,
                                    layout->dpi, contentEdit.left,
                                    contentEdit.top, editWidth,
                                    contentEdit.bottom - contentEdit.top);
    }

    const int groupIds[] = {
        IDC_NOTIFICATION_CONTENT_GROUP,
        IDC_NOTIFICATION_DISPLAY_GROUP,
        IDC_NOTIFICATION_AUDIO_GROUP,
        IDC_NOTIFICATION_METHOD_GROUP
    };
    for (size_t i = 0; i < _countof(groupIds); i++) {
        RECT group = {0};
        if (DialogModern_GetChildRect96(layout->hwndDlg, groupIds[i],
                                        layout->dpi, &group)) {
            DialogModern_SetChildRect96(layout->hwndDlg, groupIds[i],
                                        layout->dpi, group.left, group.top,
                                        right - group.left,
                                        group.bottom - group.top);
        }
    }
}

void DialogNotificationInternal_Layout(HWND hwndDlg) {
    NotificationLayoutContext layout = {0};
    if (!InitializeNotificationLayout(hwndDlg, &layout)) return;

    int displayRight = 0;
    if (!DialogNotificationInternal_LayoutDisplayControls(
            &layout, &displayRight)) {
        DeleteObject(layout.measureFont);
        return;
    }

    int right = LayoutNotificationContentControls(&layout);
    right = DialogNotificationInternal_MaxWidth(right, displayRight);
    right = DialogNotificationInternal_MaxWidth(
        right, DialogNotificationInternal_LayoutAudioControls(&layout));
    right = DialogNotificationInternal_MaxWidth(
        right, LayoutNotificationMethodControls(&layout));

    ExpandNotificationContentAndGroups(&layout, right);
    DeleteObject(layout.measureFont);
}
