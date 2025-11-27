/**
 * @file markdown_inline.c
 * @brief Inline element parsing: links, styles, code, strikethrough
 */

#include "markdown/markdown_parser.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

BOOL ExtractWideString(const wchar_t* start, const wchar_t* end, wchar_t** output) {
    if (!start || !end || start >= end || !output) return FALSE;

    size_t length = end - start;
    *output = (wchar_t*)malloc((length + 1) * sizeof(wchar_t));
    if (!*output) return FALSE;

    wcsncpy(*output, start, length);
    (*output)[length] = L'\0';
    return TRUE;
}

/* ============================================================================
 * Pre-count Functions (for efficient allocation)
 * ============================================================================ */

int CountMarkdownLinks(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;

    while (*p) {
        if (*p == L'[') {
            const wchar_t* textEnd = wcschr(p + 1, L']');
            if (textEnd && textEnd[1] == L'(' && wcschr(textEnd + 2, L')')) {
                count++;
            }
        }
        p++;
    }

    return count;
}

int CountMarkdownHeadings(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;
    BOOL atLineStart = TRUE;

    while (*p) {
        if (atLineStart && *p == L'#') {
            const wchar_t* hashEnd = p;
            while (*hashEnd == L'#' && (hashEnd - p) < 6) {
                hashEnd++;
            }
            if (*hashEnd == L' ' && (hashEnd - p) >= 1 && (hashEnd - p) <= 6) {
                count++;
            }
        }
        atLineStart = (*p == L'\n' || *p == L'\r');
        p++;
    }

    return count;
}

int CountMarkdownStyles(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;

    while (*p) {
        if (*p == L'`') {
            const wchar_t* end = p + 1;
            while (*end && *end != L'`') end++;
            if (*end == L'`' && end > p + 1) {
                count++;
                p = end;
            }
        } else if (*p == L'*' || *p == L'_') {
            wchar_t marker = *p;
            int markerCount = 0;
            while (*p == marker && markerCount < 3) {
                markerCount++;
                p++;
            }
            if (markerCount > 0 && *p != L' ' && *p != L'\0') {
                const wchar_t* searchStart = p;
                for (int i = markerCount; i >= 1; i--) {
                    const wchar_t* end = searchStart;
                    while (*end) {
                        if (*end == marker) {
                            int endCount = 0;
                            const wchar_t* endCheck = end;
                            while (*endCheck == marker && endCount < i) {
                                endCount++;
                                endCheck++;
                            }
                            if (endCount == i && end > searchStart) {
                                count++;
                                p = endCheck - 1;
                                break;
                            }
                        }
                        end++;
                    }
                    if (*p != marker) break;
                }
            }
        }
        p++;
    }

    return count;
}

int CountMarkdownListItems(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;
    BOOL atLineStart = TRUE;

    while (*p) {
        if (atLineStart) {
            // Skip leading spaces
            while (*p == L' ') p++;
            // Unordered: -, *, +
            if ((*p == L'-' || *p == L'*' || *p == L'+') && *(p + 1) == L' ') {
                count++;
            }
            // Ordered: digit(s) + '.' + ' '
            else {
                const wchar_t* numCheck = p;
                while (*numCheck >= L'0' && *numCheck <= L'9') numCheck++;
                if (numCheck > p && *numCheck == L'.' && *(numCheck + 1) == L' ') {
                    count++;
                }
            }
        }
        atLineStart = (*p == L'\n' || *p == L'\r');
        p++;
    }

    return count;
}

int CountMarkdownBlockquotes(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;
    BOOL atLineStart = TRUE;

    while (*p) {
        if (atLineStart && *p == L'>') {
            count++;
        }
        atLineStart = (*p == L'\n' || *p == L'\r');
        p++;
    }

    return count;
}

