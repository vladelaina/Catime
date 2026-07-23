 #include "markdown_renderer_internal.h"
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
void ProcessMarkdownCharacter(
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
