/**
 * @file markdown_state.c
 * @brief Markdown parser state management and data structure operations
 */

#include "markdown/markdown_parser.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_LINK_CAPACITY 10
#define INITIAL_HEADING_CAPACITY 5
#define INITIAL_STYLE_CAPACITY 20
#define INITIAL_LIST_ITEM_CAPACITY 10
#define INITIAL_BLOCKQUOTE_CAPACITY 5
#define INITIAL_COLOR_TAG_CAPACITY 10
#define INITIAL_FONT_TAG_CAPACITY 10

/** Unified capacity management macro to eliminate code duplication */
#define ENSURE_CAPACITY(state, type, field, count_field, capacity_field) \
    do { \
        if (!state) return FALSE; \
        if ((state)->count_field < (state)->capacity_field) return TRUE; \
        int newCapacity = (state)->capacity_field * 2; \
        type* newArray = (type*)realloc((state)->field, newCapacity * sizeof(type)); \
        if (!newArray) return FALSE; \
        (state)->field = newArray; \
        (state)->capacity_field = newCapacity; \
        return TRUE; \
    } while(0)

BOOL EnsureLinkCapacity(ParseState* state) {
    ENSURE_CAPACITY(state, MarkdownLink, links, linkCount, linkCapacity);
}

BOOL EnsureHeadingCapacity(ParseState* state) {
    ENSURE_CAPACITY(state, MarkdownHeading, headings, headingCount, headingCapacity);
}

BOOL EnsureStyleCapacity(ParseState* state) {
    ENSURE_CAPACITY(state, MarkdownStyle, styles, styleCount, styleCapacity);
}

BOOL EnsureListItemCapacity(ParseState* state) {
    ENSURE_CAPACITY(state, MarkdownListItem, listItems, listItemCount, listItemCapacity);
}

BOOL EnsureBlockquoteCapacity(ParseState* state) {
    ENSURE_CAPACITY(state, MarkdownBlockquote, blockquotes, blockquoteCount, blockquoteCapacity);
}

BOOL EnsureColorTagCapacity(ParseState* state) {
    ENSURE_CAPACITY(state, MarkdownColorTag, colorTags, colorTagCount, colorTagCapacity);
}

BOOL EnsureFontTagCapacity(ParseState* state) {
    ENSURE_CAPACITY(state, MarkdownFontTag, fontTags, fontTagCount, fontTagCapacity);
}

void CleanupParseState(ParseState* state) {
    if (!state) return;

    if (state->links) {
        FreeMarkdownLinks(state->links, state->linkCount);
        state->links = NULL;
    }

    if (state->headings) {
        free(state->headings);
        state->headings = NULL;
    }

    if (state->styles) {
        free(state->styles);
        state->styles = NULL;
    }

    if (state->listItems) {
        free(state->listItems);
        state->listItems = NULL;
    }

    if (state->blockquotes) {
        free(state->blockquotes);
        state->blockquotes = NULL;
    }

    if (state->colorTags) {
        free(state->colorTags);
        state->colorTags = NULL;
    }

    if (state->fontTags) {
        free(state->fontTags);
        state->fontTags = NULL;
    }

    if (state->displayText) {
        free(state->displayText);
        state->displayText = NULL;
    }

    state->linkCount = 0;
    state->linkCapacity = 0;
    state->headingCount = 0;
    state->headingCapacity = 0;
    state->styleCount = 0;
    state->styleCapacity = 0;
    state->listItemCount = 0;
    state->listItemCapacity = 0;
    state->blockquoteCount = 0;
    state->blockquoteCapacity = 0;
    state->colorTagCount = 0;
    state->colorTagCapacity = 0;
    state->fontTagCount = 0;
    state->fontTagCapacity = 0;
    state->currentPos = 0;
}

void FreeMarkdownLinks(MarkdownLink* links, int linkCount) {
    if (!links) return;

    for (int i = 0; i < linkCount; i++) {
        if (links[i].linkText) {
            free(links[i].linkText);
            links[i].linkText = NULL;
        }
        if (links[i].linkUrl) {
            free(links[i].linkUrl);
            links[i].linkUrl = NULL;
        }
    }
    free(links);
}

/** Unified query function macro to eliminate code duplication */
#define IS_CHARACTER_IN_RANGE(array, count, position, index) \
    do { \
        if (!array) return FALSE; \
        for (int i = 0; i < count; i++) { \
            if (position >= array[i].startPos && position < array[i].endPos) { \
                if (index) *index = i; \
                return TRUE; \
            } \
        } \
        return FALSE; \
    } while(0)

BOOL IsCharacterInLink(MarkdownLink* links, int linkCount, int position, int* linkIndex) {
    IS_CHARACTER_IN_RANGE(links, linkCount, position, linkIndex);
}

BOOL IsCharacterInHeading(MarkdownHeading* headings, int headingCount, int position, int* headingIndex) {
    IS_CHARACTER_IN_RANGE(headings, headingCount, position, headingIndex);
}

