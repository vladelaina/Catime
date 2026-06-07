/**
 * @file markdown_renderer.c
 * @brief Markdown text rendering and layout calculation
 */

#include "markdown/markdown_parser.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_WRAP_MARGIN 10
#define LIST_ITEM_INDENT 20
#define BLOCKQUOTE_INDENT 20
#define MARKDOWN_FONT_CACHE_CAPACITY 64

/** Unified position tracking eliminates duplicate calculations */
typedef struct {
    int x;
    int y;
    int lineHeight;
    RECT bounds;
} TextLayoutContext;

typedef struct {
    int height;
    int weight;
    BYTE italic;
    BOOL monospace;
    HFONT font;
} MarkdownFontCacheEntry;

typedef struct {
    MarkdownFontCacheEntry entries[MARKDOWN_FONT_CACHE_CAPACITY];
    int count;
} MarkdownFontCache;

typedef struct {
    int linkIndex;
    int headingIndex;
    int styleIndex;
    int listItemIndex;
    int blockquoteIndex;
} MarkdownRangeCursors;

#define DEFINE_MARKDOWN_CURSOR_LOOKUP(name, type) \
    static BOOL name(const type* ranges, int count, int position, int* cursor, int* outIndex) { \
        if (outIndex) *outIndex = -1; \
        if (!ranges || count <= 0 || !cursor) return FALSE; \
        while (*cursor < count && position >= ranges[*cursor].endPos) { \
            (*cursor)++; \
        } \
        if (*cursor < count && \
            position >= ranges[*cursor].startPos && \
            position < ranges[*cursor].endPos) { \
            if (outIndex) *outIndex = *cursor; \
            return TRUE; \
        } \
        return FALSE; \
    }

DEFINE_MARKDOWN_CURSOR_LOOKUP(FindLinkAtCursor, MarkdownLink)
DEFINE_MARKDOWN_CURSOR_LOOKUP(FindHeadingAtCursor, MarkdownHeading)
DEFINE_MARKDOWN_CURSOR_LOOKUP(FindStyleAtCursor, MarkdownStyle)
DEFINE_MARKDOWN_CURSOR_LOOKUP(FindListItemAtCursor, MarkdownListItem)
DEFINE_MARKDOWN_CURSOR_LOOKUP(FindBlockquoteAtCursor, MarkdownBlockquote)

static int GetMarkdownRenderTextLength(const wchar_t* text) {
    if (!text) return 0;

    size_t len = wcslen(text);
    return (len > (size_t)INT_MAX) ? INT_MAX : (int)len;
}

static BOOL IsHorizontalRuleMarker(const wchar_t* text, int position, int textLen) {
    if (!text || position < 0 || textLen < 3 || position > textLen - 3) {
        return FALSE;
    }

    return text[position] == L'\x2500' &&
           text[position + 1] == L'\x2500' &&
           text[position + 2] == L'\x2500';
}

static void InitTextLayout(TextLayoutContext* ctx, HDC hdc, RECT drawRect) {
    if (!ctx) return;

    ctx->x = drawRect.left;
    ctx->y = drawRect.top;
    ctx->bounds = drawRect;

    if (hdc) {
        TEXTMETRIC tm;
        if (GetTextMetrics(hdc, &tm)) {
            ctx->lineHeight = tm.tmHeight;
        } else {
            ctx->lineHeight = 0;
        }
    } else {
        ctx->lineHeight = 0;
    }
}

static void UpdateLineHeightFromCurrentFont(HDC hdc, TextLayoutContext* ctx) {
    if (!hdc || !ctx) return;

    TEXTMETRIC tm;
    if (GetTextMetrics(hdc, &tm)) {
        ctx->lineHeight = tm.tmHeight;
    }
}

