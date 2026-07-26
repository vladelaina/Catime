/**
 * @file dialog_notification_settings_internal.h
 * @brief Private state and helpers for the notification settings dialog.
 */

#ifndef DIALOG_NOTIFICATION_SETTINGS_INTERNAL_H
#define DIALOG_NOTIFICATION_SETTINGS_INTERNAL_H

#include "dialog/dialog_notification.h"

typedef struct {
    BOOL isPlaying;
    BOOL isInitializing;
    BOOL soundComboRefreshPending;
    int originalVolume;
} NotificationSettingsState;

typedef struct {
    HWND hwndDlg;
    UINT dpi;
    HFONT measureFont;
    RECT contentGroup;
    RECT displayGroup;
    RECT audioGroup;
    RECT methodGroup;
    RECT labelRect;
} NotificationLayoutContext;

NotificationSettingsState* DialogNotificationInternal_GetState(HWND hwndDlg);
HWND DialogNotificationInternal_GetDialog(void);
void DialogNotificationInternal_SetDialog(HWND hwndDlg);
BOOL DialogNotificationInternal_IsCurrentProcessWindow(HWND hwnd);
BOOL DialogNotificationInternal_IsValidParent(HWND hwnd);
HWND DialogNotificationInternal_GetParent(HWND hwndDlg);
BOOL DialogNotificationInternal_IsCurrentDialog(HWND hwnd);

int DialogNotificationInternal_MaxWidth(int current, int desired);
int DialogNotificationInternal_MeasureControlText96(
    const NotificationLayoutContext* layout, int controlId, int padding96);
BOOL DialogNotificationInternal_LayoutDisplayControls(
    const NotificationLayoutContext* layout, int* right);
int DialogNotificationInternal_LayoutAudioControls(
    const NotificationLayoutContext* layout);
void DialogNotificationInternal_Layout(HWND hwndDlg);

int DialogNotificationInternal_ClampOpacity(int opacity);
void DialogNotificationInternal_RefreshPreview(HWND hwndDlg);
void DialogNotificationInternal_UpdatePreviewText(HWND hwndDlg,
                                                  const wchar_t* newText);
void DialogNotificationInternal_ClosePreview(void);
BOOL DialogNotificationInternal_SavePreviewPlacement(void);

INT_PTR DialogNotificationInternal_OnInit(HWND hwndDlg);
INT_PTR DialogNotificationInternal_OnHScroll(
    HWND hwndDlg, NotificationSettingsState* state, LPARAM lParam);
INT_PTR DialogNotificationInternal_OnCommand(
    HWND hwndDlg, NotificationSettingsState* state, WPARAM wParam);
void DialogNotificationInternal_CloseWithoutSaving(
    HWND hwndDlg, NotificationSettingsState* state, BOOL clearPlayingState);
void DialogNotificationInternal_OnDestroy(
    HWND hwndDlg, NotificationSettingsState* state);

#endif /* DIALOG_NOTIFICATION_SETTINGS_INTERNAL_H */
