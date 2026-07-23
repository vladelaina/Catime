/**
 * @file markdown_renderer.c
 * @brief Markdown text rendering and layout calculation
 */

#include "markdown/markdown_parser.h"
#include "markdown_renderer_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>


int GetMarkdownRenderTextLength(const wchar_t* text) {
    if (!text) return 0;

    size_t len = wcslen(text);
    return (len > (size_t)INT_MAX) ? INT_MAX : (int)len;
}

BOOL IsHorizontalRuleMarker(const wchar_t* text, int position, int textLen) {
    if (!text || position < 0 || textLen < 3 || position > textLen - 3) {
        return FALSE;
    }

    return text[position] == L'\x2500' &&
           text[position + 1] == L'\x2500' &&
           text[position + 2] == L'\x2500';
}

void InitTextLayout(TextLayoutContext* ctx, HDC hdc, RECT drawRect) {
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

void UpdateLineHeightFromCurrentFont(HDC hdc, TextLayoutContext* ctx) {
    if (!hdc || !ctx) return;

    TEXTMETRIC tm;
    if (GetTextMetrics(hdc, &tm)) {
        ctx->lineHeight = tm.tmHeight;
    }
}

void AdvanceNewline(TextLayoutContext* ctx) {
    if (!ctx) return;
    ctx->x = ctx->bounds.left;
    ctx->y += ctx->lineHeight;
}

void AdvanceCharacter(TextLayoutContext* ctx, int charWidth) {
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