static void InitBaseFontState(HDC hdc, HFONT* hOriginalFont, LOGFONT* baseLf, int* baseFontHeight) {
    if (hOriginalFont) {
        *hOriginalFont = NULL;
    }
    if (baseLf) {
        memset(baseLf, 0, sizeof(*baseLf));
    }
    if (baseFontHeight) {
        *baseFontHeight = 16;
    }
    if (!hdc || !baseLf || !baseFontHeight) return;

    TEXTMETRIC tm;
    if (GetTextMetrics(hdc, &tm) && tm.tmHeight > 0) {
        *baseFontHeight = tm.tmHeight;
    }

    HFONT hCurrentFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
    if (hOriginalFont) {
        *hOriginalFont = hCurrentFont;
    }

    if (hCurrentFont && GetObject(hCurrentFont, sizeof(*baseLf), baseLf) == sizeof(*baseLf)) {
        if (baseLf->lfHeight != 0) {
            *baseFontHeight = baseLf->lfHeight;
        }
        return;
    }

    HFONT hDefaultFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (hDefaultFont && GetObject(hDefaultFont, sizeof(*baseLf), baseLf) == sizeof(*baseLf)) {
        if (baseLf->lfHeight != 0) {
            *baseFontHeight = baseLf->lfHeight;
        }
        return;
    }

    baseLf->lfHeight = *baseFontHeight;
    baseLf->lfWeight = FW_NORMAL;
    wcscpy_s(baseLf->lfFaceName, LF_FACESIZE, L"Segoe UI");
}

static HFONT GetCachedMarkdownFont(MarkdownFontCache* cache, const LOGFONT* baseLf,
                                   int height, int weight, BOOL italic, BOOL monospace) {
    if (!cache || !baseLf) return NULL;

    BYTE italicByte = (BYTE)(italic ? 1 : 0);

    for (int i = 0; i < cache->count; i++) {
        MarkdownFontCacheEntry* entry = &cache->entries[i];
        if (entry->height == height &&
            entry->weight == weight &&
            entry->italic == italicByte &&
            entry->monospace == monospace) {
            return entry->font;
        }
    }

    if (cache->count >= MARKDOWN_FONT_CACHE_CAPACITY) {
        return NULL;
    }

    LOGFONT lf;
    memcpy(&lf, baseLf, sizeof(LOGFONT));
    lf.lfHeight = height;
    lf.lfWeight = weight;
    lf.lfItalic = italicByte;

    if (monospace) {
        wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Consolas");
    }

    HFONT font = CreateFontIndirect(&lf);
    if (!font) {
        return NULL;
    }

    MarkdownFontCacheEntry* entry = &cache->entries[cache->count++];
    entry->height = height;
    entry->weight = weight;
    entry->italic = italicByte;
    entry->monospace = monospace;
    entry->font = font;
    return font;
}

