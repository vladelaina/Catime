/**
 * @file markdown_block.c
 * @brief Block-level element parsing: headings, lists, blockquotes, code blocks, horizontal rules
 */

#include "markdown/markdown_parser.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BULLET_POINT L"• "

/* ============================================================================
 * Alert Type Data (data-driven approach to eliminate repetition)
 * ============================================================================ */

typedef struct {
    const wchar_t* name;
    int nameLen;
    BlockquoteAlertType type;
    const wchar_t* prefix;
} AlertTypeInfo;

static const AlertTypeInfo g_alertTypes[] = {
    {L"NOTE", 4, BLOCKQUOTE_NOTE, L"NOTE: "},
    {L"TIP", 3, BLOCKQUOTE_TIP, L"TIP: "},
    {L"IMPORTANT", 9, BLOCKQUOTE_IMPORTANT, L"IMPORTANT: "},
    {L"WARNING", 7, BLOCKQUOTE_WARNING, L"WARNING: "},
    {L"CAUTION", 7, BLOCKQUOTE_CAUTION, L"CAUTION: "},
};
static const int g_alertTypeCount = sizeof(g_alertTypes) / sizeof(g_alertTypes[0]);

static BOOL StartsWithLiteralW(const wchar_t* text, const wchar_t* literal) {
    if (!text || !literal) return FALSE;
    while (*literal) {
        if (*text++ != *literal++) {
            return FALSE;
        }
    }
    return TRUE;
}

/* ============================================================================
 * Block Element Parsers
 * ============================================================================ */

BOOL ParseCodeBlock(const wchar_t** src, ParseState* state, wchar_t** dest, BOOL* inCodeBlock) {
    const wchar_t* p = *src;
    UNREFERENCED_PARAMETER(state);
    UNREFERENCED_PARAMETER(dest);

    
    // Skip leading spaces
    while (*p == L' ') p++;
    
    // Check for ```
    if (!StartsWithLiteralW(p, L"```")) {
        return FALSE;
    }
    
    if (!*inCodeBlock) {
        // Start of code block
        *inCodeBlock = TRUE;
        *src = p + 3;
        // Skip language identifier
        while (**src && **src != L'\n' && **src != L'\r') (*src)++;
        if (**src == L'\r') (*src)++;
        if (**src == L'\n') (*src)++;
        return TRUE;
    } else {
        // End of code block
        *inCodeBlock = FALSE;
        *src = p + 3;
        while (**src && **src != L'\n' && **src != L'\r') (*src)++;
        return TRUE;
    }
}

BOOL ParseCodeBlockContent(const wchar_t** src, ParseState* state, wchar_t** dest) {
    if (!EnsureStyleCapacity(state)) return FALSE;
    
    MarkdownStyle* style = &state->styles[state->styleCount];
    style->startPos = state->currentPos;
    style->type = STYLE_CODE;

    while (**src && **src != L'\n' && **src != L'\r') {
        if (!AppendMarkdownOutputChar(state, *(*src)++)) {
            return FALSE;
        }
        SyncMarkdownOutputPointer(state, dest);
    }
    
    style->endPos = state->currentPos;
    state->styleCount++;
    
    // Add newline
    if (**src == L'\r') {
        if (!AppendMarkdownOutputChar(state, *(*src)++)) return FALSE;
        SyncMarkdownOutputPointer(state, dest);
    }
    if (**src == L'\n') {
        if (!AppendMarkdownOutputChar(state, *(*src)++)) return FALSE;
        SyncMarkdownOutputPointer(state, dest);
    }
    
    return TRUE;
}

BOOL ParseHorizontalRule(const wchar_t** src, ParseState* state, wchar_t** dest) {
    const wchar_t* p = *src;
    
    // Skip leading spaces
    while (*p == L' ') p++;
    
    if (*p != L'-' && *p != L'*' && *p != L'_') return FALSE;
    
    wchar_t hrChar = *p;
    const wchar_t* hrCheck = p;
    int hrCount = 0;
    
    while (*hrCheck == hrChar || *hrCheck == L' ') {
        if (*hrCheck == hrChar) hrCount++;
        hrCheck++;
    }
    
    // Must have 3+ chars and end at newline
    if (hrCount < 3 || (*hrCheck != L'\n' && *hrCheck != L'\r' && *hrCheck != L'\0')) {
        return FALSE;
    }
    
    // Insert horizontal rule marker
    *src = hrCheck;
    if (!AppendMarkdownOutputSpan(state, L"\x2500\x2500\x2500", 3)) {
        return FALSE;
    }
    SyncMarkdownOutputPointer(state, dest);
    
    return TRUE;
}

