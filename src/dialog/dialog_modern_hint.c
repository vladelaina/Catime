/**
 * @file dialog_modern_hint.c
 * @brief Countdown-style instruction panels and live input feedback.
 */

#include "dialog_modern_internal.h"
#include <wctype.h>

#define MODERN_HINT_MAX_CHARS 4096
#define MODERN_HINT_MAX_LINES 64

static wchar_t* ModernHintReadText(HWND hwnd) {
    int length = GetWindowTextLengthW(hwnd);
    if (length <= 0 || length > MODERN_HINT_MAX_CHARS) return NULL;
    wchar_t* text = (wchar_t*)calloc((size_t)length + 1, sizeof(*text));
    if (!text) return NULL;
    if (GetWindowTextW(hwnd, text, length + 1) <= 0) {
        free(text);
        return NULL;
    }
    return text;
}

static int ModernHintSplitLines(wchar_t* text, wchar_t** lines, int capacity) {
    if (!text || !lines || capacity <= 0) return 0;
    int count = 0;
    wchar_t* cursor = text;
    while (*cursor && count < capacity) {
        lines[count++] = cursor;
        while (*cursor && *cursor != L'\r' && *cursor != L'\n') cursor++;
        if (!*cursor) break;
        wchar_t newline = *cursor;
        *cursor++ = L'\0';
        if (newline == L'\r' && *cursor == L'\n') cursor++;
    }
    return count;
}

static wchar_t* ModernHintTrim(wchar_t* text) {
    while (*text == L' ' || *text == L'\t') text++;
    wchar_t* end = text + wcslen(text);
    while (end > text && (end[-1] == L' ' || end[-1] == L'\t')) end--;
    *end = L'\0';
    return text;
}

static wchar_t* ModernHintFindSeparator(const wchar_t* line) {
    wchar_t* separator = wcschr(line, L'=');
    if (separator) return separator;
    if (!iswdigit(*line)) return NULL;
    separator = wcschr(line, L':');
    return separator ? separator : wcschr(line, L'\xff1a');
}

static int ModernHintFontHeight(HDC hdc, HFONT font) {
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    TEXTMETRICW metrics = {0};
    int height = GetTextMetricsW(hdc, &metrics)
        ? metrics.tmHeight + metrics.tmExternalLeading : 0;
    if (oldFont) SelectObject(hdc, oldFont);
    return height;
}