static void ReleaseMarkdownFontCache(MarkdownFontCache* cache) {
    if (!cache) return;

    for (int i = 0; i < cache->count; i++) {
        if (cache->entries[i].font) {
            DeleteObject(cache->entries[i].font);
            cache->entries[i].font = NULL;
        }
    }
    cache->count = 0;
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


/**
 * @brief Process single character with unified font/style logic
 * @param renderMode TRUE=render and update link rects, FALSE=calculate only
 */
static void ProcessMarkdownCharacter(
    HDC hdc,
    wchar_t ch,
    int position,
    TextLayoutContext* ctx,
    MarkdownLink* links,
    int linkCount,
    MarkdownHeading* headings,
    int headingCount,
    MarkdownStyle* styles,
    int styleCount,
    MarkdownListItem* listItems,
    int listItemCount,
    const MarkdownBlockquote* blockquotes,
    int blockquoteCount,
    MarkdownRangeCursors* cursors,
    HFONT hOriginalFont,
    const LOGFONT* baseLf,
    int baseFontHeight,
    MarkdownFontCache* fontCache,
    HFONT* hCurrentFont,
    int* lastHeadingLevel,
    int* lastStyleType,
    BOOL* lastBlockquoteFont,
    COLORREF* lastTextColor,
    int* lastListItemIndex,
    int* lastBlockquoteIndex,
    COLORREF linkColor,
    COLORREF normalColor,
    BOOL renderMode
) {
    if (ch == L'\n') {
        if (*hCurrentFont) {
            if (hOriginalFont) {
                SelectObject(hdc, hOriginalFont);
            }
            *hCurrentFont = NULL;
            UpdateLineHeightFromCurrentFont(hdc, ctx);
        }
        *lastHeadingLevel = 0;
        *lastStyleType = STYLE_NONE;
        *lastBlockquoteFont = FALSE;
        *lastListItemIndex = -1;
        *lastBlockquoteIndex = -1;
        AdvanceNewline(ctx);
        return;
    }

    int linkIndex = -1;
    BOOL isLink = FindLinkAtCursor(links, linkCount, position,
                                   &cursors->linkIndex, &linkIndex);

    int headingIndex = -1;
    BOOL isHeading = FindHeadingAtCursor(headings, headingCount, position,
                                         &cursors->headingIndex, &headingIndex);

    int styleIndex = -1;
    BOOL isStyled = FindStyleAtCursor(styles, styleCount, position,
                                      &cursors->styleIndex, &styleIndex);

    int listItemIndex = -1;
    BOOL isListItem = FindListItemAtCursor(listItems, listItemCount, position,
                                           &cursors->listItemIndex, &listItemIndex);

    int blockquoteIndex = -1;
    BOOL isBlockquote = FindBlockquoteAtCursor(blockquotes, blockquoteCount, position,
                                               &cursors->blockquoteIndex, &blockquoteIndex);

    if (isListItem && listItemIndex != *lastListItemIndex) {
        if (position == listItems[listItemIndex].startPos) {
            ctx->x += LIST_ITEM_INDENT * (1 + listItems[listItemIndex].indentLevel);
        }
        *lastListItemIndex = listItemIndex;
    }

    if (isBlockquote && blockquoteIndex != *lastBlockquoteIndex) {
        if (position == blockquotes[blockquoteIndex].startPos) {
            ctx->x += BLOCKQUOTE_INDENT;
        }
        *lastBlockquoteIndex = blockquoteIndex;
    }

    int currentFontHeight = baseFontHeight;
    int currentFontWeight = FW_NORMAL;
    BOOL currentItalic = FALSE;
    BOOL currentMonospace = FALSE;

    if (isHeading && headingIndex != -1) {
        int level = headings[headingIndex].level;
        currentFontWeight = FW_BOLD;

        switch (level) {
            case 1: currentFontHeight = (int)(baseFontHeight * 1.5); break;
            case 2: currentFontHeight = (int)(baseFontHeight * 1.35); break;
            case 3: currentFontHeight = (int)(baseFontHeight * 1.2); break;
            case 4: currentFontHeight = (int)(baseFontHeight * 1.1); break;
            case 5: currentFontHeight = (int)(baseFontHeight * 1.0); break;
            case 6: currentFontHeight = (int)(baseFontHeight * 0.9); break;
        }
    }

    if (isBlockquote) {
        currentItalic = TRUE;
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
            default:
                break;
        }
    }

    int currentHeadingLevel = isHeading ? headings[headingIndex].level : 0;
    BOOL currentBlockquoteFont = isBlockquote ? TRUE : FALSE;

    if (*lastHeadingLevel != currentHeadingLevel ||
        *lastStyleType != currentStyleType ||
        *lastBlockquoteFont != currentBlockquoteFont) {

        if (hOriginalFont && (isHeading || isStyled || isBlockquote)) {
            HFONT hNewFont = GetCachedMarkdownFont(fontCache, baseLf, currentFontHeight,
                                                   currentFontWeight, currentItalic,
                                                   currentMonospace);
            if (hNewFont) {
                if (SelectObject(hdc, hNewFont)) {
                    *hCurrentFont = hNewFont;
                } else {
                    SelectObject(hdc, hOriginalFont);
                    *hCurrentFont = NULL;
                }
            } else {
                SelectObject(hdc, hOriginalFont);
                *hCurrentFont = NULL;
            }
            UpdateLineHeightFromCurrentFont(hdc, ctx);
        } else {
            if (hOriginalFont) {
                SelectObject(hdc, hOriginalFont);
            }
            UpdateLineHeightFromCurrentFont(hdc, ctx);
        }

        *lastHeadingLevel = currentHeadingLevel;
        *lastStyleType = currentStyleType;
        *lastBlockquoteFont = currentBlockquoteFont;
    }

    if (renderMode) {
        COLORREF textColor = normalColor;
        if (isLink) {
            textColor = linkColor;
        } else if (currentStyleType == STYLE_CODE) {
            textColor = RGB(200, 0, 0);
        } else if (isBlockquote && blockquoteIndex != -1) {
            switch (blockquotes[blockquoteIndex].alertType) {
                case BLOCKQUOTE_NOTE:
                    textColor = RGB(31, 111, 235);
                    break;
                case BLOCKQUOTE_TIP:
                    textColor = RGB(26, 127, 55);
                    break;
                case BLOCKQUOTE_IMPORTANT:
                    textColor = RGB(130, 80, 223);
                    break;
                case BLOCKQUOTE_WARNING:
                    textColor = RGB(154, 103, 0);
                    break;
                case BLOCKQUOTE_CAUTION:
                    textColor = RGB(207, 34, 46);
                    break;
                default:
                    textColor = RGB(100, 100, 100);
                    break;
            }
        }
        if (!lastTextColor || *lastTextColor != textColor) {
            SetTextColor(hdc, textColor);
            if (lastTextColor) {
                *lastTextColor = textColor;
            }
        }
    }

    SIZE charSize = {0};
    if (!GetTextExtentPoint32W(hdc, &ch, 1, &charSize)) {
        TEXTMETRIC tm;
        if (GetTextMetrics(hdc, &tm)) {
            charSize.cx = tm.tmAveCharWidth;
            charSize.cy = tm.tmHeight;
        }
    }

    if (renderMode && isLink) {
        MarkdownLink* link = &links[linkIndex];

        if (position == link->startPos) {
            link->linkRect.left = ctx->x;
            link->linkRect.top = ctx->y;
            link->linkRect.right = ctx->x;
            link->linkRect.bottom = ctx->y;
        }

        if (ctx->x < link->linkRect.left) link->linkRect.left = ctx->x;
        if (ctx->x + charSize.cx > link->linkRect.right) link->linkRect.right = ctx->x + charSize.cx;
        if (ctx->y < link->linkRect.top) link->linkRect.top = ctx->y;
        if (ctx->y + ctx->lineHeight > link->linkRect.bottom) link->linkRect.bottom = ctx->y + ctx->lineHeight;
    }

    if (renderMode) {
        TextOutW(hdc, ctx->x, ctx->y, &ch, 1);
    }

    AdvanceCharacter(ctx, charSize.cx);
}

