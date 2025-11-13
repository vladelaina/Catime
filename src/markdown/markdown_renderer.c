/**
 * @file markdown_renderer.c
 * @brief Markdown text rendering and layout calculation
 */

#include "markdown/markdown_parser.h"
#include <stdlib.h>
#include <string.h>

#define TEXT_WRAP_MARGIN 10
#define LIST_ITEM_INDENT 20
#define BLOCKQUOTE_INDENT 20

/** Unified position tracking eliminates duplicate calculations */
typedef struct {
    int x;
    int y;
    int lineHeight;
    RECT bounds;
} TextLayoutContext;

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

/** Check if character can be rendered by current font */
static BOOL IsCharacterSupported(HDC hdc, wchar_t ch) {
    WORD glyphIndex;
    DWORD result = GetGlyphIndicesW(hdc, &ch, 1, &glyphIndex, GGI_MARK_NONEXISTING_GLYPHS);

    if (result == GDI_ERROR) {
        return TRUE;
    }

    return (glyphIndex != 0xFFFF);
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
    MarkdownBlockquote* blockquotes,
    int blockquoteCount,
    HFONT hOriginalFont,
    const LOGFONT* baseLf,
    int baseFontHeight,
    HFONT* hCurrentFont,
    int* lastHeadingLevel,
    int* lastStyleType,
    int* lastListItemIndex,
    int* lastBlockquoteIndex,
    COLORREF linkColor,
    COLORREF normalColor,
    BOOL renderMode
) {
    if (ch == L'\n') {
        if (*hCurrentFont) {
            SelectObject(hdc, hOriginalFont);
            DeleteObject(*hCurrentFont);
            *hCurrentFont = NULL;
        }
        *lastHeadingLevel = 0;
        *lastStyleType = STYLE_NONE;
        *lastListItemIndex = -1;
        *lastBlockquoteIndex = -1;
        AdvanceNewline(ctx);
        return;
    }

    int linkIndex = -1;
    BOOL isLink = IsCharacterInLink(links, linkCount, position, &linkIndex);

    int headingIndex = -1;
    BOOL isHeading = IsCharacterInHeading(headings, headingCount, position, &headingIndex);

    int styleIndex = -1;
    BOOL isStyled = IsCharacterInStyle(styles, styleCount, position, &styleIndex);

    int listItemIndex = -1;
    BOOL isListItem = IsCharacterInListItem(listItems, listItemCount, position, &listItemIndex);

    int blockquoteIndex = -1;
    BOOL isBlockquote = IsCharacterInBlockquote(blockquotes, blockquoteCount, position, &blockquoteIndex);

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
            case 1: currentFontHeight = (int)(baseFontHeight * 1.6); break;
            case 2: currentFontHeight = (int)(baseFontHeight * 1.4); break;
            case 3: currentFontHeight = (int)(baseFontHeight * 1.2); break;
            case 4: currentFontHeight = (int)(baseFontHeight * 1.1); break;
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
        }
    }

    if (*lastHeadingLevel != (isHeading ? headings[headingIndex].level : 0) ||
        *lastStyleType != currentStyleType) {

        if (*hCurrentFont) {
            SelectObject(hdc, hOriginalFont);
            DeleteObject(*hCurrentFont);
            *hCurrentFont = NULL;
        }

        if (isHeading || isStyled) {
            LOGFONT lf;
            memcpy(&lf, baseLf, sizeof(LOGFONT));
            lf.lfHeight = currentFontHeight;
            lf.lfWeight = currentFontWeight;
            lf.lfItalic = currentItalic;

            if (currentMonospace) {
                wcscpy(lf.lfFaceName, L"Consolas");
            }

            *hCurrentFont = CreateFontIndirect(&lf);
            SelectObject(hdc, *hCurrentFont);

            TEXTMETRIC tm;
            GetTextMetrics(hdc, &tm);
            ctx->lineHeight = tm.tmHeight;
        } else {
            SelectObject(hdc, hOriginalFont);
            TEXTMETRIC tm;
            GetTextMetrics(hdc, &tm);
            ctx->lineHeight = tm.tmHeight;
        }

        *lastHeadingLevel = isHeading ? headings[headingIndex].level : 0;
        *lastStyleType = currentStyleType;
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
        SetTextColor(hdc, textColor);
    }

    if (!IsCharacterSupported(hdc, ch)) {
        return;
    }

    SIZE charSize;
    GetTextExtentPoint32W(hdc, &ch, 1, &charSize);

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
                        MarkdownBlockquote* blockquotes, int blockquoteCount,
                        RECT drawRect, COLORREF linkColor, COLORREF normalColor) {
    if (!hdc || !displayText) return;

    TextLayoutContext ctx;
    InitTextLayout(&ctx, hdc, drawRect);

    HFONT hOriginalFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
    LOGFONT baseLf;
    GetObject(hOriginalFont, sizeof(LOGFONT), &baseLf);
    int baseFontHeight = baseLf.lfHeight;

    int textLen = wcslen(displayText);
    HFONT hCurrentFont = NULL;
    int lastHeadingLevel = 0;
    int lastStyleType = STYLE_NONE;
    int lastListItemIndex = -1;
    int lastBlockquoteIndex = -1;

    for (int i = 0; i < textLen; i++) {
        ProcessMarkdownCharacter(
            hdc, displayText[i], i, &ctx,
            links, linkCount,
            headings, headingCount,
            styles, styleCount,
            listItems, listItemCount,
            blockquotes, blockquoteCount,
            hOriginalFont, &baseLf, baseFontHeight,
            &hCurrentFont, &lastHeadingLevel, &lastStyleType,
            &lastListItemIndex, &lastBlockquoteIndex,
            linkColor, normalColor, TRUE
        );
    }

    if (hCurrentFont) {
        SelectObject(hdc, hOriginalFont);
        DeleteObject(hCurrentFont);
    }
}

int CalculateMarkdownTextHeight(HDC hdc, const wchar_t* displayText,
                                  MarkdownHeading* headings, int headingCount,
                                  MarkdownStyle* styles, int styleCount,
                                  MarkdownListItem* listItems, int listItemCount,
                                  MarkdownBlockquote* blockquotes, int blockquoteCount,
                                  RECT drawRect) {
    if (!hdc || !displayText) return 0;

    TextLayoutContext ctx;
    InitTextLayout(&ctx, hdc, drawRect);

    HFONT hOriginalFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
    LOGFONT baseLf;
    GetObject(hOriginalFont, sizeof(LOGFONT), &baseLf);
    int baseFontHeight = baseLf.lfHeight;

    int textLen = wcslen(displayText);
    HFONT hCurrentFont = NULL;
    int lastHeadingLevel = 0;
    int lastStyleType = STYLE_NONE;
    int lastListItemIndex = -1;
    int lastBlockquoteIndex = -1;

    for (int i = 0; i < textLen; i++) {
        ProcessMarkdownCharacter(
            hdc, displayText[i], i, &ctx,
            NULL, 0,
            headings, headingCount,
            styles, styleCount,
            listItems, listItemCount,
            blockquotes, blockquoteCount,
            hOriginalFont, &baseLf, baseFontHeight,
            &hCurrentFont, &lastHeadingLevel, &lastStyleType,
            &lastListItemIndex, &lastBlockquoteIndex,
            0, 0, FALSE
        );
    }

    if (hCurrentFont) {
        SelectObject(hdc, hOriginalFont);
        DeleteObject(hCurrentFont);
    }

    return ctx.y + ctx.lineHeight - drawRect.top;
}
