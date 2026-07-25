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

static HWND ResolvePreviewNotificationWindow(void) {
    if (!g_hwndPreviewNotification ||
        !IsWindow(g_hwndPreviewNotification) ||
        !IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        g_hwndPreviewNotification = FindPreviewNotificationWindow();
    }

    if (g_hwndPreviewNotification &&
        IsWindow(g_hwndPreviewNotification) &&
        IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        return g_hwndPreviewNotification;
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
    HWND hwndPreview = ResolvePreviewNotificationWindow();
    if (!hwndPreview) return;
    int opacity = ClampNotificationOpacityForDialog(
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
    HWND preview = g_hwndPreviewNotification;
    g_hwndPreviewNotification = NULL;
    if (preview && IsWindow(preview) &&
        IsToastNotificationPreviewWindow(preview)) {
        DestroyWindow(preview);
    }
