/**
 * @file markdown_parser.c
 * @brief Markdown link parser with single-pass rendering
 */

#include "markdown_parser.h"
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>

/* Initial link array capacity grows dynamically if needed */
#define INITIAL_LINK_CAPACITY 10
#define INITIAL_HEADING_CAPACITY 5
#define INITIAL_STYLE_CAPACITY 20
#define INITIAL_LIST_ITEM_CAPACITY 10
#define TEXT_WRAP_MARGIN 10
#define LIST_ITEM_INDENT 20
#define BULLET_POINT L"• "

/** Unified position tracking eliminates duplicate calculations */
typedef struct {
    int x;
    int y;
    int lineHeight;
    RECT bounds;
} TextLayoutContext;

typedef struct {
    wchar_t* displayText;
    MarkdownLink* links;
    int linkCount;
    int linkCapacity;
    MarkdownHeading* headings;
    int headingCount;
    int headingCapacity;
    MarkdownStyle* styles;
    int styleCount;
    int styleCapacity;
    MarkdownListItem* listItems;
    int listItemCount;
    int listItemCapacity;
    int currentPos;
} ParseState;

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

static void CleanupParseState(ParseState* state) {
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
    state->currentPos = 0;
}

static BOOL EnsureLinkCapacity(ParseState* state) {
    if (!state) return FALSE;
    if (state->linkCount < state->linkCapacity) return TRUE;

    int newCapacity = state->linkCapacity * 2;
    MarkdownLink* newLinks = (MarkdownLink*)realloc(state->links,
                                                     newCapacity * sizeof(MarkdownLink));
    if (!newLinks) return FALSE;

    state->links = newLinks;
    state->linkCapacity = newCapacity;
    return TRUE;
}

static BOOL EnsureHeadingCapacity(ParseState* state) {
    if (!state) return FALSE;
    if (state->headingCount < state->headingCapacity) return TRUE;

    int newCapacity = state->headingCapacity * 2;
    MarkdownHeading* newHeadings = (MarkdownHeading*)realloc(state->headings,
                                                               newCapacity * sizeof(MarkdownHeading));
    if (!newHeadings) return FALSE;

    state->headings = newHeadings;
    state->headingCapacity = newCapacity;
    return TRUE;
}

static BOOL EnsureStyleCapacity(ParseState* state) {
    if (!state) return FALSE;
    if (state->styleCount < state->styleCapacity) return TRUE;

    int newCapacity = state->styleCapacity * 2;
    MarkdownStyle* newStyles = (MarkdownStyle*)realloc(state->styles,
                                                         newCapacity * sizeof(MarkdownStyle));
    if (!newStyles) return FALSE;

    state->styles = newStyles;
    state->styleCapacity = newCapacity;
    return TRUE;
}

static BOOL EnsureListItemCapacity(ParseState* state) {
    if (!state) return FALSE;
    if (state->listItemCount < state->listItemCapacity) return TRUE;

    int newCapacity = state->listItemCapacity * 2;
    MarkdownListItem* newListItems = (MarkdownListItem*)realloc(state->listItems,
                                                                  newCapacity * sizeof(MarkdownListItem));
    if (!newListItems) return FALSE;

    state->listItems = newListItems;
    state->listItemCapacity = newCapacity;
    return TRUE;
}

static void InitTextLayout(TextLayoutContext* ctx, HDC hdc, RECT drawRect) {
    if (!ctx) return;
    
    ctx->x = drawRect.left;
    ctx->y = drawRect.top;
    ctx->bounds = drawRect;
    
    if (hdc) {
        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        ctx->lineHeight = tm.tmHeight;
    } else {
        ctx->lineHeight = 0;
    }
}

static void AdvanceNewline(TextLayoutContext* ctx) {
    if (!ctx) return;
    ctx->x = ctx->bounds.left;
    ctx->y += ctx->lineHeight;
}

