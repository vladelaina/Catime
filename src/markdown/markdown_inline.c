/**
 * @file markdown_inline.c
 * @brief Common inline parser entry points.
 */

#include "markdown_inline_internal.h"

BOOL ExtractWideString(const wchar_t* start, const wchar_t* end, wchar_t** output) {
    if (!start || !end || start >= end || !output) return FALSE;

    size_t length = end - start;
    if (length > (SIZE_MAX / sizeof(wchar_t)) - 1) return FALSE;
    *output = (wchar_t*)malloc((length + 1) * sizeof(wchar_t));
    if (!*output) return FALSE;

    wcsncpy(*output, start, length);
    (*output)[length] = L'\0';
    return TRUE;
}

/* Process all inline elements at current position */
BOOL ProcessInlineElements(const wchar_t** src, ParseState* state, wchar_t** dest) {
    /* Color tag: <color:#RRGGBB>text</color> */
    if (*src[0] == L'<' && wcsncmp(*src, L"<color:", 7) == 0) {
        if (ExtractMarkdownColorTag(src, state)) {
            SyncMarkdownOutputPointer(state, dest);
            return TRUE;
        }
    }

    /* Font tag: <font:FontName>text</font> */
    if (*src[0] == L'<' && wcsncmp(*src, L"<font:", 6) == 0) {
        if (ExtractMarkdownFontTag(src, state)) {
            SyncMarkdownOutputPointer(state, dest);
            return TRUE;
        }
    }

    if (*src[0] == L'[' && ExtractMarkdownLink(src, state)) {
        SyncMarkdownOutputPointer(state, dest);
        return TRUE;
    }

    if (*src[0] == L'`' && ExtractMarkdownCode(src, state)) {
        SyncMarkdownOutputPointer(state, dest);
        return TRUE;
    }

    if ((*src[0] == L'*' || *src[0] == L'_') && ExtractMarkdownStyle(src, state)) {
        SyncMarkdownOutputPointer(state, dest);
        return TRUE;
    }

    if (*src[0] == L'~' && ExtractMarkdownStrikethrough(src, state)) {
        SyncMarkdownOutputPointer(state, dest);
        return TRUE;
    }

    return FALSE;
}
