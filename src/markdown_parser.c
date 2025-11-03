/**
 * @file markdown_parser.c
 * @brief Markdown link parser with single-pass rendering
 */

#include "markdown_parser.h"
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>

#define INITIAL_LINK_CAPACITY 10
#define TEXT_WRAP_MARGIN 10

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
    
    if (state->displayText) {
        free(state->displayText);
        state->displayText = NULL;
    }
    
    state->linkCount = 0;
    state->linkCapacity = 0;
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

static BOOL ExtractMarkdownLink(const wchar_t** src, ParseState* state) {
    if (!src || !*src || !state) return FALSE;
    
    const wchar_t* linkTextStart = *src + 1;
    const wchar_t* linkTextEnd = wcschr(linkTextStart, L']');
    
    if (!linkTextEnd || linkTextEnd[1] != L'(') return FALSE;
    
    const wchar_t* urlStart = linkTextEnd + 2;
    const wchar_t* urlEnd = wcschr(urlStart, L')');
    
    if (!urlEnd) return FALSE;
    
    if (!EnsureLinkCapacity(state)) return FALSE;
    
    MarkdownLink* link = &state->links[state->linkCount];
    
    if (!ExtractWideString(linkTextStart, linkTextEnd, &link->linkText)) {
        return FALSE;
    }
    
    if (!ExtractWideString(urlStart, urlEnd, &link->linkUrl)) {
        free(link->linkText);
        return FALSE;
    }
    
    int textLen = linkTextEnd - linkTextStart;
    link->startPos = state->currentPos;
    link->endPos = state->currentPos + textLen;
    ZeroMemory(&link->linkRect, sizeof(RECT));
    
    wcsncpy(state->displayText + state->currentPos, link->linkText, textLen);
    state->currentPos += textLen;
    state->linkCount++;
    
    *src = urlEnd + 1;
    return TRUE;
}

/** Parse [text](url) format with pre-allocation optimization */
BOOL ParseMarkdownLinks(const wchar_t* input, wchar_t** displayText, 
                        MarkdownLink** links, int* linkCount) {
    if (!input || !displayText || !links || !linkCount) return FALSE;
    
    *displayText = NULL;
    *links = NULL;
    *linkCount = 0;
    
    size_t inputLen = wcslen(input);
    ParseState state = {0};
    
    state.displayText = (wchar_t*)malloc((inputLen + 1) * sizeof(wchar_t));
    if (!state.displayText) return FALSE;
    
    int estimatedLinks = CountMarkdownLinks(input);
    state.linkCapacity = estimatedLinks > 0 ? estimatedLinks + 2 : INITIAL_LINK_CAPACITY;
    state.links = (MarkdownLink*)malloc(state.linkCapacity * sizeof(MarkdownLink));
    
    if (!state.links) {
        CleanupParseState(&state);
        return FALSE;
    }
    
    const wchar_t* src = input;
    wchar_t* dest = state.displayText;
    
    while (*src) {
        if (*src == L'[' && ExtractMarkdownLink(&src, &state)) {
            dest = state.displayText + state.currentPos;
            continue;
        }
        
        *dest++ = *src++;
        state.currentPos++;
    }
    
    *dest = L'\0';
    
    *displayText = state.displayText;
    *links = state.links;
    *linkCount = state.linkCount;
    
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

/** Single-pass rendering calculates link rects during draw (O(n) vs O(2n)) */
void RenderMarkdownText(HDC hdc, const wchar_t* displayText, MarkdownLink* links, int linkCount, 
                        RECT drawRect, COLORREF linkColor, COLORREF normalColor) {
    if (!hdc || !displayText) return;
    
    TextLayoutContext ctx;
    InitTextLayout(&ctx, hdc, drawRect);
    
    int textLen = wcslen(displayText);
    
    for (int i = 0; i < textLen; i++) {
        wchar_t ch = displayText[i];
        
        if (ch == L'\n') {
            AdvanceNewline(&ctx);
            continue;
        }
        
        int linkIndex = -1;
        BOOL isLink = IsCharacterInLink(links, linkCount, i, &linkIndex);
        
        SetTextColor(hdc, isLink ? linkColor : normalColor);
        
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
}

BOOL HandleMarkdownClick(MarkdownLink* links, int linkCount, POINT clickPoint) {
    const wchar_t* clickedUrl = GetClickedLinkUrl(links, linkCount, clickPoint);
    if (clickedUrl) {
        ShellExecuteW(NULL, L"open", clickedUrl, NULL, NULL, SW_SHOWNORMAL);
        return TRUE;
    }
    return FALSE;
}
