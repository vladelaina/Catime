/**
 * @file hotkey_dialog_layout.c
 * @brief Localized labels, responsive columns, and control subclass setup
 */
#include "hotkey_internal.h"

#include "dialog/dialog_modern.h"
#include "language.h"

static void LayoutColumns(HWND dialog) {
    UINT dpi = DialogModern_GetDpi(dialog);
    HFONT font = DialogModern_CreateFont(dpi, 12, FW_NORMAL);
    int labelWidth96 = 128;

    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        HWND label = GetDlgItem(dialog, g_hotkeyMetadata[i].labelCtrlId);
        wchar_t text[256] = {0};
        SIZE size = {0};
        if (!label || !GetWindowTextW(label, text, _countof(text))) continue;
        if (DialogModern_MeasureText96(label, font, text, dpi, &size) &&
            size.cx + 8 > labelWidth96) {
            labelWidth96 = size.cx + 8;
        }
    }
    if (labelWidth96 > 280) labelWidth96 = 280;
    if (font) DeleteObject(font);

    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        RECT labelRect = {0};
        RECT editRect = {0};
        int gap96;
        if (!GetDlgItem(dialog, g_hotkeyMetadata[i].labelCtrlId) ||
            !GetDlgItem(dialog, g_hotkeyMetadata[i].editCtrlId) ||
            !DialogModern_GetChildRect96(
                dialog, g_hotkeyMetadata[i].labelCtrlId, dpi, &labelRect) ||
            !DialogModern_GetChildRect96(
                dialog, g_hotkeyMetadata[i].editCtrlId, dpi, &editRect)) {
            continue;
        }
        gap96 = editRect.left - labelRect.right;
        if (gap96 < 12) gap96 = 12;
        DialogModern_SetChildRect96(
            dialog, g_hotkeyMetadata[i].labelCtrlId, dpi,
            labelRect.left, labelRect.top, labelWidth96,
            labelRect.bottom - labelRect.top);
        DialogModern_SetChildRect96(
            dialog, g_hotkeyMetadata[i].editCtrlId, dpi,
            labelRect.left + labelWidth96 + gap96, editRect.top,
            editRect.right - editRect.left,
            editRect.bottom - editRect.top);
    }
}

void Hotkey_InitializeLabels(HWND dialog) {
    SetWindowTextW(dialog, GetLocalizedString(NULL, L"Hotkey Settings"));
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        SetDlgItemTextW(
            dialog, g_hotkeyMetadata[i].labelCtrlId,
            GetLocalizedString(g_hotkeyMetadata[i].labelCN,
                               g_hotkeyMetadata[i].labelEN));
    }
    SetDlgItemTextW(dialog, IDOK, GetLocalizedString(NULL, L"OK"));
    SetDlgItemTextW(dialog, IDCANCEL, GetLocalizedString(NULL, L"Cancel"));
    LayoutColumns(dialog);
}

void Hotkey_SetupControlSubclasses(HWND dialog) {
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        HWND control = GetDlgItem(dialog, g_hotkeyMetadata[i].editCtrlId);
        if (control) {
            SetWindowSubclass(control, HotkeyControlSubclassProc,
                              g_hotkeyMetadata[i].editCtrlId, 0);
        }
    }
}

void Hotkey_RemoveControlSubclasses(HWND dialog) {
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        HWND control = GetDlgItem(dialog, g_hotkeyMetadata[i].editCtrlId);
        if (control) {
            RemoveWindowSubclass(control, HotkeyControlSubclassProc,
                                 g_hotkeyMetadata[i].editCtrlId);
        }
    }
}