void RenderMarkdownText(HDC hdc, const wchar_t* displayText,
                        MarkdownLink* links, int linkCount,
                        MarkdownHeading* headings, int headingCount,
                        MarkdownStyle* styles, int styleCount,
                        MarkdownListItem* listItems, int listItemCount,
                        const MarkdownBlockquote* blockquotes, int blockquoteCount,
                        RECT drawRect, COLORREF linkColor, COLORREF normalColor) {
    if (!hdc || !displayText) return;

    COLORREF originalTextColor = GetTextColor(hdc);
    TextLayoutContext ctx;
    InitTextLayout(&ctx, hdc, drawRect);

    HFONT hOriginalFont = NULL;
    LOGFONT baseLf;
    int baseFontHeight = 0;
    InitBaseFontState(hdc, &hOriginalFont, &baseLf, &baseFontHeight);

    int textLen = GetMarkdownRenderTextLength(displayText);
    MarkdownRangeCursors cursors = {0};
    MarkdownFontCache fontCache = {0};
    HFONT hCurrentFont = NULL;
    int lastHeadingLevel = 0;
    int lastStyleType = STYLE_NONE;
    BOOL lastBlockquoteFont = FALSE;
    COLORREF lastTextColor = CLR_INVALID;
    int lastListItemIndex = -1;
    int lastBlockquoteIndex = -1;

    for (int i = 0; i < textLen; i++) {
        /* Check for horizontal rule marker (─── = \x2500\x2500\x2500) */
        if (IsHorizontalRuleMarker(displayText, i, textLen)) {
            /* Draw horizontal line across full width */
            int lineY = ctx.y + ctx.lineHeight / 2;
            HGDIOBJ hPen = GetStockObject(DC_PEN);
            if (hPen) {
                COLORREF oldPenColor = SetDCPenColor(hdc, normalColor);
                HGDIOBJ hOldPen = SelectObject(hdc, hPen);
                MoveToEx(hdc, drawRect.left, lineY, NULL);
                LineTo(hdc, drawRect.right, lineY);
                if (hOldPen) {
                    SelectObject(hdc, hOldPen);
                }
                if (oldPenColor != CLR_INVALID) {
                    SetDCPenColor(hdc, oldPenColor);
                }
            }
            
            /* Move to next line */
            ctx.y += ctx.lineHeight;
            ctx.x = ctx.bounds.left;
            i += 2;  /* Skip the other two marker chars */
            continue;
        }
        
        ProcessMarkdownCharacter(
            hdc, displayText[i], i, &ctx,
            links, linkCount,
            headings, headingCount,
            styles, styleCount,
            listItems, listItemCount,
            blockquotes, blockquoteCount,
            &cursors,
            hOriginalFont, &baseLf, baseFontHeight,
            &fontCache, &hCurrentFont, &lastHeadingLevel, &lastStyleType,
            &lastBlockquoteFont, &lastTextColor,
            &lastListItemIndex, &lastBlockquoteIndex,
            linkColor, normalColor, TRUE
        );
    }

    if (hCurrentFont) {
        if (hOriginalFont) {
            SelectObject(hdc, hOriginalFont);
        }
    }
    if (originalTextColor != CLR_INVALID) {
        SetTextColor(hdc, originalTextColor);
    }
    ReleaseMarkdownFontCache(&fontCache);
}

