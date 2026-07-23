/**
 * @file markdown_inline_tags.c
 * @brief Nested color and font tag parsing.
 */

#include "markdown_inline_internal.h"

/* Parse hex color from wide string: #RRGGBB -> COLORREF */
static COLORREF ParseWideHexColor(const wchar_t* hex) {
    if (!hex || *hex != L'#') return RGB(0, 0, 0);

    unsigned int r = 0, g = 0, b = 0;
    if (swscanf(hex + 1, L"%02x%02x%02x", &r, &g, &b) == 3) {
        return RGB((int)r, (int)g, (int)b);
    }
    return RGB(0, 0, 0);
}

BOOL ExtractMarkdownColorTag(const wchar_t** src, ParseState* state) {
    if (!src || !*src || !state) return FALSE;

    /* Check for <color: prefix */
    if (wcsncmp(*src, L"<color:", 7) != 0) return FALSE;

    const wchar_t* colorStart = *src + 7;

    /* Find closing > of opening tag */
    const wchar_t* tagEnd = wcschr(colorStart, L'>');
    if (!tagEnd) return FALSE;

    /* Find </color> closing tag */
    const wchar_t* closeTag = wcsstr(tagEnd, L"</color>");
    if (!closeTag) return FALSE;

    /* Extract color specification (between : and >) */
    int colorSpecLen = (int)(tagEnd - colorStart);
    if (colorSpecLen <= 0 || colorSpecLen >= 128) return FALSE;

    wchar_t colorSpec[128];
    wcsncpy(colorSpec, colorStart, colorSpecLen);
    colorSpec[colorSpecLen] = L'\0';

    COLORREF colors[MAX_COLOR_TAG_COLORS];
    int colorCount = 0;

    /* Parse colors (single or gradient separated by _) */
    wchar_t* ctx = NULL;
    const wchar_t* token = wcstok_s(colorSpec, L"_", &ctx);

    while (token && colorCount < MAX_COLOR_TAG_COLORS) {
        /* Skip leading whitespace */
        while (*token == L' ') token++;

        if (*token == L'#') {
            colors[colorCount++] = ParseWideHexColor(token);
        }
        token = wcstok_s(NULL, L"_", &ctx);
    }

    if (colorCount == 0) return FALSE;
    if (!EnsureColorTagCapacity(state)) return FALSE;

    int colorTagIndex = state->colorTagCount++;
    MarkdownColorTag* tag = &state->colorTags[colorTagIndex];
    tag->startPos = state->currentPos;
    tag->endPos = state->currentPos;
    tag->colorCount = colorCount;
    for (int i = 0; i < colorCount; i++) {
        tag->colors[i] = colors[i];
    }

    /* Parse content (between > and </color>) - supports nested tags and styles */
    const wchar_t* contentSrc = tagEnd + 1;
    wchar_t* dest = state->displayText + state->currentPos;

    while (contentSrc < closeTag) {
        /* Try nested font tag */
        if (*contentSrc == L'<' && wcsncmp(contentSrc, L"<font:", 6) == 0) {
            /* Check if closing tag is before </color> */
            const wchar_t* nestedClose = wcsstr(contentSrc, L"</font>");
            if (nestedClose && nestedClose < closeTag) {
                if (ExtractMarkdownFontTag(&contentSrc, state)) {
                    SyncMarkdownOutputPointer(state, &dest);
                    continue;
                }
            }
        }

        /* Try Markdown styles (bold, italic, strikethrough) */
        if ((*contentSrc == L'*' || *contentSrc == L'_') && contentSrc + 1 < closeTag) {
            if (MarkdownInline_ExtractStyleBounded(&contentSrc, state, closeTag)) {
                SyncMarkdownOutputPointer(state, &dest);
                continue;
            }
        }

        /* Try strikethrough ~~text~~ */
        if (*contentSrc == L'~' && contentSrc + 1 < closeTag && *(contentSrc + 1) == L'~') {
            if (MarkdownInline_ExtractStrikethroughBounded(&contentSrc, state, closeTag)) {
                SyncMarkdownOutputPointer(state, &dest);
                continue;
            }
        }

        /* Regular character */
        if (!AppendMarkdownOutputChar(state, *contentSrc++)) {
            return FALSE;
        }
        SyncMarkdownOutputPointer(state, &dest);
    }

    state->colorTags[colorTagIndex].endPos = state->currentPos;

    *src = closeTag + 8;  /* Skip </color> */
    return TRUE;
}

