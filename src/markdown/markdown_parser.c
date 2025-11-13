/**
 * @file markdown_parser.c
 * @brief Markdown link parser with single-pass rendering
 */

#include "markdown/markdown_parser.h"
#include <stdlib.h>
#include <string.h>

#define BULLET_POINT L"• "

/** @return TRUE on success, FALSE on allocation failure */
static BOOL ExtractWideString(const wchar_t* start, const wchar_t* end, wchar_t** output) {
    if (!start || !end || start >= end || !output) return FALSE;

    size_t length = end - start;
    *output = (wchar_t*)malloc((length + 1) * sizeof(wchar_t));
    if (!*output) return FALSE;

    wcsncpy(*output, start, length);
    (*output)[length] = L'\0';
    return TRUE;
}

/** Pre-count links for efficient allocation */
static int CountMarkdownLinks(const wchar_t* input) {
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

/** Pre-count headings for efficient allocation */
static int CountMarkdownHeadings(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;
    BOOL atLineStart = TRUE;

    while (*p) {
        if (atLineStart && *p == L'#') {
            const wchar_t* hashEnd = p;
            while (*hashEnd == L'#' && (hashEnd - p) < 4) {
                hashEnd++;
            }
            if (*hashEnd == L' ' && (hashEnd - p) >= 1 && (hashEnd - p) <= 4) {
                count++;
            }
        }
        atLineStart = (*p == L'\n' || *p == L'\r');
        p++;
    }

    return count;
}

/** Pre-count inline styles for efficient allocation */
static int CountMarkdownStyles(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;

    while (*p) {
        if (*p == L'`') {
            const wchar_t* end = p + 1;
            while (*end && *end != L'`') {
                end++;
            }
            if (*end == L'`' && end > p + 1) {
                count++;
                p = end;
            }
        } else if (*p == L'*' || *p == L'_') {
            wchar_t marker = *p;
            int markerCount = 0;
            const wchar_t* start = p;

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

/** Pre-count list items for efficient allocation */
static int CountMarkdownListItems(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;
    BOOL atLineStart = TRUE;

    while (*p) {
        if (atLineStart && (*p == L'-' || *p == L'*') && *(p + 1) == L' ') {
            count++;
        }
        atLineStart = (*p == L'\n' || *p == L'\r');
        p++;
    }

    return count;
}

/** Pre-count blockquotes for efficient allocation */
static int CountMarkdownBlockquotes(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;
    BOOL atLineStart = TRUE;

    while (*p) {
        if (atLineStart && *p == L'>' && *(p + 1) == L' ') {
            count++;
        }
        atLineStart = (*p == L'\n' || *p == L'\r');
        p++;
    }

    return count;
}

static BOOL ExtractMarkdownLink(const wchar_t** src, ParseState* state) {
    if (!src || !*src || !state) return FALSE;

    const wchar_t* linkTextStart = *src + 1;
    const wchar_t* linkTextEnd = wcschr(linkTextStart, L']');

    if (!linkTextEnd || linkTextEnd[1] != L'(') return FALSE;

    const wchar_t* urlStart = linkTextEnd + 2;
    const wchar_t* urlEnd = wcschr(urlStart, L')');

    if (!urlEnd) return FALSE;

    int textLen = linkTextEnd - linkTextStart;
    int urlLen = urlEnd - urlStart;

    if (urlLen == 0) {
        wcsncpy(state->displayText + state->currentPos, linkTextStart, textLen);
        state->currentPos += textLen;
        *src = urlEnd + 1;
        return TRUE;
    }

    if (!EnsureLinkCapacity(state)) return FALSE;

    MarkdownLink* link = &state->links[state->linkCount];

    if (!ExtractWideString(linkTextStart, linkTextEnd, &link->linkText)) {
        return FALSE;
    }

    if (!ExtractWideString(urlStart, urlEnd, &link->linkUrl)) {
        free(link->linkText);
        return FALSE;
    }

    link->startPos = state->currentPos;
    link->endPos = state->currentPos + textLen;
    ZeroMemory(&link->linkRect, sizeof(RECT));

    wcsncpy(state->displayText + state->currentPos, link->linkText, textLen);
    state->currentPos += textLen;
    state->linkCount++;

    *src = urlEnd + 1;
    return TRUE;
}

static BOOL ExtractMarkdownStyle(const wchar_t** src, ParseState* state) {
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

                int textLen = end - textStart;
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

static BOOL ExtractMarkdownCode(const wchar_t** src, ParseState* state) {
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

    int textLen = end - textStart;
    wcsncpy(state->displayText + state->currentPos, textStart, textLen);
    state->currentPos += textLen;

    style->endPos = state->currentPos;
    style->type = STYLE_CODE;

    state->styleCount++;
    *src = end + 1;
    return TRUE;
}

/** Parse [text](url) format, # headings, inline styles, list items, and blockquotes with pre-allocation optimization */
BOOL ParseMarkdownLinks(const wchar_t* input, wchar_t** displayText,
                        MarkdownLink** links, int* linkCount,
                        MarkdownHeading** headings, int* headingCount,
                        MarkdownStyle** styles, int* styleCount,
                        MarkdownListItem** listItems, int* listItemCount,
                        MarkdownBlockquote** blockquotes, int* blockquoteCount) {
    if (!input || !displayText || !links || !linkCount || !headings || !headingCount ||
        !styles || !styleCount || !listItems || !listItemCount || !blockquotes || !blockquoteCount) return FALSE;

    *displayText = NULL;
    *links = NULL;
    *linkCount = 0;
    *headings = NULL;
    *headingCount = 0;
    *styles = NULL;
    *styleCount = 0;
    *listItems = NULL;
    *listItemCount = 0;
    *blockquotes = NULL;
    *blockquoteCount = 0;

    size_t inputLen = wcslen(input);
    ParseState state = {0};

    state.displayText = (wchar_t*)malloc((inputLen + wcslen(BULLET_POINT) * 200 + 1) * sizeof(wchar_t));
    if (!state.displayText) return FALSE;

    int estimatedLinks = CountMarkdownLinks(input);
    state.linkCapacity = GetInitialLinkCapacity(estimatedLinks);
    state.links = (MarkdownLink*)malloc(state.linkCapacity * sizeof(MarkdownLink));

    if (!state.links) {
        CleanupParseState(&state);
        return FALSE;
    }

    int estimatedHeadings = CountMarkdownHeadings(input);
    state.headingCapacity = GetInitialHeadingCapacity(estimatedHeadings);
    state.headings = (MarkdownHeading*)malloc(state.headingCapacity * sizeof(MarkdownHeading));

    if (!state.headings) {
        CleanupParseState(&state);
        return FALSE;
    }

    int estimatedStyles = CountMarkdownStyles(input);
    state.styleCapacity = GetInitialStyleCapacity(estimatedStyles);
    state.styles = (MarkdownStyle*)malloc(state.styleCapacity * sizeof(MarkdownStyle));

    if (!state.styles) {
        CleanupParseState(&state);
        return FALSE;
    }

    int estimatedListItems = CountMarkdownListItems(input);
    state.listItemCapacity = GetInitialListItemCapacity(estimatedListItems);
    state.listItems = (MarkdownListItem*)malloc(state.listItemCapacity * sizeof(MarkdownListItem));

    if (!state.listItems) {
        CleanupParseState(&state);
        return FALSE;
    }

    int estimatedBlockquotes = CountMarkdownBlockquotes(input);
    state.blockquoteCapacity = GetInitialBlockquoteCapacity(estimatedBlockquotes);
    state.blockquotes = (MarkdownBlockquote*)malloc(state.blockquoteCapacity * sizeof(MarkdownBlockquote));

    if (!state.blockquotes) {
        CleanupParseState(&state);
        return FALSE;
    }

    const wchar_t* src = input;
    wchar_t* dest = state.displayText;
    BOOL atLineStart = TRUE;
    BOOL inListItem = FALSE;
    int currentListItemIndex = -1;
    BOOL inHeading = FALSE;
    int currentHeadingIndex = -1;

    while (*src) {
        if (atLineStart) {
            int spaceCount = 0;
            const wchar_t* afterSpaces = src;
            while (*afterSpaces == L' ') {
                spaceCount++;
                afterSpaces++;
            }

            if (*afterSpaces == L'-' || (*afterSpaces == L'*' && *(afterSpaces + 1) == L' ')) {
                if (!EnsureListItemCapacity(&state)) {
                    CleanupParseState(&state);
                    return FALSE;
                }

                int indentLevel = spaceCount / 4;

                while (spaceCount > 0 && *src == L' ') {
                    *dest++ = *src++;
                    state.currentPos++;
                    spaceCount--;
                }

                MarkdownListItem* listItem = &state.listItems[state.listItemCount];
                listItem->startPos = state.currentPos;
                listItem->indentLevel = indentLevel;

                size_t bulletLen = wcslen(BULLET_POINT);
                wcsncpy(dest, BULLET_POINT, bulletLen);
                dest += bulletLen;
                state.currentPos += bulletLen;

                src += 2;

                inListItem = TRUE;
                currentListItemIndex = state.listItemCount;
                state.listItemCount++;

                atLineStart = FALSE;
                continue;
            }
        }

        if (atLineStart && *src == L'#') {
            const wchar_t* hashEnd = src;
            int level = 0;
            while (*hashEnd == L'#' && level < 4) {
                hashEnd++;
                level++;
            }

            if (*hashEnd == L' ' && level >= 1 && level <= 4) {
                if (!EnsureHeadingCapacity(&state)) {
                    CleanupParseState(&state);
                    return FALSE;
                }

                MarkdownHeading* heading = &state.headings[state.headingCount];
                heading->level = level;
                heading->startPos = state.currentPos;

                src = hashEnd + 1;

                inHeading = TRUE;
                currentHeadingIndex = state.headingCount;
                state.headingCount++;

                atLineStart = FALSE;
                continue;
            }
        }

        if (atLineStart && *src == L'>' && *(src + 1) == L' ') {
            if (!EnsureBlockquoteCapacity(&state)) {
                CleanupParseState(&state);
                return FALSE;
            }

            MarkdownBlockquote* blockquote = &state.blockquotes[state.blockquoteCount];
            blockquote->startPos = state.currentPos;
            blockquote->alertType = BLOCKQUOTE_NORMAL;

            src += 2;

            if (*src == L'[' && *(src + 1) == L'!') {
                const wchar_t* alertStart = src + 2;
                const wchar_t* alertEnd = alertStart;
                while (*alertEnd && *alertEnd != L']') {
                    alertEnd++;
                }

                if (*alertEnd == L']') {
                    int alertLen = alertEnd - alertStart;

                    if (alertLen == 4 && wcsncmp(alertStart, L"NOTE", 4) == 0) {
                        blockquote->alertType = BLOCKQUOTE_NOTE;
                        const wchar_t* prefix = L"NOTE: ";
                        size_t prefixLen = wcslen(prefix);
                        wcsncpy(dest, prefix, prefixLen);
                        dest += prefixLen;
                        state.currentPos += prefixLen;
                    } else if (alertLen == 3 && wcsncmp(alertStart, L"TIP", 3) == 0) {
                        blockquote->alertType = BLOCKQUOTE_TIP;
                        const wchar_t* prefix = L"TIP: ";
                        size_t prefixLen = wcslen(prefix);
                        wcsncpy(dest, prefix, prefixLen);
                        dest += prefixLen;
                        state.currentPos += prefixLen;
                    } else if (alertLen == 9 && wcsncmp(alertStart, L"IMPORTANT", 9) == 0) {
                        blockquote->alertType = BLOCKQUOTE_IMPORTANT;
                        const wchar_t* prefix = L"IMPORTANT: ";
                        size_t prefixLen = wcslen(prefix);
                        wcsncpy(dest, prefix, prefixLen);
                        dest += prefixLen;
                        state.currentPos += prefixLen;
                    } else if (alertLen == 7 && wcsncmp(alertStart, L"WARNING", 7) == 0) {
                        blockquote->alertType = BLOCKQUOTE_WARNING;
                        const wchar_t* prefix = L"WARNING: ";
                        size_t prefixLen = wcslen(prefix);
                        wcsncpy(dest, prefix, prefixLen);
                        dest += prefixLen;
                        state.currentPos += prefixLen;
                    } else if (alertLen == 7 && wcsncmp(alertStart, L"CAUTION", 7) == 0) {
                        blockquote->alertType = BLOCKQUOTE_CAUTION;
                        const wchar_t* prefix = L"CAUTION: ";
                        size_t prefixLen = wcslen(prefix);
                        wcsncpy(dest, prefix, prefixLen);
                        dest += prefixLen;
                        state.currentPos += prefixLen;
                    }

                    src = alertEnd + 1;
                    if (*src == L'\n' || *src == L'\r') {
                        src++;
                        if (*src == L'\n' || *src == L'\r') src++;
                        if (*src == L'>' && *(src + 1) == L' ') {
                            src += 2;
                        }
                    }
                }
            }

            BOOL inBlockquote = TRUE;
            int currentBlockquoteIndex = state.blockquoteCount;
            state.blockquoteCount++;

            while (*src && *src != L'\n' && *src != L'\r') {
                if (*src == L'[' && ExtractMarkdownLink(&src, &state)) {
                    dest = state.displayText + state.currentPos;
                    continue;
                }

                if (*src == L'`' && ExtractMarkdownCode(&src, &state)) {
                    dest = state.displayText + state.currentPos;
                    continue;
                }

                if ((*src == L'*' || *src == L'_') && ExtractMarkdownStyle(&src, &state)) {
                    dest = state.displayText + state.currentPos;
                    continue;
                }

                *dest++ = *src++;
                state.currentPos++;
            }

            if (inBlockquote && currentBlockquoteIndex >= 0) {
                state.blockquotes[currentBlockquoteIndex].endPos = state.currentPos;
            }

            atLineStart = FALSE;
            continue;
        }

        if (*src == L'[' && ExtractMarkdownLink(&src, &state)) {
            dest = state.displayText + state.currentPos;
            atLineStart = FALSE;
            continue;
        }

        if (*src == L'`' && ExtractMarkdownCode(&src, &state)) {
            dest = state.displayText + state.currentPos;
            atLineStart = FALSE;
            continue;
        }

        if ((*src == L'*' || *src == L'_') && ExtractMarkdownStyle(&src, &state)) {
            dest = state.displayText + state.currentPos;
            atLineStart = FALSE;
            continue;
        }

        if (*src == L'\n' || *src == L'\r') {
            if (inListItem && currentListItemIndex >= 0) {
                state.listItems[currentListItemIndex].endPos = state.currentPos;
                inListItem = FALSE;
                currentListItemIndex = -1;
            }
            if (inHeading && currentHeadingIndex >= 0) {
                state.headings[currentHeadingIndex].endPos = state.currentPos;
                inHeading = FALSE;
                currentHeadingIndex = -1;
            }
            atLineStart = TRUE;
            *dest++ = *src++;
            state.currentPos++;
            continue;
        }

        atLineStart = FALSE;
        *dest++ = *src++;
        state.currentPos++;
    }

    *dest = L'\0';

    if (inListItem && currentListItemIndex >= 0) {
        state.listItems[currentListItemIndex].endPos = state.currentPos;
    }
    if (inHeading && currentHeadingIndex >= 0) {
        state.headings[currentHeadingIndex].endPos = state.currentPos;
    }

    *displayText = state.displayText;
    *links = state.links;
    *linkCount = state.linkCount;
    *headings = state.headings;
    *headingCount = state.headingCount;
    *styles = state.styles;
    *styleCount = state.styleCount;
    *listItems = state.listItems;
    *listItemCount = state.listItemCount;
    *blockquotes = state.blockquotes;
    *blockquoteCount = state.blockquoteCount;

    return TRUE;
}
