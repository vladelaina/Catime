/**
 * @file markdown_inline_link.c
 * @brief Markdown link parsing with nested text styles.
 */

#include "markdown_inline_internal.h"

/* Helper: Strip style markers from link text and record styles */
static int StripStyleMarkersWithStyles(const wchar_t* src, int srcLen, wchar_t* dst, int dstSize,
                                       ParseState* state, int basePos) {
    int dstPos = 0;
    int i = 0;

    /* Track active styles */
    int boldItalicStart = -1, boldStart = -1, italicStart = -1, strikeStart = -1;

    while (i < srcLen && dstPos < dstSize - 1) {
        /* Check for *** or ___ (bold-italic) */
        if (i + 2 < srcLen && ((src[i] == L'*' && src[i+1] == L'*' && src[i+2] == L'*') ||
                               (src[i] == L'_' && src[i+1] == L'_' && src[i+2] == L'_'))) {
            if (boldItalicStart < 0) {
                boldItalicStart = dstPos;
            } else {
                if (state && EnsureStyleCapacity(state)) {
                    MarkdownStyle* style = &state->styles[state->styleCount];
                    style->type = STYLE_BOLD_ITALIC;
                    style->startPos = basePos + boldItalicStart;
                    style->endPos = basePos + dstPos;
                    state->styleCount++;
                }
                boldItalicStart = -1;
            }
            i += 3;
            continue;
        }
        /* Check for ** or __ (bold) */
        if (i + 1 < srcLen && ((src[i] == L'*' && src[i+1] == L'*') ||
                               (src[i] == L'_' && src[i+1] == L'_'))) {
            if (boldStart < 0) {
                boldStart = dstPos;
            } else {
                if (state && EnsureStyleCapacity(state)) {
                    MarkdownStyle* style = &state->styles[state->styleCount];
                    style->type = STYLE_BOLD;
                    style->startPos = basePos + boldStart;
                    style->endPos = basePos + dstPos;
                    state->styleCount++;
                }
                boldStart = -1;
            }
            i += 2;
            continue;
        }
        /* Check for ~~ (strikethrough) */
        if (i + 1 < srcLen && src[i] == L'~' && src[i+1] == L'~') {
            if (strikeStart < 0) {
                strikeStart = dstPos;
            } else {
                if (state && EnsureStyleCapacity(state)) {
                    MarkdownStyle* style = &state->styles[state->styleCount];
                    style->type = STYLE_STRIKETHROUGH;
                    style->startPos = basePos + strikeStart;
                    style->endPos = basePos + dstPos;
                    state->styleCount++;
                }
                strikeStart = -1;
            }
            i += 2;
            continue;
        }
        /* Check for single * or _ (italic) */
        if ((src[i] == L'*' || src[i] == L'_') &&
            (i + 1 >= srcLen || src[i+1] != src[i])) {
            if (italicStart < 0) {
                italicStart = dstPos;
            } else {
                if (state && EnsureStyleCapacity(state)) {
                    MarkdownStyle* style = &state->styles[state->styleCount];
                    style->type = STYLE_ITALIC;
                    style->startPos = basePos + italicStart;
                    style->endPos = basePos + dstPos;
                    state->styleCount++;
                }
                italicStart = -1;
            }
            i++;
            continue;
        }
        dst[dstPos++] = src[i++];
    }
    dst[dstPos] = L'\0';
    return dstPos;
}

BOOL ExtractMarkdownLink(const wchar_t** src, ParseState* state) {
    if (!src || !*src || !state) return FALSE;

    const wchar_t* linkTextStart = *src + 1;
    const wchar_t* linkTextEnd = wcschr(linkTextStart, L']');

    if (!linkTextEnd || linkTextEnd[1] != L'(') return FALSE;

    const wchar_t* urlStart = linkTextEnd + 2;
    const wchar_t* urlEnd = urlStart;

    // Find closing ')' handling optional title in quotes
    while (*urlEnd && *urlEnd != L')') {
        if (*urlEnd == L'"' || *urlEnd == L'\'') {
            wchar_t quote = *urlEnd++;
            while (*urlEnd && *urlEnd != quote) urlEnd++;
            if (*urlEnd == quote) urlEnd++;
        } else {
            urlEnd++;
        }
    }

    if (!*urlEnd) return FALSE;

    // Find actual URL end (before space/title)
    const wchar_t* actualUrlEnd = urlStart;
    while (actualUrlEnd < urlEnd && *actualUrlEnd != L' ' && *actualUrlEnd != L'"' && *actualUrlEnd != L'\'') {
        actualUrlEnd++;
    }

    int rawTextLen = (int)(linkTextEnd - linkTextStart);
    int urlLen = (int)(actualUrlEnd - urlStart);

    /* Strip style markers from link text and record styles */
    if (rawTextLen < 0 || urlLen < 0) return FALSE;
    if (rawTextLen > MARKDOWN_LINK_TEXT_MAX_CHARS ||
        urlLen > MARKDOWN_LINK_URL_MAX_CHARS) {
        return FALSE;
    }

    wchar_t stackCleanText[512];
    wchar_t* heapCleanText = NULL;
    wchar_t* cleanText = stackCleanText;
    int cleanTextCapacity = (int)_countof(stackCleanText);
    if (rawTextLen >= cleanTextCapacity) {
        if ((size_t)rawTextLen > ((size_t)-1 / sizeof(wchar_t)) - 1) {
            return FALSE;
        }
        heapCleanText = (wchar_t*)malloc(((size_t)rawTextLen + 1) * sizeof(wchar_t));
        if (!heapCleanText) {
            return FALSE;
        }
        cleanText = heapCleanText;
        cleanTextCapacity = rawTextLen + 1;
    }

    int originalStyleCount = state->styleCount;
    int cleanLen = StripStyleMarkersWithStyles(linkTextStart, rawTextLen, cleanText,
                                               cleanTextCapacity, state, state->currentPos);

    if (urlLen == 0) {
        state->styleCount = originalStyleCount;
        if (!AppendMarkdownOutputSpan(state, cleanText, (size_t)cleanLen)) {
            free(heapCleanText);
            return FALSE;
        }
        *src = urlEnd + 1;
        free(heapCleanText);
        return TRUE;
    }

    if (!EnsureLinkCapacity(state)) {
        state->styleCount = originalStyleCount;
        free(heapCleanText);
        return FALSE;
    }

    MarkdownLink* link = &state->links[state->linkCount];

    /* Store clean text (without markers) */
    link->linkText = _wcsdup(cleanText);
    if (!link->linkText) {
        state->styleCount = originalStyleCount;
        free(heapCleanText);
        return FALSE;
    }

    if (!ExtractWideString(urlStart, actualUrlEnd, &link->linkUrl)) {
        free(link->linkText);
        link->linkText = NULL;
        state->styleCount = originalStyleCount;
        free(heapCleanText);
        return FALSE;
    }

    link->startPos = state->currentPos;
    link->endPos = state->currentPos + cleanLen;
    ZeroMemory(&link->linkRect, sizeof(RECT));

    if (!AppendMarkdownOutputSpan(state, cleanText, (size_t)cleanLen)) {
        free(link->linkUrl);
        link->linkUrl = NULL;
        free(link->linkText);
        link->linkText = NULL;
        state->styleCount = originalStyleCount;
        free(heapCleanText);
        return FALSE;
    }
    state->linkCount++;

    *src = urlEnd + 1;
    free(heapCleanText);
    return TRUE;
}
