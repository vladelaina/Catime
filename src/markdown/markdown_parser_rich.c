#include "markdown_parser_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

BOOL MarkdownParser_CalculateDisplayBufferCapacity(size_t textLen,
                                                   size_t* capacity) {
    if (!capacity || textLen > (SIZE_MAX - 1024) / 2) return FALSE;
    *capacity = textLen * 2 + 1024;
    return *capacity <= SIZE_MAX / sizeof(wchar_t);
}

BOOL ParseMarkdownRichText(const wchar_t* input, wchar_t** displayText,
                           MarkdownLink** links, int* linkCount,
                           MarkdownHeading** headings, int* headingCount,
                           MarkdownStyle** styles, int* styleCount,
                           MarkdownListItem** listItems, int* listItemCount,
                           MarkdownBlockquote** blockquotes, int* blockquoteCount,
                           MarkdownColorTag** colorTags, int* colorTagCount,
                           MarkdownFontTag** fontTags, int* fontTagCount) {
    size_t inputLen = wcslen(input);
    ParseState state = {0};
    size_t displayCapacity = 0;
    if (!MarkdownParser_CalculateDisplayBufferCapacity(inputLen,
                                                        &displayCapacity))
        return FALSE;
    state.displayText = (wchar_t*)malloc(displayCapacity * sizeof(wchar_t));
    if (!state.displayText) return FALSE;
    state.displayCapacity = displayCapacity;
    state.colorTagCapacity = GetInitialColorTagCapacity(CountMarkdownColorTags(input));
    state.colorTags = (MarkdownColorTag*)malloc(state.colorTagCapacity * sizeof(MarkdownColorTag));
    if (!state.colorTags) { CleanupParseState(&state); return FALSE; }
    state.fontTagCapacity = GetInitialFontTagCapacity(CountMarkdownFontTags(input));
    state.fontTags = (MarkdownFontTag*)malloc(state.fontTagCapacity * sizeof(MarkdownFontTag));
    if (!state.fontTags) { CleanupParseState(&state); return FALSE; }
    const wchar_t* src = input;
    wchar_t* dest = state.displayText;
    while (*src) {
        if (*src == L'<' && wcsncmp(src, L"<color:", 7) == 0 &&
            ExtractMarkdownColorTag(&src, &state)) {
            dest = state.displayText + state.currentPos;
            continue;
        }
        if (*src == L'<' && wcsncmp(src, L"<font:", 6) == 0 &&
            ExtractMarkdownFontTag(&src, &state)) {
            dest = state.displayText + state.currentPos;
            continue;
        }
        if (!AppendMarkdownOutputChar(&state, *src++)) {
            CleanupParseState(&state);
            return FALSE;
        }
        SyncMarkdownOutputPointer(&state, &dest);
    }
    state.displayText[state.currentPos] = L'\0';
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
    return TRUE;
}