BOOL ParseList(const wchar_t** src, ParseState* state, wchar_t** dest, 
               BOOL* inListItem, int* currentListItemIndex) {
    const wchar_t* p = *src;
    int spaceCount = 0;
    
    // Count leading spaces
    while (*p == L' ') {
        spaceCount++;
        p++;
    }
    
    // Check for ordered list: digit(s) + '.' + ' '
    BOOL isOrderedList = FALSE;
    const wchar_t* olCheck = p;
    while (*olCheck >= L'0' && *olCheck <= L'9') olCheck++;
    if (olCheck > p && *olCheck == L'.' && *(olCheck + 1) == L' ') {
        isOrderedList = TRUE;
    }
    
    // Check for unordered list: -, +, or * followed by space
    BOOL isUnorderedList = (*p == L'-' || *p == L'+' || *p == L'*') && *(p + 1) == L' ';
    
    if (!isOrderedList && !isUnorderedList) return FALSE;
    
    if (!EnsureListItemCapacity(state)) return FALSE;
    
    int indentLevel = spaceCount / 2;
    
    // Copy leading spaces
    while (spaceCount > 0 && **src == L' ') {
        if (!AppendMarkdownOutputChar(state, *(*src)++)) {
            return FALSE;
        }
        SyncMarkdownOutputPointer(state, dest);
        spaceCount--;
    }
    
    MarkdownListItem* listItem = &state->listItems[state->listItemCount];
    listItem->startPos = state->currentPos;
    listItem->indentLevel = indentLevel;
    listItem->isChecked = FALSE;
    
    p = *src;
    wchar_t replacement[16] = {0};
    int advanceSrc = 0;
    
    if (isOrderedList) {
        // Extract number
        int num = 0;
        while (*p >= L'0' && *p <= L'9') {
            int digit = (int)(*p - L'0');
            if (num <= (INT_MAX - digit) / 10) {
                num = num * 10 + digit;
            } else {
                num = INT_MAX;
            }
            p++;
        }
        p += 2;  // Skip ". "
        advanceSrc = (int)(p - *src);
        _snwprintf_s(replacement, 16, _TRUNCATE, L"%d. ", num);
    } else if (StartsWithLiteralW(p, L"- [ ] ")) {
        // Unchecked task: "- [ ] "
        wcscpy_s(replacement, 16, L"\x25A1 ");
        advanceSrc = 6;
    } else if (StartsWithLiteralW(p, L"- [x] ") || StartsWithLiteralW(p, L"- [X] ")) {
        // Checked task: "- [x] "
        wcscpy_s(replacement, 16, L"\x25A0 ");
        advanceSrc = 6;
        listItem->isChecked = TRUE;  /* Mark as completed */
    } else {
        // Normal bullet
        wcscpy_s(replacement, 16, BULLET_POINT);
        advanceSrc = 2;
    }
    
    size_t replLen = wcslen(replacement);
    if (!AppendMarkdownOutputSpan(state, replacement, replLen)) {
        return FALSE;
    }
    SyncMarkdownOutputPointer(state, dest);
    *src += advanceSrc;
    
    *inListItem = TRUE;
    *currentListItemIndex = state->listItemCount;
    state->listItemCount++;
    
    return TRUE;
}

BOOL ParseHeading(const wchar_t** src, ParseState* state, wchar_t** dest,
                  BOOL* inHeading, int* currentHeadingIndex) {
    UNREFERENCED_PARAMETER(dest);
    if (**src != L'#') return FALSE;
    
    const wchar_t* hashEnd = *src;
    int level = 0;
    
    while (*hashEnd == L'#' && level < 6) {
        hashEnd++;
        level++;
    }
    
    if (*hashEnd != L' ' || level < 1 || level > 6) return FALSE;
    
    if (!EnsureHeadingCapacity(state)) return FALSE;
    
    MarkdownHeading* heading = &state->headings[state->headingCount];
    heading->level = level;
    heading->startPos = state->currentPos;
    
    *src = hashEnd + 1;
    
    *inHeading = TRUE;
    *currentHeadingIndex = state->headingCount;
    state->headingCount++;
    
    return TRUE;
}

