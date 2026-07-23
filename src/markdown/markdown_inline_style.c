/**
 * @file markdown_inline_style.c
 * @brief Bounded style, code, and strikethrough parsing.
 */

#include "markdown_inline_internal.h"

static BOOL IsBeforeInlineLimit(const wchar_t* p, const wchar_t* limit) {
    return !limit || p < limit;
}

static BOOL IsAtInlineLimitOrEnd(const wchar_t* p, const wchar_t* limit) {
    return limit ? p >= limit : *p == L'\0';
}

BOOL MarkdownInline_ExtractStyleBounded(const wchar_t** src, ParseState* state,
                                        const wchar_t* limit) {
    if (!src || !*src || !state) return FALSE;
    if (!IsBeforeInlineLimit(*src, limit)) return FALSE;

    wchar_t marker = **src;
    if (marker != L'*' && marker != L'_') return FALSE;

    const wchar_t* start = *src;
    int markerCount = 0;

    while (IsBeforeInlineLimit(*src, limit) && **src == marker && markerCount < 3) {
        markerCount++;
        (*src)++;
    }

    if (markerCount == 0 || IsAtInlineLimitOrEnd(*src, limit) || **src == L' ') {
        *src = start;
        return FALSE;
    }

    const wchar_t* textStart = *src;
    const wchar_t* end = textStart;

    while (IsBeforeInlineLimit(end, limit) && *end) {
        if (*end == marker) {
            int endCount = 0;
            const wchar_t* endCheck = end;
            while (IsBeforeInlineLimit(endCheck, limit) &&
                   *endCheck == marker &&
                   endCount < markerCount) {
                endCount++;
                endCheck++;
            }
            if (endCount == markerCount && end > textStart) {
                if (!EnsureStyleCapacity(state)) {
                    *src = start;
                    return FALSE;
                }

                MarkdownStyle* style = &state->styles[state->styleCount];
                style->startPos = state->currentPos;

                int textLen = (int)(end - textStart);
                if (!AppendMarkdownOutputSpan(state, textStart, (size_t)textLen)) {
                    *src = start;
                    return FALSE;
                }

                style->endPos = state->currentPos;

                if (markerCount == 3) style->type = STYLE_BOLD_ITALIC;
                else if (markerCount == 2) style->type = STYLE_BOLD;
                else style->type = STYLE_ITALIC;

                state->styleCount++;
                *src = endCheck;
                return TRUE;
            }
        }
        end++;
    }

    *src = start;
    return FALSE;
}

BOOL ExtractMarkdownStyle(const wchar_t** src, ParseState* state) {
    return MarkdownInline_ExtractStyleBounded(src, state, NULL);
}

BOOL ExtractMarkdownCode(const wchar_t** src, ParseState* state) {
    if (!src || !*src || !state) return FALSE;

    if (**src != L'`') return FALSE;

    const wchar_t* start = *src;
    const wchar_t* textStart = *src + 1;
    const wchar_t* end = textStart;

    while (*end && *end != L'`') {
        end++;
    }

    if (*end != L'`' || end == textStart) {
        return FALSE;
    }

    if (!EnsureStyleCapacity(state)) {
        *src = start;
        return FALSE;
    }

    MarkdownStyle* style = &state->styles[state->styleCount];
    style->startPos = state->currentPos;

    int textLen = (int)(end - textStart);
    if (!AppendMarkdownOutputSpan(state, textStart, (size_t)textLen)) {
        *src = start;
        return FALSE;
    }

    style->endPos = state->currentPos;
    style->type = STYLE_CODE;

    state->styleCount++;
    *src = end + 1;
    return TRUE;
}

BOOL MarkdownInline_ExtractStrikethroughBounded(const wchar_t** src, ParseState* state,
                                                const wchar_t* limit) {
    if (!src || !*src || !state) return FALSE;
    if (!IsBeforeInlineLimit(*src, limit)) return FALSE;

    if (**src != L'~' ||
        !IsBeforeInlineLimit(*src + 1, limit) ||
        *(*src + 1) != L'~') return FALSE;

    const wchar_t* start = *src;
    const wchar_t* textStart = *src + 2;
    const wchar_t* end = textStart;

    while (IsBeforeInlineLimit(end, limit) && *end) {
        if (*end == L'~' &&
            IsBeforeInlineLimit(end + 1, limit) &&
            *(end + 1) == L'~') {
            if (end > textStart) {
                if (!EnsureStyleCapacity(state)) {
                    *src = start;
                    return FALSE;
                }

                MarkdownStyle* style = &state->styles[state->styleCount];
                style->startPos = state->currentPos;

                int textLen = (int)(end - textStart);
                if (!AppendMarkdownOutputSpan(state, textStart, (size_t)textLen)) {
                    *src = start;
                    return FALSE;
                }

                style->endPos = state->currentPos;
                style->type = STYLE_STRIKETHROUGH;

                state->styleCount++;
                *src = end + 2;
                return TRUE;
            }
            break;
        }
        end++;
    }

    *src = start;
    return FALSE;
}

BOOL ExtractMarkdownStrikethrough(const wchar_t** src, ParseState* state) {
    return MarkdownInline_ExtractStrikethroughBounded(src, state, NULL);
}