static int ModernHintMeasureWrapped(HDC hdc, HFONT font,
                                    const wchar_t* text, int width) {
    if (!text || !text[0] || width <= 0) return 0;
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    RECT measure = {0, 0, width, 0};
    DrawTextW(hdc, text, -1, &measure,
              DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    if (oldFont) SelectObject(hdc, oldFont);
    return measure.bottom - measure.top;
}

static int ModernHintMeasureToken(HDC hdc, HFONT font,
                                  const wchar_t* line,
                                  const wchar_t* separator) {
    const wchar_t* end = separator;
    while (end > line && (end[-1] == L' ' || end[-1] == L'\t')) end--;
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    SIZE size = {0};
    GetTextExtentPoint32W(hdc, line, (int)(end - line), &size);
    if (oldFont) SelectObject(hdc, oldFont);
    return size.cx;
}

static void ModernHintDrawTokenRow(HDC hdc, ModernDialogState* state,
                                   const wchar_t* line, wchar_t* separator,
                                   const RECT* content, int* y,
                                   int tokenWidth, int rowGap) {
    wchar_t* tokenEnd = separator;
    while (tokenEnd > line &&
           (tokenEnd[-1] == L' ' || tokenEnd[-1] == L'\t')) tokenEnd--;
    const wchar_t* explanation = separator + 1;
    while (*explanation == L' ' || *explanation == L'\t') explanation++;

    int textGap = DialogModern_Scale(state->dpi, 8);
    int explanationWidth = content->right - content->left - tokenWidth - textGap;
    int labelHeight = ModernHintFontHeight(hdc, state->labelFont);
    int explanationHeight = ModernHintMeasureWrapped(
        hdc, state->bodyFont, explanation, explanationWidth);
    int rowHeight = max(labelHeight, explanationHeight);
    if (rowHeight <= 0) return;

    RECT tokenRect = {content->left, *y,
                      content->left + tokenWidth, *y + rowHeight};
    RECT explanationRect = {tokenRect.right + textGap, *y,
                            content->right, *y + rowHeight};
    if (explanationHeight > 0 && explanationHeight < rowHeight) {
        explanationRect.top += (rowHeight - explanationHeight) / 2;
    }

    wchar_t saved = *tokenEnd;
    *tokenEnd = L'\0';
    DialogModern_DrawText(hdc, state->labelFont, state->palette.accent,
                          &tokenRect, line,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                              DT_END_ELLIPSIS);
    *tokenEnd = saved;
    DialogModern_DrawText(hdc, state->bodyFont, state->palette.mutedText,
                          &explanationRect, explanation,
                          DT_LEFT | DT_WORDBREAK);
    *y += rowHeight + rowGap;
}

static void ModernHintDrawPlainRow(HDC hdc, ModernDialogState* state,
                                   const wchar_t* line, const RECT* content,
                                   int* y, int rowGap) {
    BOOL code = line[0] == L'#' || line[0] == L'<';
    HFONT font = code ? state->labelFont : state->bodyFont;
    COLORREF color = code ? state->palette.accent : state->palette.mutedText;
    int height = ModernHintMeasureWrapped(
        hdc, font, line, content->right - content->left);
    if (height <= 0) return;
    RECT row = {content->left, *y, content->right, *y + height};
    DialogModern_DrawText(hdc, font, color, &row, line,
                          DT_LEFT | DT_WORDBREAK);
    *y += height + rowGap;
}

static void ModernDrawInstruction(ModernControl* control, HDC hdc,
                                  const RECT* client) {
    ModernDialogState* state = control->owner;
    FillRect(hdc, client, state->surfaceBrush);
    DialogModern_DrawRoundedRect(
        hdc, client, DialogModern_Scale(state->dpi, 18),
        state->palette.field,
        state->palette.highContrast ? state->palette.border
                                    : state->palette.field,
        state->palette.highContrast ? 1 : 0);

    int inset = DialogModern_Scale(state->dpi, 16);
    RECT label = {client->left + inset,
                  client->top + DialogModern_Scale(state->dpi, 7),
                  client->right - inset,
                  client->top + DialogModern_Scale(state->dpi, 25)};
    const wchar_t* format = GetLocalizedString(NULL, L"Format");
    DialogModern_DrawText(hdc, state->labelFont, state->palette.mutedText,
                          &label, format ? format : L"Format",
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                              DT_END_ELLIPSIS);

    wchar_t* text = ModernHintReadText(control->hwnd);
    if (!text) return;
    wchar_t* lines[MODERN_HINT_MAX_LINES] = {0};
    int count = ModernHintSplitLines(text, lines, _countof(lines));
    RECT content = {client->left + inset,
                    client->top + DialogModern_Scale(state->dpi, 30),
                    client->right - inset,
                    client->bottom - DialogModern_Scale(state->dpi, 9)};
    int savedDc = SaveDC(hdc);
    IntersectClipRect(hdc, content.left, content.top,
                      content.right, content.bottom);

    int maximumToken = 0;
    for (int i = 0; i < count; i++) {
        lines[i] = ModernHintTrim(lines[i]);
        const wchar_t* separator = ModernHintFindSeparator(lines[i]);
        if (separator) {
            maximumToken = max(maximumToken, ModernHintMeasureToken(
                hdc, state->labelFont, lines[i], separator));
        }
    }
    int tokenWidth = maximumToken + DialogModern_Scale(state->dpi, 4);
    int maximumWidth = (content.right - content.left) * 45 / 100;
    if (tokenWidth > maximumWidth) tokenWidth = maximumWidth;
    int minimumWidth = DialogModern_Scale(state->dpi, 34);
    if (tokenWidth < minimumWidth) tokenWidth = minimumWidth;

    int y = content.top;
    int rowGap = DialogModern_Scale(state->dpi, 2);
    int blankGap = DialogModern_Scale(state->dpi, 5);
    for (int i = 0; i < count && y < content.bottom; i++) {
        if (!lines[i][0]) {
            y += blankGap;
            continue;
        }
        wchar_t* separator = ModernHintFindSeparator(lines[i]);
        if (separator) {
            ModernHintDrawTokenRow(hdc, state, lines[i], separator,
                                   &content, &y, tokenWidth, rowGap);
        } else {
            ModernHintDrawPlainRow(hdc, state, lines[i],
                                   &content, &y, rowGap);
        }
    }
    RestoreDC(hdc, savedDc);
    free(text);
}

void ModernPaintInstruction(ModernControl* control, HDC suppliedDc) {
    const ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd) return;
    PAINTSTRUCT paint = {0};
    HDC hdc = suppliedDc ? suppliedDc : BeginPaint(control->hwnd, &paint);
    if (!hdc) return;

    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    HDC buffer = width > 0 && height > 0 ? CreateCompatibleDC(hdc) : NULL;
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(hdc, width, height) : NULL;
    HGDIOBJ oldBitmap = buffer && bitmap ? SelectObject(buffer, bitmap) : NULL;
    HDC target = buffer && bitmap ? buffer : hdc;

    ModernDrawInstruction(control, target, &client);
    if (buffer && bitmap) {
        BitBlt(hdc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldBitmap);
    }
    if (bitmap) DeleteObject(bitmap);
    if (buffer) DeleteDC(buffer);
    if (!suppliedDc) EndPaint(control->hwnd, &paint);
}