/* ============================================================================
 * Inline Element Extractors
 * ============================================================================ */

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
    wchar_t cleanText[512];
    int cleanLen = StripStyleMarkersWithStyles(linkTextStart, rawTextLen, cleanText, 512,
                                               state, state->currentPos);

    if (urlLen == 0) {
        wcsncpy(state->displayText + state->currentPos, cleanText, cleanLen);
        state->currentPos += cleanLen;
        *src = urlEnd + 1;
        return TRUE;
    }

    if (!EnsureLinkCapacity(state)) return FALSE;

    MarkdownLink* link = &state->links[state->linkCount];

    /* Store clean text (without markers) */
    link->linkText = _wcsdup(cleanText);
    if (!link->linkText) return FALSE;

    if (!ExtractWideString(urlStart, actualUrlEnd, &link->linkUrl)) {
        free(link->linkText);
        return FALSE;
    }

    link->startPos = state->currentPos;
    link->endPos = state->currentPos + cleanLen;
    ZeroMemory(&link->linkRect, sizeof(RECT));

    wcsncpy(state->displayText + state->currentPos, cleanText, cleanLen);
    state->currentPos += cleanLen;
    state->linkCount++;

    *src = urlEnd + 1;
    return TRUE;
}

BOOL ExtractMarkdownStyle(const wchar_t** src, ParseState* state) {
    if (!src || !*src || !state) return FALSE;

    wchar_t marker = **src;
    if (marker != L'*' && marker != L'_') return FALSE;

    const wchar_t* start = *src;
    int markerCount = 0;

    while (**src == marker && markerCount < 3) {
        markerCount++;
        (*src)++;
    }

    if (markerCount == 0 || **src == L' ' || **src == L'\0') {
        *src = start;
        return FALSE;
    }

    const wchar_t* textStart = *src;
    const wchar_t* end = textStart;

    while (*end) {
        if (*end == marker) {
            int endCount = 0;
            const wchar_t* endCheck = end;
            while (*endCheck == marker && endCount < markerCount) {
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
                wcsncpy(state->displayText + state->currentPos, textStart, textLen);
                state->currentPos += textLen;

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
        *src = start;
        return FALSE;
    }

    if (!EnsureStyleCapacity(state)) {
        *src = start;
        return FALSE;
    }

    MarkdownStyle* style = &state->styles[state->styleCount];
    style->startPos = state->currentPos;

    int textLen = (int)(end - textStart);
    wcsncpy(state->displayText + state->currentPos, textStart, textLen);
    state->currentPos += textLen;

    style->endPos = state->currentPos;
    style->type = STYLE_CODE;

    state->styleCount++;
    *src = end + 1;
    return TRUE;
}

BOOL ExtractMarkdownStrikethrough(const wchar_t** src, ParseState* state) {
    if (!src || !*src || !state) return FALSE;

    if (**src != L'~' || *(*src + 1) != L'~') return FALSE;

    const wchar_t* start = *src;
    const wchar_t* textStart = *src + 2;
    const wchar_t* end = textStart;

    while (*end) {
        if (*end == L'~' && *(end + 1) == L'~') {
            if (end > textStart) {
                if (!EnsureStyleCapacity(state)) {
                    *src = start;
                    return FALSE;
                }

                MarkdownStyle* style = &state->styles[state->styleCount];
                style->startPos = state->currentPos;

                int textLen = (int)(end - textStart);
                wcsncpy(state->displayText + state->currentPos, textStart, textLen);
                state->currentPos += textLen;

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

/* Process all inline elements at current position */
BOOL ProcessInlineElements(const wchar_t** src, ParseState* state, wchar_t** dest) {
    if (*src[0] == L'[' && ExtractMarkdownLink(src, state)) {
        *dest = state->displayText + state->currentPos;
        return TRUE;
    }

    if (*src[0] == L'`' && ExtractMarkdownCode(src, state)) {
        *dest = state->displayText + state->currentPos;
        return TRUE;
    }

    if ((*src[0] == L'*' || *src[0] == L'_') && ExtractMarkdownStyle(src, state)) {
        *dest = state->displayText + state->currentPos;
        return TRUE;
    }

    if (*src[0] == L'~' && ExtractMarkdownStrikethrough(src, state)) {
        *dest = state->displayText + state->currentPos;
        return TRUE;
    }

    return FALSE;
}