static void AdvanceCharacter(TextLayoutContext* ctx, int charWidth) {
    if (!ctx) return;
    ctx->x += charWidth;
    
    if (ctx->x > ctx->bounds.right - TEXT_WRAP_MARGIN) {
        AdvanceNewline(ctx);
    }
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

/** Parse [text](url) format, # headings, inline styles, and list items with pre-allocation optimization */
BOOL ParseMarkdownLinks(const wchar_t* input, wchar_t** displayText,
                        MarkdownLink** links, int* linkCount,
                        MarkdownHeading** headings, int* headingCount,
                        MarkdownStyle** styles, int* styleCount,
                        MarkdownListItem** listItems, int* listItemCount) {
    if (!input || !displayText || !links || !linkCount || !headings || !headingCount ||
        !styles || !styleCount || !listItems || !listItemCount) return FALSE;

    *displayText = NULL;
    *links = NULL;
    *linkCount = 0;
    *headings = NULL;
    *headingCount = 0;
    *styles = NULL;
    *styleCount = 0;
    *listItems = NULL;
    *listItemCount = 0;

    size_t inputLen = wcslen(input);
    ParseState state = {0};

    state.displayText = (wchar_t*)malloc((inputLen + wcslen(BULLET_POINT) * 100 + 1) * sizeof(wchar_t));
    if (!state.displayText) return FALSE;

    int estimatedLinks = CountMarkdownLinks(input);
    state.linkCapacity = estimatedLinks > 0 ? estimatedLinks + 2 : INITIAL_LINK_CAPACITY;
    state.links = (MarkdownLink*)malloc(state.linkCapacity * sizeof(MarkdownLink));

    if (!state.links) {
        CleanupParseState(&state);
        return FALSE;
    }

    int estimatedHeadings = CountMarkdownHeadings(input);
    state.headingCapacity = estimatedHeadings > 0 ? estimatedHeadings + 2 : INITIAL_HEADING_CAPACITY;
    state.headings = (MarkdownHeading*)malloc(state.headingCapacity * sizeof(MarkdownHeading));

    if (!state.headings) {
        CleanupParseState(&state);
        return FALSE;
    }

    int estimatedStyles = CountMarkdownStyles(input);
    state.styleCapacity = estimatedStyles > 0 ? estimatedStyles + 2 : INITIAL_STYLE_CAPACITY;
    state.styles = (MarkdownStyle*)malloc(state.styleCapacity * sizeof(MarkdownStyle));

    if (!state.styles) {
        CleanupParseState(&state);
        return FALSE;
    }

    int estimatedListItems = CountMarkdownListItems(input);
    state.listItemCapacity = estimatedListItems > 0 ? estimatedListItems + 2 : INITIAL_LIST_ITEM_CAPACITY;
    state.listItems = (MarkdownListItem*)malloc(state.listItemCapacity * sizeof(MarkdownListItem));

    if (!state.listItems) {
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

    return TRUE;
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

const wchar_t* GetClickedLinkUrl(MarkdownLink* links, int linkCount, POINT point) {
    if (!links) return NULL;
    
    for (int i = 0; i < linkCount; i++) {
        if (PtInRect(&links[i].linkRect, point)) {
            return links[i].linkUrl;
        }
    }
    return NULL;
}

BOOL IsCharacterInLink(MarkdownLink* links, int linkCount, int position, int* linkIndex) {
    if (!links) return FALSE;

    for (int i = 0; i < linkCount; i++) {
        if (position >= links[i].startPos && position < links[i].endPos) {
            if (linkIndex) *linkIndex = i;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL IsCharacterInHeading(MarkdownHeading* headings, int headingCount, int position, int* headingIndex) {
    if (!headings) return FALSE;

    for (int i = 0; i < headingCount; i++) {
        if (position >= headings[i].startPos && position < headings[i].endPos) {
            if (headingIndex) *headingIndex = i;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL IsCharacterInStyle(MarkdownStyle* styles, int styleCount, int position, int* styleIndex) {
    if (!styles) return FALSE;

    for (int i = 0; i < styleCount; i++) {
        if (position >= styles[i].startPos && position < styles[i].endPos) {
            if (styleIndex) *styleIndex = i;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL IsCharacterInListItem(MarkdownListItem* listItems, int listItemCount, int position, int* listItemIndex) {
    if (!listItems) return FALSE;

    for (int i = 0; i < listItemCount; i++) {
        if (position >= listItems[i].startPos && position < listItems[i].endPos) {
            if (listItemIndex) *listItemIndex = i;
            return TRUE;
        }
    }
    return FALSE;
}

/** Single-pass rendering with styles and list items (O(n)) */
void RenderMarkdownText(HDC hdc, const wchar_t* displayText,
                        MarkdownLink* links, int linkCount,
                        MarkdownHeading* headings, int headingCount,
                        MarkdownStyle* styles, int styleCount,
                        MarkdownListItem* listItems, int listItemCount,
                        RECT drawRect, COLORREF linkColor, COLORREF normalColor) {
    if (!hdc || !displayText) return;

    TextLayoutContext ctx;
    InitTextLayout(&ctx, hdc, drawRect);

    HFONT hOriginalFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
    LOGFONT lf, baseLf;
    GetObject(hOriginalFont, sizeof(LOGFONT), &lf);
    memcpy(&baseLf, &lf, sizeof(LOGFONT));
    int baseFontHeight = lf.lfHeight;

    int textLen = wcslen(displayText);
    HFONT hCurrentFont = NULL;
    int lastHeadingLevel = 0;
    int lastStyleType = STYLE_NONE;
    int lastListItemIndex = -1;

    for (int i = 0; i < textLen; i++) {
        wchar_t ch = displayText[i];

        if (ch == L'\n') {
            if (hCurrentFont) {
                SelectObject(hdc, hOriginalFont);
                DeleteObject(hCurrentFont);
                hCurrentFont = NULL;
            }
            lastHeadingLevel = 0;
            lastStyleType = STYLE_NONE;
            lastListItemIndex = -1;
            AdvanceNewline(&ctx);
            continue;
        }

        int linkIndex = -1;
        BOOL isLink = IsCharacterInLink(links, linkCount, i, &linkIndex);

        int headingIndex = -1;
        BOOL isHeading = IsCharacterInHeading(headings, headingCount, i, &headingIndex);

        int styleIndex = -1;
        BOOL isStyled = IsCharacterInStyle(styles, styleCount, i, &styleIndex);

        int listItemIndex = -1;
        BOOL isListItem = IsCharacterInListItem(listItems, listItemCount, i, &listItemIndex);

        if (isListItem && listItemIndex != lastListItemIndex) {
            if (i == listItems[listItemIndex].startPos) {
                ctx.x += LIST_ITEM_INDENT * (1 + listItems[listItemIndex].indentLevel);
            }
            lastListItemIndex = listItemIndex;
        }

        int currentFontHeight = baseFontHeight;
        int currentFontWeight = FW_NORMAL;
        BOOL currentItalic = FALSE;
        BOOL currentMonospace = FALSE;

        if (isHeading && headingIndex != -1) {
            int level = headings[headingIndex].level;
            currentFontWeight = FW_BOLD;

            switch (level) {
                case 1: currentFontHeight = (int)(baseFontHeight * 1.6); break;
                case 2: currentFontHeight = (int)(baseFontHeight * 1.4); break;
                case 3: currentFontHeight = (int)(baseFontHeight * 1.2); break;
                case 4: currentFontHeight = (int)(baseFontHeight * 1.1); break;
            }
        }

        int currentStyleType = STYLE_NONE;
        if (isStyled && styleIndex != -1) {
            currentStyleType = styles[styleIndex].type;

            switch (currentStyleType) {
                case STYLE_ITALIC:
                    currentItalic = TRUE;
                    break;
                case STYLE_BOLD:
                    currentFontWeight = FW_BOLD;
                    break;
                case STYLE_BOLD_ITALIC:
                    currentFontWeight = FW_BOLD;
                    currentItalic = TRUE;
                    break;
                case STYLE_CODE:
                    currentMonospace = TRUE;
                    break;
            }
        }

        if (lastHeadingLevel != (isHeading ? headings[headingIndex].level : 0) ||
            lastStyleType != currentStyleType) {

            if (hCurrentFont) {
                SelectObject(hdc, hOriginalFont);
                DeleteObject(hCurrentFont);
                hCurrentFont = NULL;
            }

            if (isHeading || isStyled) {
                memcpy(&lf, &baseLf, sizeof(LOGFONT));
                lf.lfHeight = currentFontHeight;
                lf.lfWeight = currentFontWeight;
                lf.lfItalic = currentItalic;

                if (currentMonospace) {
                    wcscpy(lf.lfFaceName, L"Consolas");
                }

                hCurrentFont = CreateFontIndirect(&lf);
                SelectObject(hdc, hCurrentFont);

                TEXTMETRIC tm;
                GetTextMetrics(hdc, &tm);
                ctx.lineHeight = tm.tmHeight;
            } else {
                SelectObject(hdc, hOriginalFont);
                TEXTMETRIC tm;
                GetTextMetrics(hdc, &tm);
                ctx.lineHeight = tm.tmHeight;
            }

            lastHeadingLevel = isHeading ? headings[headingIndex].level : 0;
            lastStyleType = currentStyleType;
        }

        COLORREF textColor = normalColor;
        if (isLink) {
            textColor = linkColor;
        } else if (currentStyleType == STYLE_CODE) {
            textColor = RGB(200, 0, 0);
        }
        SetTextColor(hdc, textColor);

        SIZE charSize;
        GetTextExtentPoint32W(hdc, &ch, 1, &charSize);

        if (isLink) {
            MarkdownLink* link = &links[linkIndex];

            if (i == link->startPos) {
                link->linkRect.left = ctx.x;
                link->linkRect.top = ctx.y;
                link->linkRect.bottom = ctx.y + ctx.lineHeight;
            }

            if (i == link->endPos - 1) {
                link->linkRect.right = ctx.x + charSize.cx;
            }
        }

        TextOutW(hdc, ctx.x, ctx.y, &ch, 1);

        AdvanceCharacter(&ctx, charSize.cx);
    }

    if (hCurrentFont) {
        SelectObject(hdc, hOriginalFont);
        DeleteObject(hCurrentFont);
    }
}

BOOL HandleMarkdownClick(MarkdownLink* links, int linkCount, POINT clickPoint) {
    const wchar_t* clickedUrl = GetClickedLinkUrl(links, linkCount, clickPoint);
    if (clickedUrl) {
        ShellExecuteW(NULL, L"open", clickedUrl, NULL, NULL, SW_SHOWNORMAL);
        return TRUE;
    }
    return FALSE;
}
