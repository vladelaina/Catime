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
                        MarkdownBlockquote** blockquotes, int* blockquoteCount,
                        MarkdownColorTag** colorTags, int* colorTagCount,
                        MarkdownFontTag** fontTags, int* fontTagCount) {
    if (!input || !displayText || !links || !linkCount || !headings || !headingCount ||
        !styles || !styleCount || !listItems || !listItemCount || !blockquotes || !blockquoteCount ||
        !colorTags || !colorTagCount || !fontTags || !fontTagCount) return FALSE;

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

    if (!input || wcslen(input) == 0) return FALSE;

    // Skip BOM if present (double safety)
    if (*input == 0xFEFF) {
        input++;
        if (wcslen(input) == 0) return FALSE;
    }

    // Check for <md> tag - only parse content inside tags
    const wchar_t* mdTagStart = wcsstr(input, L"<md>");
    const wchar_t* mdTagEnd = wcsstr(input, L"</md>");
    
    // Check if we have <color> or <font> tags (these work without <md>)
    BOOL hasColorTags = (wcsstr(input, L"<color:") != NULL);
    BOOL hasFontTags = (wcsstr(input, L"<font:") != NULL);
    BOOL hasRichTextTags = hasColorTags || hasFontTags;
    
    // If no <md> tags and no rich text tags, return plain text
    if (!mdTagStart || !mdTagEnd || mdTagEnd <= mdTagStart) {
        if (!hasRichTextTags) {
            size_t len = wcslen(input);
            *displayText = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
            if (!*displayText) return FALSE;
            wcscpy_s(*displayText, len + 1, input);
            return TRUE;  // Success but no markdown elements
        }
        
        // Has rich text tags but no <md> - parse only color/font tags
        size_t inputLen = wcslen(input);
        
        ParseState state = {0};
        state.currentPos = 0;
        
        state.displayText = (wchar_t*)malloc((inputLen + 1) * sizeof(wchar_t));
        if (!state.displayText) return FALSE;
        
        int estimatedColorTags = CountMarkdownColorTags(input);
        state.colorTagCapacity = GetInitialColorTagCapacity(estimatedColorTags);
        state.colorTags = (MarkdownColorTag*)malloc(state.colorTagCapacity * sizeof(MarkdownColorTag));
        if (!state.colorTags) { CleanupParseState(&state); return FALSE; }
        
        int estimatedFontTags = CountMarkdownFontTags(input);
        state.fontTagCapacity = GetInitialFontTagCapacity(estimatedFontTags);
        state.fontTags = (MarkdownFontTag*)malloc(state.fontTagCapacity * sizeof(MarkdownFontTag));
        if (!state.fontTags) { CleanupParseState(&state); return FALSE; }
        
        // Simple parsing loop for color/font tags only
        const wchar_t* src = input;
        wchar_t* dest = state.displayText;
        
        while (*src) {
            // Try color tag
            if (*src == L'<' && wcsncmp(src, L"<color:", 7) == 0) {
                if (ExtractMarkdownColorTag(&src, &state)) {
                    dest = state.displayText + state.currentPos;
                    continue;
                }
            }
            
            // Try font tag
            if (*src == L'<' && wcsncmp(src, L"<font:", 6) == 0) {
                if (ExtractMarkdownFontTag(&src, &state)) {
                    dest = state.displayText + state.currentPos;
                    continue;
                }
            }
            
            // Regular character
            *dest++ = *src++;
            state.currentPos++;
        }
        *dest = L'\0';
        
        *displayText = state.displayText;
        *colorTags = state.colorTags;
        *colorTagCount = state.colorTagCount;
        *fontTags = state.fontTags;
        *fontTagCount = state.fontTagCount;
        
        // Allocate empty arrays for other elements
        *links = NULL; *linkCount = 0;
        *headings = NULL; *headingCount = 0;
        *styles = NULL; *styleCount = 0;
        *listItems = NULL; *listItemCount = 0;
        *blockquotes = NULL; *blockquoteCount = 0;
        
        return TRUE;
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
    state.currentPos = 0;  // Start from 0, will be updated after parsing before section

    state.displayText = (wchar_t*)malloc((totalLen + wcslen(BULLET_POINT) * 200 + 1) * sizeof(wchar_t));
    if (!state.displayText) { free(mdContent); return FALSE; }
    
    // Pre-allocate color/font tag arrays early (needed for before section parsing)
    int estimatedColorTags = CountMarkdownColorTags(input);  // Count from full input
    state.colorTagCapacity = GetInitialColorTagCapacity(estimatedColorTags);
    state.colorTags = (MarkdownColorTag*)malloc(state.colorTagCapacity * sizeof(MarkdownColorTag));
    if (!state.colorTags) { CleanupParseState(&state); free(mdContent); return FALSE; }
    
    int estimatedFontTags = CountMarkdownFontTags(input);  // Count from full input
    state.fontTagCapacity = GetInitialFontTagCapacity(estimatedFontTags);
    state.fontTags = (MarkdownFontTag*)malloc(state.fontTagCapacity * sizeof(MarkdownFontTag));
    if (!state.fontTags) { CleanupParseState(&state); free(mdContent); return FALSE; }
    
    // Parse text before <md> tag (only color/font tags, no full markdown)
    if (beforeLen > 0) {
        const wchar_t* beforeSrc = input;
        const wchar_t* beforeEnd = mdTagStart;
        wchar_t* dest = state.displayText;
        
        while (beforeSrc < beforeEnd) {
            // Try color tag - but only if closing tag is also before <md>
            if (*beforeSrc == L'<' && wcsncmp(beforeSrc, L"<color:", 7) == 0) {
                const wchar_t* closeTag = wcsstr(beforeSrc, L"</color>");
                if (closeTag && closeTag < beforeEnd) {
                    if (ExtractMarkdownColorTag(&beforeSrc, &state)) {
                        dest = state.displayText + state.currentPos;
                        continue;
                    }
                }
            }
            
            // Try font tag - but only if closing tag is also before <md>
            if (*beforeSrc == L'<' && wcsncmp(beforeSrc, L"<font:", 6) == 0) {
                const wchar_t* closeTag = wcsstr(beforeSrc, L"</font>");
                if (closeTag && closeTag < beforeEnd) {
                    if (ExtractMarkdownFontTag(&beforeSrc, &state)) {
                        dest = state.displayText + state.currentPos;
                        continue;
                    }
                }
            }
            
            // Regular character
            *dest++ = *beforeSrc++;
            state.currentPos++;
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
    
    // Note: colorTags and fontTags already allocated above (before parsing before section)

    /* ========================================================================
     * Main Parsing Loop
     * ======================================================================== */
    const wchar_t* src = mdContent;
    wchar_t* dest = state.displayText + state.currentPos;  // Use currentPos (after parsing before section)
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

    // Parse text after closing tag (only color/font tags, no full markdown)
    if (afterLen > 0) {
        const wchar_t* afterSrc = afterStart;
        
        while (*afterSrc) {
            // Try color tag
            if (*afterSrc == L'<' && wcsncmp(afterSrc, L"<color:", 7) == 0) {
                if (ExtractMarkdownColorTag(&afterSrc, &state)) {
                    dest = state.displayText + state.currentPos;
                    continue;
                }
            }
            
            // Try font tag
            if (*afterSrc == L'<' && wcsncmp(afterSrc, L"<font:", 6) == 0) {
                if (ExtractMarkdownFontTag(&afterSrc, &state)) {
                    dest = state.displayText + state.currentPos;
                    continue;
                }
            }
            
            // Regular character
            *dest++ = *afterSrc++;
            state.currentPos++;
        }
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
    *colorTags = state.colorTags;
    *colorTagCount = state.colorTagCount;
    *fontTags = state.fontTags;
    *fontTagCount = state.fontTagCount;

    LOG_INFO("MD Parse Done: DisplayText len %d, Links %d, Headings %d, Styles %d, ColorTags %d, FontTags %d", 
             wcslen(state.displayText), state.linkCount, state.headingCount, state.styleCount,
             state.colorTagCount, state.fontTagCount);

    free(mdContent);  // Free temporary markdown content buffer
    return TRUE;
}