BOOL IsCharacterInStyle(MarkdownStyle* styles, int styleCount, int position, int* styleIndex) {
    IS_CHARACTER_IN_RANGE(styles, styleCount, position, styleIndex);
}

BOOL IsCharacterInListItem(MarkdownListItem* listItems, int listItemCount, int position, int* listItemIndex) {
    IS_CHARACTER_IN_RANGE(listItems, listItemCount, position, listItemIndex);
}

BOOL IsCharacterInBlockquote(MarkdownBlockquote* blockquotes, int blockquoteCount, int position, int* blockquoteIndex) {
    IS_CHARACTER_IN_RANGE(blockquotes, blockquoteCount, position, blockquoteIndex);
}

BOOL IsCharacterInColorTag(MarkdownColorTag* colorTags, int colorTagCount, int position, int* colorTagIndex) {
    IS_CHARACTER_IN_RANGE(colorTags, colorTagCount, position, colorTagIndex);
}

BOOL IsCharacterInFontTag(MarkdownFontTag* fontTags, int fontTagCount, int position, int* fontTagIndex) {
    IS_CHARACTER_IN_RANGE(fontTags, fontTagCount, position, fontTagIndex);
}

COLORREF InterpolateGradientColor(const MarkdownColorTag* colorTag, int position) {
    if (!colorTag || colorTag->colorCount == 0) return RGB(0, 0, 0);
    if (colorTag->colorCount == 1) return colorTag->colors[0];
    
    int range = colorTag->endPos - colorTag->startPos;
    if (range <= 0) return colorTag->colors[0];
    
    int relPos = position - colorTag->startPos;
    if (relPos < 0) relPos = 0;
    if (relPos >= range) relPos = range - 1;
    
    /* Map position to color segment using integer math */
    int segments = colorTag->colorCount - 1;
    int scaledPos = relPos * segments;
    int segmentIndex = scaledPos / range;
    int segmentFrac = (scaledPos % range) * 256 / range;  /* 0-255 fixed point */
    
    if (segmentIndex >= segments) {
        segmentIndex = segments - 1;
        segmentFrac = 255;
    }
    
    COLORREF c1 = colorTag->colors[segmentIndex];
    COLORREF c2 = colorTag->colors[segmentIndex + 1];
    
    /* Integer interpolation: result = c1 + (c2 - c1) * frac / 256 */
    int r = GetRValue(c1) + ((GetRValue(c2) - GetRValue(c1)) * segmentFrac >> 8);
    int g = GetGValue(c1) + ((GetGValue(c2) - GetGValue(c1)) * segmentFrac >> 8);
    int b = GetBValue(c1) + ((GetBValue(c2) - GetBValue(c1)) * segmentFrac >> 8);
    
    return RGB(r, g, b);
}

const wchar_t* GetClickedLinkUrl(MarkdownLink* links, int linkCount, POINT point) {
    if (!links) return NULL;

    for (int i = 0; i < linkCount; i++) {
        if (PtInRect(&links[i].linkRect, point)) {
            return links[i].linkUrl;
        }
    }
    return NULL;
}

BOOL HandleMarkdownClick(MarkdownLink* links, int linkCount, POINT clickPoint) {
    const wchar_t* clickedUrl = GetClickedLinkUrl(links, linkCount, clickPoint);
    if (clickedUrl) {
        ShellExecuteW(NULL, L"open", clickedUrl, NULL, NULL, SW_SHOWNORMAL);
        return TRUE;
    }
    return FALSE;
}

int GetInitialLinkCapacity(int estimatedCount) {
    return estimatedCount > 0 ? estimatedCount + 2 : INITIAL_LINK_CAPACITY;
}

int GetInitialHeadingCapacity(int estimatedCount) {
    return estimatedCount > 0 ? estimatedCount + 2 : INITIAL_HEADING_CAPACITY;
}

int GetInitialStyleCapacity(int estimatedCount) {
    return estimatedCount > 0 ? estimatedCount + 2 : INITIAL_STYLE_CAPACITY;
}

int GetInitialListItemCapacity(int estimatedCount) {
    return estimatedCount > 0 ? estimatedCount + 2 : INITIAL_LIST_ITEM_CAPACITY;
}

int GetInitialBlockquoteCapacity(int estimatedCount) {
    return estimatedCount > 0 ? estimatedCount + 2 : INITIAL_BLOCKQUOTE_CAPACITY;
}

int GetInitialColorTagCapacity(int estimatedCount) {
    return estimatedCount > 0 ? estimatedCount + 2 : INITIAL_COLOR_TAG_CAPACITY;
}

int GetInitialFontTagCapacity(int estimatedCount) {
    return estimatedCount > 0 ? estimatedCount + 2 : INITIAL_FONT_TAG_CAPACITY;
}
