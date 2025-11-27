/**
 * @file markdown_parser.c
 * @brief Markdown parser main entry point and coordinator
 * 
 * This file handles:
 * - <md> tag detection and content extraction
 * - State initialization and memory allocation
 * - Main parsing loop coordination
 * - Result assembly and output
 * 
 * Block elements are parsed in markdown_block.c
 * Inline elements are parsed in markdown_inline.c
 * State management is in markdown_state.c
 */

#include "markdown/markdown_parser.h"
#include <stdlib.h>
#include <string.h>
#include "log.h"

#define BULLET_POINT L"• "

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

    // Check for <md> tag - only parse content inside tags
    const wchar_t* mdTagStart = wcsstr(input, L"<md>");
    const wchar_t* mdTagEnd = wcsstr(input, L"</md>");
    
    // If no <md> tags, return plain text without any markdown parsing
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
    const wchar_t* contentStart = mdTagStart + 4;  // Skip "<md>"
    
    // Only skip newline after <md> if tag is at line start
    // (preceded by newline or at very beginning)
    BOOL tagAtLineStart = (beforeLen == 0) || 
                          (input[beforeLen - 1] == L'\n') || 
                          (input[beforeLen - 1] == L'\r');
    if (tagAtLineStart) {
        if (*contentStart == L'\r') contentStart++;
        if (*contentStart == L'\n') contentStart++;
    }
    
    size_t contentLen = mdTagEnd - contentStart;
    
    // Strip newline before </md> tag (remove the tag line)
    while (contentLen > 0 && (contentStart[contentLen - 1] == L'\n' || contentStart[contentLen - 1] == L'\r')) {
        contentLen--;
    }
    
    const wchar_t* afterStart = mdTagEnd + 5;  // Skip "</md>"
    // Don't skip newlines - preserve user's line breaks after </md>
    size_t afterLen = wcslen(afterStart);
    
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

    /* ========================================================================
     * Main Parsing Loop
     * ======================================================================== */
    const wchar_t* src = mdContent;
    wchar_t* dest = state.displayText + beforeLen;
    BOOL atLineStart = TRUE;
    BOOL inListItem = FALSE;
    int currentListItemIndex = -1;
    BOOL inHeading = FALSE;
    int currentHeadingIndex = -1;
    BOOL inCodeBlock = FALSE;
    
    while (*src) {
        /* Block-level elements (only at line start) */
        if (atLineStart) {
            /* Code block fence */
            if (ParseCodeBlock(&src, &state, &dest, &inCodeBlock)) {
                atLineStart = TRUE;
                continue;
            }
            
            /* Inside code block - preserve as-is */
            if (inCodeBlock) {
                if (!ParseCodeBlockContent(&src, &state, &dest)) {
                    CleanupParseState(&state);
                    free(mdContent);
                    return FALSE;
                }
                atLineStart = TRUE;
                continue;
            }
            
            /* Horizontal rule */
            if (ParseHorizontalRule(&src, &state, &dest)) {
                atLineStart = FALSE;
                continue;
            }
            
            /* List item */
            if (ParseList(&src, &state, &dest, &inListItem, &currentListItemIndex)) {
                atLineStart = FALSE;
                continue;
            }
            
            /* Heading */
            if (ParseHeading(&src, &state, &dest, &inHeading, &currentHeadingIndex)) {
                atLineStart = FALSE;
                continue;
            }
            
            /* Blockquote */
            if (ParseBlockquote(&src, &state, &dest)) {
                int blockquoteIndex = state.blockquoteCount - 1;
                ParseBlockquoteContent(&src, &state, &dest, blockquoteIndex);
                atLineStart = FALSE;
                continue;
            }
        }
        
        /* Inline elements */
        if (ProcessInlineElements(&src, &state, &dest)) {
            atLineStart = FALSE;
            continue;
        }
        
        /* Escape character handling - \* \_ \~ etc. */
        if (*src == L'\\' && *(src + 1)) {
            wchar_t next = *(src + 1);
            /* Check if next char is a special markdown character */
            if (next == L'*' || next == L'_' || next == L'~' || next == L'#' ||
                next == L'>' || next == L'-' || next == L'+' || next == L'[' ||
                next == L']' || next == L'(' || next == L')' || next == L'\\' ||
                next == L'`' || next == L'!' || next == L'|') {
                src++;  /* Skip backslash */
                *dest++ = *src++;  /* Copy the escaped character */
                state.currentPos++;
                atLineStart = FALSE;
                continue;
            }
        }
        
        /* Line break handling */
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
        
        /* Regular character */
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