BOOL ExtractMarkdownFontTag(const wchar_t** src, ParseState* state) {
    if (!src || !*src || !state) return FALSE;

    /* Check for <font: prefix */
    if (wcsncmp(*src, L"<font:", 6) != 0) return FALSE;

    const wchar_t* fontStart = *src + 6;

    /* Find closing > of opening tag */
    const wchar_t* tagEnd = wcschr(fontStart, L'>');
    if (!tagEnd) return FALSE;

    /* Find </font> closing tag */
    const wchar_t* closeTag = wcsstr(tagEnd, L"</font>");
    if (!closeTag) return FALSE;

    /* Extract font name (between : and >) */
    int fontNameLen = (int)(tagEnd - fontStart);
    if (fontNameLen <= 0 || fontNameLen >= MAX_FONT_NAME_LENGTH) return FALSE;

    wchar_t fontName[MAX_FONT_NAME_LENGTH];
    wcsncpy(fontName, fontStart, fontNameLen);
    fontName[fontNameLen] = L'\0';

    /* Trim whitespace from font name */
    wchar_t* p = fontName;
    while (*p == L' ') p++;
    if (p != fontName) {
        memmove(fontName, p, (wcslen(p) + 1) * sizeof(wchar_t));
    }
    int len = (int)wcslen(fontName);
    while (len > 0 && fontName[len - 1] == L' ') {
        fontName[--len] = L'\0';
    }
    if (len == 0) return FALSE;
    if (!EnsureFontTagCapacity(state)) return FALSE;

    int fontTagIndex = state->fontTagCount++;
    MarkdownFontTag* tag = &state->fontTags[fontTagIndex];
    tag->startPos = state->currentPos;
    tag->endPos = state->currentPos;
    wcscpy_s(tag->fontName, MAX_FONT_NAME_LENGTH, fontName);

    /* Parse content (between > and </font>) - supports nested tags and styles */
    const wchar_t* contentSrc = tagEnd + 1;
    wchar_t* dest = state->displayText + state->currentPos;

    while (contentSrc < closeTag) {
        /* Try nested color tag */
        if (*contentSrc == L'<' && wcsncmp(contentSrc, L"<color:", 7) == 0) {
            /* Check if closing tag is before </font> */
            const wchar_t* nestedClose = wcsstr(contentSrc, L"</color>");
            if (nestedClose && nestedClose < closeTag) {
                if (ExtractMarkdownColorTag(&contentSrc, state)) {
                    SyncMarkdownOutputPointer(state, &dest);
                    continue;
                }
            }
        }

        /* Try Markdown styles (bold, italic, strikethrough) */
        if ((*contentSrc == L'*' || *contentSrc == L'_') && contentSrc + 1 < closeTag) {
            if (MarkdownInline_ExtractStyleBounded(&contentSrc, state, closeTag)) {
                SyncMarkdownOutputPointer(state, &dest);
                continue;
            }
        }

        /* Try strikethrough ~~text~~ */
        if (*contentSrc == L'~' && contentSrc + 1 < closeTag && *(contentSrc + 1) == L'~') {
            if (MarkdownInline_ExtractStrikethroughBounded(&contentSrc, state, closeTag)) {
                SyncMarkdownOutputPointer(state, &dest);
                continue;
            }
        }

        /* Regular character */
        if (!AppendMarkdownOutputChar(state, *contentSrc++)) {
            return FALSE;
        }
        SyncMarkdownOutputPointer(state, &dest);
    }

    state->fontTags[fontTagIndex].endPos = state->currentPos;

    *src = closeTag + 7;  /* Skip </font> */
    return TRUE;
}