BOOL ParseBlockquote(const wchar_t** src, ParseState* state, wchar_t** dest) {
    if (**src != L'>') return FALSE;
    
    // Count nesting level
    int nestLevel = 0;
    const wchar_t* quoteCheck = *src;
    while (*quoteCheck == L'>') {
        nestLevel++;
        quoteCheck++;
        if (*quoteCheck == L' ') quoteCheck++;
    }
    
    if (!EnsureBlockquoteCapacity(state)) return FALSE;
    
    MarkdownBlockquote* blockquote = &state->blockquotes[state->blockquoteCount];
    blockquote->startPos = state->currentPos;
    blockquote->alertType = BLOCKQUOTE_NORMAL;
    
    // Skip all '>' and spaces
    while (**src == L'>' || **src == L' ') {
        if (**src == L'>') (*src)++;
        else if (**src == L' ' && *(*src - 1) == L'>') (*src)++;
        else break;
    }
    
    BOOL isAlert = FALSE;
    
    // Check for alert syntax: [!TYPE]
    if (**src == L'[' && *(*src + 1) == L'!') {
        const wchar_t* alertStart = *src + 2;
        const wchar_t* alertEnd = alertStart;
        while (*alertEnd && *alertEnd != L']') alertEnd++;
        
        if (*alertEnd == L']') {
            int alertLen = (int)(alertEnd - alertStart);
            
            // Data-driven alert matching
            for (int i = 0; i < g_alertTypeCount; i++) {
                if (alertLen == g_alertTypes[i].nameLen && 
                    wcsncmp(alertStart, g_alertTypes[i].name, alertLen) == 0) {
                    blockquote->alertType = g_alertTypes[i].type;
                    const wchar_t* prefix = g_alertTypes[i].prefix;
                    size_t prefixLen = wcslen(prefix);
                    if (!AppendMarkdownOutputSpan(state, prefix, prefixLen)) {
                        return FALSE;
                    }
                    SyncMarkdownOutputPointer(state, dest);
                    isAlert = TRUE;

                    *src = alertEnd + 1;
                    // Add newline after title so content appears on next line
                    if (!AppendMarkdownOutputChar(state, L'\n')) {
                        return FALSE;
                    }
                    SyncMarkdownOutputPointer(state, dest);
                    
                    // Skip whitespace after [!TYPE]
                    while (**src == L' ') (*src)++;
                    if (**src == L'\n' || **src == L'\r') {
                        (*src)++;
                        if (**src == L'\n' || **src == L'\r') (*src)++;
                    }
                    // Skip next line's '> ' prefix
                    if (**src == L'>' && *(*src + 1) == L' ') {
                        *src += 2;
                    }
                    break;
                }
            }
        }
    }
    
    if (!isAlert) {
        // Show nesting level with quote markers
        for (int n = 0; n < nestLevel; n++) {
            if (!AppendMarkdownOutputChar(state, L'\x258C')) {
                return FALSE;
            }
            SyncMarkdownOutputPointer(state, dest);
        }
        if (!AppendMarkdownOutputChar(state, L' ')) {
            return FALSE;
        }
        SyncMarkdownOutputPointer(state, dest);
    }
    
    state->blockquoteCount++;
    
    return TRUE;
}

/* Process blockquote content (inline elements within blockquote) */
BOOL ParseBlockquoteContent(const wchar_t** src, ParseState* state, wchar_t** dest, int blockquoteIndex) {
    while (**src && **src != L'\n' && **src != L'\r') {
        if (!ProcessInlineElements(src, state, dest)) {
            if (!AppendMarkdownOutputChar(state, *(*src)++)) {
                return FALSE;
            }
            SyncMarkdownOutputPointer(state, dest);
        }
    }
    
    if (blockquoteIndex >= 0) {
        state->blockquotes[blockquoteIndex].endPos = state->currentPos;
    }
    return TRUE;
}
