#include "dialog_notification_audio_internal.h"

static int FindNotificationSoundItem(
    const wchar_t* items, int count, int firstIndex,
    const wchar_t* value) {
    if (!items || !value || firstIndex < 0) {
        return -1;
    }
    for (int i = firstIndex; i < count; i++) {
        if (wcscmp(items + (size_t)i * MAX_PATH, value) == 0) {
            return i;
        }
    }
    return -1;
}

static int BuildNotificationSoundItems(
    wchar_t items[][MAX_PATH], int capacity, const char* currentFile,
    int* currentSelection) {
    if (!items || capacity < 2) {
        return 0;
    }

    const wchar_t* noneText = GetLocalizedString(NULL, L"None");
    const wchar_t* systemBeepText = GetLocalizedString(NULL, L"System Beep");
    wcsncpy_s(items[0], MAX_PATH, noneText ? noneText : L"None", _TRUNCATE);
    wcsncpy_s(items[1], MAX_PATH,
              systemBeepText ? systemBeepText : L"System Beep", _TRUNCATE);
    int count = 2;
    count += NotificationAudio_CopyCache(
        items + count, capacity - count, NULL);

    int selection = 0;
    if (currentFile && currentFile[0] != '\0') {
        if (strcmp(currentFile, "SYSTEM_BEEP") == 0) {
            selection = 1;
        } else {
            wchar_t currentFileName[MAX_PATH] = {0};
            if (NotificationAudio_GetCurrentFileName(
                    currentFile, currentFileName, MAX_PATH)) {
                int index = FindNotificationSoundItem(
                    items[0], count, 2, currentFileName);
                if (index < 0 && count < capacity &&
                    NotificationAudio_IsSupportedFileName(currentFileName)) {
                    wcsncpy_s(items[count], MAX_PATH,
                              currentFileName, _TRUNCATE);
                    index = count++;
                }
                if (index >= 0) {
                    selection = index;
                }
            }
        }
    }
    if (currentSelection) {
        *currentSelection = selection;
    }
    return count;
}

static BOOL NotificationSoundComboItemsMatch(
    HWND combo, const wchar_t* items, int count) {
    if (!combo || !items || count < 0 ||
        (int)SendMessageW(combo, CB_GETCOUNT, 0, 0) != count) {
        return FALSE;
    }

    for (int i = 0; i < count; i++) {
        LRESULT length = SendMessageW(combo, CB_GETLBTEXTLEN, i, 0);
        if (length == CB_ERR || length < 0 || length >= MAX_PATH) {
            return FALSE;
        }
        wchar_t existing[MAX_PATH] = {0};
        if (SendMessageW(combo, CB_GETLBTEXT, i, (LPARAM)existing) == CB_ERR) {
            return FALSE;
        }
        existing[MAX_PATH - 1] = L'\0';
        if (wcscmp(existing, items + (size_t)i * MAX_PATH) != 0) {
            return FALSE;
        }
    }
    return TRUE;
}

static void ApplyNotificationSoundItems(
    HWND combo, const wchar_t* items, int count, int selection) {
    if (!combo || !items || count < 0) {
        return;
    }
    if (selection < 0 || selection >= count) {
        selection = 0;
    }

    BOOL contentChanged = !NotificationSoundComboItemsMatch(
        combo, items, count);
    int oldSelection = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (!contentChanged && oldSelection == selection) {
        return;
    }

    SendMessageW(combo, WM_SETREDRAW, FALSE, 0);
    if (contentChanged) {
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        SendMessageW(
            combo, CB_INITSTORAGE, count,
            (LPARAM)((size_t)count * MAX_PATH * sizeof(wchar_t)));
        int added = 0;
        for (; added < count; added++) {
            if (SendMessageW(combo, CB_ADDSTRING, 0,
                             (LPARAM)(items + (size_t)added * MAX_PATH)) < 0) {
                break;
            }
        }
        if (selection >= added) {
            selection = 0;
        }
    }
    SendMessageW(combo, CB_SETCURSEL, selection, 0);
    SendMessageW(combo, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(combo, NULL, NULL,
                 RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW |
                 RDW_ALLCHILDREN);
}

void PopulateNotificationSoundComboBox(HWND hwndCombo, const char* currentFile) {
    if (!hwndCombo) {
        return;
    }
    NotificationSoundCache_RequestScanAsync();

    const int capacity = NOTIFICATION_SOUND_ENTRY_LIMIT + 3;
    wchar_t (*items)[MAX_PATH] = calloc((size_t)capacity, sizeof(*items));
    if (!items) {
        return;
    }
    int selection = 0;
    int count = BuildNotificationSoundItems(
        items, capacity, currentFile, &selection);
    ApplyNotificationSoundItems(hwndCombo, items[0], count, selection);
    free(items);
}

void RefreshNotificationSoundComboBox(HWND hwndCombo) {
    if (!hwndCombo) {
        return;
    }

    int selectedIndex = (int)SendMessageW(hwndCombo, CB_GETCURSEL, 0, 0);
    wchar_t selectedFile[MAX_PATH] = {0};
    if (selectedIndex >= 2) {
        SendMessageW(hwndCombo, CB_GETLBTEXT,
                     selectedIndex, (LPARAM)selectedFile);
        selectedFile[MAX_PATH - 1] = L'\0';
    }

    const char* currentFile = selectedIndex > 0
        ? g_AppConfig.notification.sound.sound_file
        : NULL;
    const int capacity = NOTIFICATION_SOUND_ENTRY_LIMIT + 3;
    wchar_t (*items)[MAX_PATH] = calloc((size_t)capacity, sizeof(*items));
    if (!items) {
        return;
    }

    int selection = 0;
    int count = BuildNotificationSoundItems(
        items, capacity, currentFile, &selection);
    if (selectedIndex == 1) {
        selection = 1;
    } else if (selectedIndex >= 2 && selectedFile[0] != L'\0') {
        selection = FindNotificationSoundItem(
            items[0], count, 2, selectedFile);
        if (selection < 0) {
            selection = 0;
        }
    } else if (selectedIndex == 0) {
        selection = 0;
    }
    ApplyNotificationSoundItems(hwndCombo, items[0], count, selection);
    free(items);
}
