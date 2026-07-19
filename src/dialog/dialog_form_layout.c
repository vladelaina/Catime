/**
 * @file dialog_form_layout.c
 * @brief Responsive form layouts shared by localized dialogs.
 */

#include "dialog/dialog_form_layout.h"
#include "dialog/dialog_modern.h"
#include <stdlib.h>
#include <wchar.h>

#define DIALOG_FORM_MAX_INSTRUCTION_CHARS 4096

static int DialogFormLayout_To96(UINT dpi, int value) {
    return MulDiv(value, 96, (int)(dpi ? dpi : 96u));
}

void DialogFormLayout_ApplyInstruction(HWND hwndDlg, int instructionId,
                                       int editId, int actionId) {
    if (!hwndDlg) return;
    HWND instruction = GetDlgItem(hwndDlg, instructionId);
    HWND edit = GetDlgItem(hwndDlg, editId);
    HWND action = GetDlgItem(hwndDlg, actionId);
    if (!instruction || !edit || !action) return;

    UINT dpi = DialogModern_GetDpi(hwndDlg);
    RECT instructionRect = {0};
    RECT editRect = {0};
    if (!DialogModern_GetChildRect96(hwndDlg, instructionId, dpi,
                                     &instructionRect) ||
        !DialogModern_GetChildRect96(hwndDlg, editId, dpi, &editRect)) {
        return;
    }

    int textLength = GetWindowTextLengthW(instruction);
    if (textLength <= 0 || textLength > DIALOG_FORM_MAX_INSTRUCTION_CHARS) {
        return;
    }

    wchar_t* text = (wchar_t*)calloc((size_t)textLength + 1, sizeof(*text));
    HFONT bodyFont = DialogModern_CreateFont(dpi, 12, FW_NORMAL);
    HFONT buttonFont = DialogModern_CreateFont(dpi, 12, FW_SEMIBOLD);
    if (!text || !bodyFont ||
        GetWindowTextW(instruction, text, textLength + 1) <= 0) {
        free(text);
        if (bodyFont) DeleteObject(bodyFont);
        if (buttonFont) DeleteObject(buttonFont);
        return;
    }

    HDC hdc = GetDC(instruction);
    int longestLine96 = 0;
    if (hdc) {
        HGDIOBJ oldFont = SelectObject(hdc, bodyFont);
        const wchar_t* line = text;
        while (*line) {
            const wchar_t* end = wcschr(line, L'\n');
            int length = end ? (int)(end - line) : (int)wcslen(line);
            if (length > 0 && line[length - 1] == L'\r') length--;
            SIZE lineSize = {0};
            if (length > 0 &&
                GetTextExtentPoint32W(hdc, line, length, &lineSize)) {
                int width96 = DialogFormLayout_To96(dpi, lineSize.cx);
                if (width96 > longestLine96) longestLine96 = width96;
            }
            if (!end) break;
            line = end + 1;
        }
        SelectObject(hdc, oldFont);
        ReleaseDC(instruction, hdc);
    }

    int originalWidth96 = instructionRect.right - instructionRect.left;
    int contentWidth96 = longestLine96 + 16;
    if (contentWidth96 < originalWidth96) contentWidth96 = originalWidth96;
    if (contentWidth96 < 320) contentWidth96 = 320;
    if (contentWidth96 > 480) contentWidth96 = 480;

    int textHeight96 = instructionRect.bottom - instructionRect.top;
    hdc = GetDC(instruction);
    if (hdc) {
        HGDIOBJ oldFont = SelectObject(hdc, bodyFont);
        RECT measure = {0, 0, DialogModern_Scale(dpi, contentWidth96), 0};
        if (DrawTextW(hdc, text, -1, &measure,
                      DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL |
                          DT_NOPREFIX) > 0) {
            int measured96 = DialogFormLayout_To96(dpi, measure.bottom) + 8;
            if (measured96 > textHeight96) textHeight96 = measured96;
        }
        SelectObject(hdc, oldFont);
        ReleaseDC(instruction, hdc);
    }
    if (textHeight96 > 300) textHeight96 = 300;

    wchar_t buttonText[256] = {0};
    SIZE buttonTextSize = {0};
    GetWindowTextW(action, buttonText, _countof(buttonText));
    int buttonWidth96 = 80;
    if (buttonFont &&
        DialogModern_MeasureText96(action, buttonFont, buttonText, dpi,
                                   &buttonTextSize)) {
        buttonWidth96 = buttonTextSize.cx + 30;
        if (buttonWidth96 < 80) buttonWidth96 = 80;
        if (buttonWidth96 > 220) buttonWidth96 = 220;
    }

    int editHeight96 = editRect.bottom - editRect.top;
    if (editHeight96 < 36) editHeight96 = 36;
    int editY96 = instructionRect.top + textHeight96 + 14;
    int buttonY96 = editY96 + editHeight96 + 20;
    DialogModern_SetChildRect96(hwndDlg, instructionId, dpi,
                                instructionRect.left, instructionRect.top,
                                contentWidth96, textHeight96);
    DialogModern_SetChildRect96(hwndDlg, editId, dpi,
                                instructionRect.left, editY96,
                                contentWidth96, editHeight96);
    DialogModern_SetChildRect96(
        hwndDlg, actionId, dpi,
        instructionRect.left + contentWidth96 - buttonWidth96,
        buttonY96, buttonWidth96, 36);

    free(text);
    DeleteObject(bodyFont);
    if (buttonFont) DeleteObject(buttonFont);
}
