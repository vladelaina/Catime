/**
 * @file markdown_inline.c
 * @brief Inline element parsing: links, styles, code, strikethrough
 */

#include "markdown/markdown_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

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

int CountMarkdownColorTags(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;

    while (*p) {
        if (wcsncmp(p, L"<color:", 7) == 0) {
            const wchar_t* end = wcsstr(p, L"</color>");
            if (end) {
                count++;
                p = end + 8;
                continue;
            }
        }
        p++;
    }

    return count;
}

int CountMarkdownFontTags(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;

    while (*p) {
        if (wcsncmp(p, L"<font:", 6) == 0) {
            const wchar_t* end = wcsstr(p, L"</font>");
            if (end) {
                count++;
                p = end + 7;
                continue;
            }
        }
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

/* Parse hex color from wide string: #RRGGBB -> COLORREF */
static COLORREF ParseWideHexColor(const wchar_t* hex) {
    if (!hex || *hex != L'#') return RGB(0, 0, 0);
    
    int r = 0, g = 0, b = 0;
    if (swscanf(hex + 1, L"%02x%02x%02x", &r, &g, &b) == 3) {
        return RGB(r, g, b);
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
    
    if (!EnsureColorTagCapacity(state)) return FALSE;
    
    MarkdownColorTag* tag = &state->colorTags[state->colorTagCount];
    tag->startPos = state->currentPos;
    tag->colorCount = 0;
    
    /* Parse colors (single or gradient separated by _) */
    wchar_t* ctx = NULL;
    wchar_t* token = wcstok_s(colorSpec, L"_", &ctx);
    
    while (token && tag->colorCount < MAX_COLOR_TAG_COLORS) {
        /* Skip leading whitespace */
        while (*token == L' ') token++;
        
        if (*token == L'#') {
            tag->colors[tag->colorCount++] = ParseWideHexColor(token);
        }
        token = wcstok_s(NULL, L"_", &ctx);
    }
    
    if (tag->colorCount == 0) return FALSE;
    
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
                    dest = state->displayText + state->currentPos;
                    continue;
                }
            }
        }
        
        /* Try Markdown styles (bold, italic, strikethrough) */
        if ((*contentSrc == L'*' || *contentSrc == L'_') && contentSrc + 1 < closeTag) {
            if (ExtractMarkdownStyle(&contentSrc, state)) {
                dest = state->displayText + state->currentPos;
                continue;
            }
        }
        
        /* Try strikethrough ~~text~~ */
        if (*contentSrc == L'~' && contentSrc + 1 < closeTag && *(contentSrc + 1) == L'~') {
            if (ExtractMarkdownStrikethrough(&contentSrc, state)) {
                dest = state->displayText + state->currentPos;
                continue;
            }
        }
        
        /* Regular character */
        *dest++ = *contentSrc++;
        state->currentPos++;
    }
    
    tag->endPos = state->currentPos;
    state->colorTagCount++;
    
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
    
    if (!EnsureFontTagCapacity(state)) return FALSE;
    
    MarkdownFontTag* tag = &state->fontTags[state->fontTagCount];
    tag->startPos = state->currentPos;
    
    /* Copy font name */
    wcsncpy(tag->fontName, fontStart, fontNameLen);
    tag->fontName[fontNameLen] = L'\0';
    
    /* Trim whitespace from font name */
    wchar_t* p = tag->fontName;
    while (*p == L' ') p++;
    if (p != tag->fontName) {
        memmove(tag->fontName, p, (wcslen(p) + 1) * sizeof(wchar_t));
    }
    int len = (int)wcslen(tag->fontName);
    while (len > 0 && tag->fontName[len - 1] == L' ') {
        tag->fontName[--len] = L'\0';
    }
    
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
                    dest = state->displayText + state->currentPos;
                    continue;
                }
            }
        }
        
        /* Try Markdown styles (bold, italic, strikethrough) */
        if ((*contentSrc == L'*' || *contentSrc == L'_') && contentSrc + 1 < closeTag) {
            if (ExtractMarkdownStyle(&contentSrc, state)) {
                dest = state->displayText + state->currentPos;
                continue;
            }
        }
        
        /* Try strikethrough ~~text~~ */
        if (*contentSrc == L'~' && contentSrc + 1 < closeTag && *(contentSrc + 1) == L'~') {
            if (ExtractMarkdownStrikethrough(&contentSrc, state)) {
                dest = state->displayText + state->currentPos;
                continue;
            }
        }
        
        /* Regular character */
        *dest++ = *contentSrc++;
        state->currentPos++;
    }
    
    tag->endPos = state->currentPos;
    state->fontTagCount++;
    
    *src = closeTag + 7;  /* Skip </font> */
    return TRUE;
}

/* Process all inline elements at current position */
BOOL ProcessInlineElements(const wchar_t** src, ParseState* state, wchar_t** dest) {
    /* Color tag: <color:#RRGGBB>text</color> */
    if (*src[0] == L'<' && wcsncmp(*src, L"<color:", 7) == 0) {
        if (ExtractMarkdownColorTag(src, state)) {
            *dest = state->displayText + state->currentPos;
            return TRUE;
        }
    }
    
    /* Font tag: <font:FontName>text</font> */
    if (*src[0] == L'<' && wcsncmp(*src, L"<font:", 6) == 0) {
        if (ExtractMarkdownFontTag(src, state)) {
            *dest = state->displayText + state->currentPos;
            return TRUE;
        }
    }

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