int CalculateMarkdownTextHeight(HDC hdc, const wchar_t* displayText,
                                  MarkdownHeading* headings, int headingCount,
                                  MarkdownStyle* styles, int styleCount,
                                  MarkdownListItem* listItems, int listItemCount,
                                  const MarkdownBlockquote* blockquotes, int blockquoteCount,
                                  RECT drawRect) {
    if (!hdc || !displayText) return 0;

    TextLayoutContext ctx;
    InitTextLayout(&ctx, hdc, drawRect);

    HFONT hOriginalFont = NULL;
    LOGFONT baseLf;
    int baseFontHeight = 0;
    InitBaseFontState(hdc, &hOriginalFont, &baseLf, &baseFontHeight);

    int textLen = GetMarkdownRenderTextLength(displayText);
    MarkdownRangeCursors cursors = {0};
    MarkdownFontCache fontCache = {0};
    HFONT hCurrentFont = NULL;
    int lastHeadingLevel = 0;
    int lastStyleType = STYLE_NONE;
    BOOL lastBlockquoteFont = FALSE;
    COLORREF lastTextColor = CLR_INVALID;
    int lastListItemIndex = -1;
    int lastBlockquoteIndex = -1;

    for (int i = 0; i < textLen; i++) {
        /* Check for horizontal rule marker */
        if (IsHorizontalRuleMarker(displayText, i, textLen)) {
            ctx.y += ctx.lineHeight;
            ctx.x = ctx.bounds.left;
            i += 2;
            continue;
        }
        
        ProcessMarkdownCharacter(
            hdc, displayText[i], i, &ctx,
            NULL, 0,
            headings, headingCount,
            styles, styleCount,
            listItems, listItemCount,
            blockquotes, blockquoteCount,
            &cursors,
            hOriginalFont, &baseLf, baseFontHeight,
            &fontCache, &hCurrentFont, &lastHeadingLevel, &lastStyleType,
            &lastBlockquoteFont, &lastTextColor,
            &lastListItemIndex, &lastBlockquoteIndex,
            0, 0, FALSE
        );
    }

    if (hCurrentFont) {
        if (hOriginalFont) {
            SelectObject(hdc, hOriginalFont);
        }
    }
    ReleaseMarkdownFontCache(&fontCache);

    return ctx.y + ctx.lineHeight - drawRect.top;
}
