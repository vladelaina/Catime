/**
 * @file markdown_parser.c
 * @brief Markdown link parser with single-pass rendering
 */

#include "markdown/markdown_parser.h"
#include <stdlib.h>
#include <string.h>
#include "log.h"

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

    if (!input || wcslen(input) == 0) return FALSE;

    // Skip BOM if present (double safety)
    if (*input == 0xFEFF) {
        input++;
        if (wcslen(input) == 0) return FALSE;
    }

    // Check for <markdown> tag - only parse content inside tags
    const wchar_t* mdTagStart = wcsstr(input, L"<markdown>");
    const wchar_t* mdTagEnd = wcsstr(input, L"</markdown>");
    
    // If no <markdown> tags, return plain text without any markdown parsing
    if (!mdTagStart || !mdTagEnd || mdTagEnd <= mdTagStart) {
        size_t len = wcslen(input);
        *displayText = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
        if (!*displayText) return FALSE;
        wcscpy(*displayText, input);
        return TRUE;  // Success but no markdown elements
    }
    
    // Build text with tags removed:
    // [text before tag] + [content inside tag] + [text after tag]
    // Remove the tag lines (tag + its trailing/leading newline)
    
    size_t beforeLen = mdTagStart - input;
    const wchar_t* contentStart = mdTagStart + 10;  // Skip "<markdown>"
    
    // Only skip newline after <markdown> if tag is at line start
    // (preceded by newline or at very beginning)
    BOOL tagAtLineStart = (beforeLen == 0) || 
                          (input[beforeLen - 1] == L'\n') || 
                          (input[beforeLen - 1] == L'\r');
    if (tagAtLineStart) {
        if (*contentStart == L'\r') contentStart++;
        if (*contentStart == L'\n') contentStart++;
    }
    
    size_t contentLen = mdTagEnd - contentStart;
    
    // Strip newline before </markdown> tag (remove the tag line)
    while (contentLen > 0 && (contentStart[contentLen - 1] == L'\n' || contentStart[contentLen - 1] == L'\r')) {
        contentLen--;
    }
    
    const wchar_t* afterStart = mdTagEnd + 11;  // Skip "</markdown>"
    // Skip newline after </markdown> tag
    if (*afterStart == L'\r') afterStart++;
    if (*afterStart == L'\n') afterStart++;
    size_t afterLen = wcslen(afterStart);
    
    // Calculate offset for markdown positions (text before tag doesn't get parsed)
    size_t mdStartOffset = beforeLen;
    
    // Create a modifiable copy of just the markdown content for parsing
    wchar_t* mdContent = (wchar_t*)malloc((contentLen + 1) * sizeof(wchar_t));
    if (!mdContent) return FALSE;
    wcsncpy(mdContent, contentStart, contentLen);
    mdContent[contentLen] = L'\0';

    size_t totalLen = beforeLen + contentLen + afterLen;
    ParseState state = {0};
    state.currentPos = (int)beforeLen;  // Start position offset for markdown content

    state.displayText = (wchar_t*)malloc((totalLen + wcslen(BULLET_POINT) * 200 + 1) * sizeof(wchar_t));
    if (!state.displayText) { free(mdContent); return FALSE; }
    
    // Copy text before tag first (plain text, no markdown parsing)
    if (beforeLen > 0) {
        wcsncpy(state.displayText, input, beforeLen);
    }

    int estimatedLinks = CountMarkdownLinks(mdContent);
    state.linkCapacity = GetInitialLinkCapacity(estimatedLinks);
    state.links = (MarkdownLink*)malloc(state.linkCapacity * sizeof(MarkdownLink));

    if (!state.links) {
        CleanupParseState(&state);
        free(mdContent);
        return FALSE;
    }

    int estimatedHeadings = CountMarkdownHeadings(mdContent);
    state.headingCapacity = GetInitialHeadingCapacity(estimatedHeadings);
    state.headings = (MarkdownHeading*)malloc(state.headingCapacity * sizeof(MarkdownHeading));

    if (!state.headings) {
        CleanupParseState(&state);
        free(mdContent);
        return FALSE;
    }

    int estimatedStyles = CountMarkdownStyles(mdContent);
    state.styleCapacity = GetInitialStyleCapacity(estimatedStyles);
    state.styles = (MarkdownStyle*)malloc(state.styleCapacity * sizeof(MarkdownStyle));

    if (!state.styles) {
        CleanupParseState(&state);
        free(mdContent);
        return FALSE;
    }

    int estimatedListItems = CountMarkdownListItems(mdContent);
    state.listItemCapacity = GetInitialListItemCapacity(estimatedListItems);
    state.listItems = (MarkdownListItem*)malloc(state.listItemCapacity * sizeof(MarkdownListItem));

    if (!state.listItems) {
        CleanupParseState(&state);
        free(mdContent);
        return FALSE;
    }

    int estimatedBlockquotes = CountMarkdownBlockquotes(mdContent);
    state.blockquoteCapacity = GetInitialBlockquoteCapacity(estimatedBlockquotes);
    state.blockquotes = (MarkdownBlockquote*)malloc(state.blockquoteCapacity * sizeof(MarkdownBlockquote));

    if (!state.blockquotes) {
        CleanupParseState(&state);
        free(mdContent);
        return FALSE;
    }

    const wchar_t* src = mdContent;
    wchar_t* dest = state.displayText + beforeLen;  // Start after the plain text prefix
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
                    free(mdContent);
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

                // Check for Task List (Checkbox)
                // Syntax: "- [ ] " or "- [x] "
                // src now points to "-" (or "*")
                // We need to check relative to 'afterSpaces' which is aligned with src now (since we consumed spaces)
                // Wait, src was advanced by the while loop above. 
                // Let's verify: 'afterSpaces' was calculated BEFORE the while loop.
                // The while loop consumed spaces from 'src'.
                // So 'src' now points to the first non-space char, which is '-' or '*'.
                
                const wchar_t* p = src;
                wchar_t replacement[8] = {0};
                int advanceSrc = 0;

                if (*p == L'-' && p[1] == L' ' && p[2] == L'[' && p[3] == L' ' && p[4] == L']' && p[5] == L' ') {
                    // Unchecked: "- [ ] " -> "□ " (Geometric Shape - widely supported)
                    wcscpy(replacement, L"\x25A1 "); 
                    advanceSrc = 6;
                } else if (*p == L'-' && p[1] == L' ' && p[2] == L'[' && (p[3] == L'x' || p[3] == L'X') && p[4] == L']' && p[5] == L' ') {
                    // Checked: "- [x] " -> "■ " (Black Square - widely supported)
                    // Alternatively could use checkmark \x2713 or \x2714 but square matches style better
                    wcscpy(replacement, L"\x25A0 ");
                    advanceSrc = 6;
                } else {
                    // Normal bullet
                    wcscpy(replacement, BULLET_POINT);
                    advanceSrc = 2;
                }

                size_t replLen = wcslen(replacement);
                wcsncpy(dest, replacement, replLen);
                dest += replLen;
                state.currentPos += replLen;

                src += advanceSrc;

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
                    free(mdContent);
                    return FALSE;
                }

                MarkdownHeading* heading = &state.headings[state.headingCount];
                heading->level = level;
                heading->startPos = state.currentPos;

                src = hashEnd + 1;

                inHeading = TRUE;
                currentHeadingIndex = state.headingCount;
                state.headingCount++;

                LOG_INFO("MD: Found Heading Level %d at pos %d", level, heading->startPos);

                atLineStart = FALSE;
                continue;
            } else {
                LOG_INFO("MD: Found # at line start but rejected. HashEnd: '%C' Level: %d", *hashEnd, level);
            }
        }

        if (atLineStart && *src == L'>' && *(src + 1) == L' ') {
            if (!EnsureBlockquoteCapacity(&state)) {
                CleanupParseState(&state);
                free(mdContent);
                return FALSE;
            }

            MarkdownBlockquote* blockquote = &state.blockquotes[state.blockquoteCount];
            blockquote->startPos = state.currentPos;
            blockquote->alertType = BLOCKQUOTE_NORMAL;

            src += 2;
            BOOL isAlert = FALSE;

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
                        isAlert = TRUE;
                    } else if (alertLen == 3 && wcsncmp(alertStart, L"TIP", 3) == 0) {
                        blockquote->alertType = BLOCKQUOTE_TIP;
                        const wchar_t* prefix = L"TIP: ";
                        size_t prefixLen = wcslen(prefix);
                        wcsncpy(dest, prefix, prefixLen);
                        dest += prefixLen;
                        state.currentPos += prefixLen;
                        isAlert = TRUE;
                    } else if (alertLen == 9 && wcsncmp(alertStart, L"IMPORTANT", 9) == 0) {
                        blockquote->alertType = BLOCKQUOTE_IMPORTANT;
                        const wchar_t* prefix = L"IMPORTANT: ";
                        size_t prefixLen = wcslen(prefix);
                        wcsncpy(dest, prefix, prefixLen);
                        dest += prefixLen;
                        state.currentPos += prefixLen;
                        isAlert = TRUE;
                    } else if (alertLen == 7 && wcsncmp(alertStart, L"WARNING", 7) == 0) {
                        blockquote->alertType = BLOCKQUOTE_WARNING;
                        const wchar_t* prefix = L"WARNING: ";
                        size_t prefixLen = wcslen(prefix);
                        wcsncpy(dest, prefix, prefixLen);
                        dest += prefixLen;
                        state.currentPos += prefixLen;
                        isAlert = TRUE;
                    } else if (alertLen == 7 && wcsncmp(alertStart, L"CAUTION", 7) == 0) {
                        blockquote->alertType = BLOCKQUOTE_CAUTION;
                        const wchar_t* prefix = L"CAUTION: ";
                        size_t prefixLen = wcslen(prefix);
                        wcsncpy(dest, prefix, prefixLen);
                        dest += prefixLen;
                        state.currentPos += prefixLen;
                        isAlert = TRUE;
                    }

                    if (isAlert) {
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
            }

            if (!isAlert) {
                const wchar_t* prefix = L"\x258C "; // U+258C Left Half Block
                size_t prefixLen = wcslen(prefix);
                wcsncpy(dest, prefix, prefixLen);
                dest += prefixLen;
                state.currentPos += prefixLen;
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

    // Append text after closing tag (plain text, no markdown parsing)
    if (afterLen > 0) {
        wcscpy(dest, afterStart);
        dest += afterLen;
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

    LOG_INFO("MD Parse Done: DisplayText len %d, Links %d, Headings %d, Styles %d", 
             wcslen(state.displayText), state.linkCount, state.headingCount, state.styleCount);

    free(mdContent);  // Free temporary markdown content buffer
    return TRUE;
}
