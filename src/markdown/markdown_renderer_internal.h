#ifndef MARKDOWN_RENDERER_INTERNAL_H
#define MARKDOWN_RENDERER_INTERNAL_H
#include "markdown/markdown_parser.h"
#define TEXT_WRAP_MARGIN 10
#define LIST_ITEM_INDENT 20
#define BLOCKQUOTE_INDENT 20
#define MARKDOWN_FONT_CACHE_CAPACITY 64
typedef struct { int x; int y; int lineHeight; RECT bounds; } TextLayoutContext;
typedef struct {
    int height; int weight; BYTE italic; BOOL monospace; HFONT font;
} MarkdownFontCacheEntry;
typedef struct {
    MarkdownFontCacheEntry entries[MARKDOWN_FONT_CACHE_CAPACITY];
    int count;
} MarkdownFontCache;
typedef struct {
    int linkIndex; int headingIndex; int styleIndex;
    int listItemIndex; int blockquoteIndex;
} MarkdownRangeCursors;
int GetMarkdownRenderTextLength(const wchar_t* text);
BOOL IsHorizontalRuleMarker(const wchar_t* text, int position, int textLen);
void InitTextLayout(TextLayoutContext* ctx, HDC hdc, RECT drawRect);
void UpdateLineHeightFromCurrentFont(HDC hdc, TextLayoutContext* ctx);
void InitBaseFontState(HDC hdc, HFONT* originalFont, LOGFONT* baseFont,
                       int* baseFontHeight);
HFONT GetCachedMarkdownFont(MarkdownFontCache* cache, const LOGFONT* baseFont,
                            int height, int weight, BOOL italic, BOOL monospace);
void ReleaseMarkdownFontCache(MarkdownFontCache* cache);
void AdvanceNewline(TextLayoutContext* ctx);
void AdvanceCharacter(TextLayoutContext* ctx, int charWidth);
void ProcessMarkdownCharacter(
    HDC hdc, wchar_t ch, int position, TextLayoutContext* ctx,
    MarkdownLink* links, int linkCount, MarkdownHeading* headings,
    int headingCount, MarkdownStyle* styles, int styleCount,
    MarkdownListItem* listItems, int listItemCount,
    const MarkdownBlockquote* blockquotes, int blockquoteCount,
    MarkdownRangeCursors* cursors, HFONT originalFont,
    const LOGFONT* baseFont, int baseFontHeight, MarkdownFontCache* fontCache,
    HFONT* currentFont, int* lastHeadingLevel, int* lastStyleType,
    BOOL* lastBlockquoteFont, COLORREF* lastTextColor,
    int* lastListItemIndex, int* lastBlockquoteIndex,
    COLORREF linkColor, COLORREF normalColor, BOOL renderMode);
#endif
