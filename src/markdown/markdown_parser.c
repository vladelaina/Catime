#include "markdown/markdown_parser.h"
#include "markdown_parser_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
BOOL ParseMarkdownLinks(const wchar_t* input, wchar_t** displayText, MarkdownLink** links, int* linkCount, MarkdownHeading** headings, int* headingCount, MarkdownStyle** styles, int* styleCount, MarkdownListItem** listItems, int* listItemCount, MarkdownBlockquote** blockquotes, int* blockquoteCount, MarkdownColorTag** colorTags, int* colorTagCount, MarkdownFontTag** fontTags, int* fontTagCount) {
    if (!input || !displayText || !links || !linkCount || !headings || !headingCount || !styles || !styleCount || !listItems || !listItemCount || !blockquotes || !blockquoteCount || !colorTags || !colorTagCount || !fontTags || !fontTagCount) return FALSE;
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
    *colorTags = NULL;
    *colorTagCount = 0;
    *fontTags = NULL;
    *fontTagCount = 0;
    if (*input == L'\0') return FALSE;
    if (*input == 0xFEFF) {
        input++;
        if (*input == L'\0') return FALSE;
    }
    size_t inputLen = wcslen(input);
    const wchar_t* mdTagStart = wcsstr(input, L"<md>");
    const wchar_t* mdTagEnd = wcsstr(input, L"</md>");
    BOOL hasColorTags = (wcsstr(input, L"<color:") != NULL);
    BOOL hasFontTags = (wcsstr(input, L"<font:") != NULL);
    BOOL hasRichTextTags = hasColorTags || hasFontTags;
    if (!mdTagStart || !mdTagEnd || mdTagEnd <= mdTagStart) {
        if (!hasRichTextTags) {
            size_t len = inputLen;
            if (len > (SIZE_MAX / sizeof(wchar_t)) - 1) return FALSE;
            *displayText = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
            if (!*displayText) return FALSE;
            wcscpy_s(*displayText, len + 1, input);
            return TRUE;  // Success but no markdown elements
        }
        return ParseMarkdownRichText(input, displayText, links, linkCount,
                                     headings, headingCount, styles, styleCount,
                                     listItems, listItemCount, blockquotes,
                                     blockquoteCount, colorTags, colorTagCount,
                                     fontTags, fontTagCount);
    }
    size_t beforeLen = mdTagStart - input;
    const wchar_t* contentStart = mdTagStart + 4;  // Skip "<md>"
    BOOL tagAtLineStart = (beforeLen == 0) ||
                          (input[beforeLen - 1] == L'\n') ||
                          (input[beforeLen - 1] == L'\r');
    if (tagAtLineStart) {
        if (*contentStart == L'\r') contentStart++;
        if (*contentStart == L'\n') contentStart++;
    }
    size_t contentLen = mdTagEnd - contentStart;
    while (contentLen > 0 && (contentStart[contentLen - 1] == L'\n' || contentStart[contentLen - 1] == L'\r')) {
        contentLen--;
    }
    const wchar_t* afterStart = mdTagEnd + 5;  // Skip "</md>"
    size_t afterOffset = (size_t)(afterStart - input);
    if (afterOffset > inputLen) return FALSE;
    size_t afterLen = inputLen - afterOffset;
    if (contentLen > (SIZE_MAX / sizeof(wchar_t)) - 1) return FALSE;
    wchar_t* mdContent = (wchar_t*)malloc((contentLen + 1) * sizeof(wchar_t));
    if (!mdContent) return FALSE;
    wcsncpy(mdContent, contentStart, contentLen);
    mdContent[contentLen] = L'\0';
    size_t totalLen = beforeLen + contentLen + afterLen;
    ParseState state = {0};
    state.currentPos = 0;  // Start from 0, will be updated after parsing before section
    size_t displayCapacity = 0;
    if (!MarkdownParser_CalculateDisplayBufferCapacity(totalLen, &displayCapacity)) { free(mdContent); return FALSE; }
    state.displayText = (wchar_t*)malloc(displayCapacity * sizeof(wchar_t));
    if (!state.displayText) { free(mdContent); return FALSE; }
    state.displayCapacity = displayCapacity;
    int estimatedColorTags = CountMarkdownColorTags(input);  // Count from full input
    state.colorTagCapacity = GetInitialColorTagCapacity(estimatedColorTags);
    state.colorTags = (MarkdownColorTag*)malloc(state.colorTagCapacity * sizeof(MarkdownColorTag));
    if (!state.colorTags) { CleanupParseState(&state); free(mdContent); return FALSE; }
    int estimatedFontTags = CountMarkdownFontTags(input);  // Count from full input
    state.fontTagCapacity = GetInitialFontTagCapacity(estimatedFontTags);
    state.fontTags = (MarkdownFontTag*)malloc(state.fontTagCapacity * sizeof(MarkdownFontTag));
    if (!state.fontTags) { CleanupParseState(&state); free(mdContent); return FALSE; }
    if (beforeLen > 0) {
        const wchar_t* beforeSrc = input;
        const wchar_t* beforeEnd = mdTagStart;
        wchar_t* dest = state.displayText;
        while (beforeSrc < beforeEnd) {
            if (*beforeSrc == L'<' && wcsncmp(beforeSrc, L"<color:", 7) == 0) {
                const wchar_t* closeTag = wcsstr(beforeSrc, L"</color>");
                if (closeTag && closeTag < beforeEnd) {
                    if (ExtractMarkdownColorTag(&beforeSrc, &state)) {
                        dest = state.displayText + state.currentPos;
                        continue;
                    }
                }
            }
            if (*beforeSrc == L'<' && wcsncmp(beforeSrc, L"<font:", 6) == 0) {
                const wchar_t* closeTag = wcsstr(beforeSrc, L"</font>");
                if (closeTag && closeTag < beforeEnd) {
                    if (ExtractMarkdownFontTag(&beforeSrc, &state)) {
                        dest = state.displayText + state.currentPos;
                        continue;
                    }
                }
            }
            if (!AppendMarkdownOutputChar(&state, *beforeSrc++)) {
                CleanupParseState(&state);
                free(mdContent);
                return FALSE;
            }
            SyncMarkdownOutputPointer(&state, &dest);
        }
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
    wchar_t* dest = state.displayText + state.currentPos;  // Use currentPos (after parsing before section)
    BOOL atLineStart = TRUE;
    BOOL inListItem = FALSE;
    int currentListItemIndex = -1;
    BOOL inHeading = FALSE;
    int currentHeadingIndex = -1;
    BOOL inCodeBlock = FALSE;
    while (*src) {
        if (atLineStart) {
            if (ParseCodeBlock(&src, &state, &dest, &inCodeBlock)) {
                atLineStart = TRUE;
                continue;
            }
            if (inCodeBlock) {
                if (!ParseCodeBlockContent(&src, &state, &dest)) {
                    CleanupParseState(&state);
                    free(mdContent);
                    return FALSE;
                }
                atLineStart = TRUE;
                continue;
            }
            if (ParseHorizontalRule(&src, &state, &dest)) {
                atLineStart = FALSE;
                continue;
            }
            if (ParseList(&src, &state, &dest, &inListItem, &currentListItemIndex)) {
                atLineStart = FALSE;
                continue;
            }
            if (ParseHeading(&src, &state, &dest, &inHeading, &currentHeadingIndex)) {
                atLineStart = FALSE;
                continue;
            }
            if (ParseBlockquote(&src, &state, &dest)) {
                int blockquoteIndex = state.blockquoteCount - 1;
                if (!ParseBlockquoteContent(&src, &state, &dest, blockquoteIndex)) {
                    CleanupParseState(&state);
                    free(mdContent);
                    return FALSE;
                }
                atLineStart = FALSE;
                continue;
            }
        }
        if (ProcessInlineElements(&src, &state, &dest)) {
            atLineStart = FALSE;
            continue;
        }
        if (*src == L'\\' && *(src + 1)) {
            wchar_t next = *(src + 1);
            if (next == L'*' || next == L'_' || next == L'~' || next == L'#' || next == L'>' || next == L'-' || next == L'+' || next == L'[' || next == L']' || next == L'(' || next == L')' || next == L'\\' || next == L'`' || next == L'!' || next == L'|') {
                src++;  /* Skip backslash */
                if (!AppendMarkdownOutputChar(&state, *src++)) {
                    CleanupParseState(&state);
                    free(mdContent);
                    return FALSE;
                }
                SyncMarkdownOutputPointer(&state, &dest);
                atLineStart = FALSE;
                continue;
            }
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
            if (!AppendMarkdownOutputChar(&state, *src++)) {
                CleanupParseState(&state);
                free(mdContent);
                return FALSE;
            }
            SyncMarkdownOutputPointer(&state, &dest);
            continue;
        }
        atLineStart = FALSE;
        if (!AppendMarkdownOutputChar(&state, *src++)) {
            CleanupParseState(&state);
            free(mdContent);
            return FALSE;
        }
        SyncMarkdownOutputPointer(&state, &dest);
    }
    if (afterLen > 0) {
        const wchar_t* afterSrc = afterStart;
        while (*afterSrc) {
            if (*afterSrc == L'<' && wcsncmp(afterSrc, L"<color:", 7) == 0) {
                if (ExtractMarkdownColorTag(&afterSrc, &state)) {
                    dest = state.displayText + state.currentPos;
                    continue;
                }
            }
            if (*afterSrc == L'<' && wcsncmp(afterSrc, L"<font:", 6) == 0) {
                if (ExtractMarkdownFontTag(&afterSrc, &state)) {
                    dest = state.displayText + state.currentPos;
                    continue;
                }
            }
            if (!AppendMarkdownOutputChar(&state, *afterSrc++)) {
                CleanupParseState(&state);
                free(mdContent);
                return FALSE;
            }
            SyncMarkdownOutputPointer(&state, &dest);
        }
    }
    state.displayText[state.currentPos] = L'\0';
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
    *colorTags = state.colorTags;
    *colorTagCount = state.colorTagCount;
    *fontTags = state.fontTags;
    *fontTagCount = state.fontTagCount;
    DetachParseState(&state);
    free(mdContent);  // Free temporary markdown content buffer
    return TRUE;
}
