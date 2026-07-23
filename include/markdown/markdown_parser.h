#ifndef MARKDOWN_PARSER_H
#define MARKDOWN_PARSER_H
#include <stddef.h>
#include <windows.h>
typedef struct {
    wchar_t* linkText;
    wchar_t* linkUrl;
    RECT linkRect;
    int startPos;
    int endPos;
} MarkdownLink;
typedef struct {
    int level;
    int startPos;
    int endPos;
} MarkdownHeading;
typedef enum {
    STYLE_NONE = 0,
    STYLE_ITALIC = 1,
    STYLE_BOLD = 2,
    STYLE_BOLD_ITALIC = 3,
    STYLE_CODE = 4,
    STYLE_STRIKETHROUGH = 5
} MarkdownStyleType;
typedef struct {
    MarkdownStyleType type;
    int startPos;
    int endPos;
} MarkdownStyle;
typedef struct {
    int startPos;
    int endPos;
    int indentLevel;
    BOOL isChecked;  /* TRUE if this is a completed todo (- [x]) */
} MarkdownListItem;
typedef enum {
    BLOCKQUOTE_NORMAL = 0,
    BLOCKQUOTE_NOTE = 1,
    BLOCKQUOTE_TIP = 2,
    BLOCKQUOTE_IMPORTANT = 3,
    BLOCKQUOTE_WARNING = 4,
    BLOCKQUOTE_CAUTION = 5
} BlockquoteAlertType;
typedef struct {
    int startPos;
    int endPos;
    BlockquoteAlertType alertType;
} MarkdownBlockquote;
#define MAX_COLOR_TAG_COLORS 8
typedef struct {
    int startPos;
    int endPos;
    COLORREF colors[MAX_COLOR_TAG_COLORS];
    int colorCount;
} MarkdownColorTag;
#define MAX_FONT_NAME_LENGTH MAX_PATH
typedef struct {
    int startPos;
    int endPos;
    wchar_t fontName[MAX_FONT_NAME_LENGTH];
} MarkdownFontTag;
typedef struct {
    wchar_t* displayText;
    size_t displayCapacity;
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
    MarkdownBlockquote* blockquotes;
    int blockquoteCount;
    int blockquoteCapacity;
    MarkdownColorTag* colorTags;
    int colorTagCount;
    int colorTagCapacity;
    MarkdownFontTag* fontTags;
    int fontTagCount;
    int fontTagCapacity;
    int currentPos;
} ParseState;
BOOL ParseMarkdownLinks(const wchar_t* input, wchar_t** displayText,
                        MarkdownLink** links, int* linkCount,
                        MarkdownHeading** headings, int* headingCount,
                        MarkdownStyle** styles, int* styleCount,
                        MarkdownListItem** listItems, int* listItemCount,
                        MarkdownBlockquote** blockquotes, int* blockquoteCount,
                        MarkdownColorTag** colorTags, int* colorTagCount,
                        MarkdownFontTag** fontTags, int* fontTagCount);
void FreeMarkdownLinks(MarkdownLink* links, int linkCount);
const wchar_t* GetClickedLinkUrl(MarkdownLink* links, int linkCount, POINT point);
BOOL IsCharacterInLink(const MarkdownLink* links, int linkCount, int position, int* linkIndex);
BOOL IsCharacterInHeading(const MarkdownHeading* headings, int headingCount, int position, int* headingIndex);
BOOL IsCharacterInStyle(const MarkdownStyle* styles, int styleCount, int position, int* styleIndex);
BOOL IsCharacterInListItem(const MarkdownListItem* listItems, int listItemCount, int position, int* listItemIndex);
BOOL IsCharacterInBlockquote(const MarkdownBlockquote* blockquotes, int blockquoteCount, int position, int* blockquoteIndex);
BOOL IsCharacterInColorTag(const MarkdownColorTag* colorTags, int colorTagCount, int position, int* colorTagIndex);
BOOL IsCharacterInFontTag(const MarkdownFontTag* fontTags, int fontTagCount, int position, int* fontTagIndex);
COLORREF InterpolateGradientColor(const MarkdownColorTag* colorTag, int position);
void RenderMarkdownText(HDC hdc, const wchar_t* displayText,
                        MarkdownLink* links, int linkCount,
                        MarkdownHeading* headings, int headingCount,
                        MarkdownStyle* styles, int styleCount,
                        MarkdownListItem* listItems, int listItemCount,
                        const MarkdownBlockquote* blockquotes, int blockquoteCount,
                        RECT drawRect, COLORREF linkColor, COLORREF normalColor);
int CalculateMarkdownTextHeight(HDC hdc, const wchar_t* displayText,
                                  MarkdownHeading* headings, int headingCount,
                                  MarkdownStyle* styles, int styleCount,
                                  MarkdownListItem* listItems, int listItemCount,
                                  const MarkdownBlockquote* blockquotes, int blockquoteCount,
                                  RECT drawRect);
BOOL HandleMarkdownClick(MarkdownLink* links, int linkCount, POINT clickPoint);
#define MARKDOWN_DEFAULT_LINK_COLOR RGB(0, 100, 200)
#define MARKDOWN_DEFAULT_TEXT_COLOR GetSysColor(COLOR_WINDOWTEXT)
BOOL EnsureLinkCapacity(ParseState* state);
BOOL EnsureHeadingCapacity(ParseState* state);
BOOL EnsureStyleCapacity(ParseState* state);
BOOL EnsureListItemCapacity(ParseState* state);
BOOL EnsureBlockquoteCapacity(ParseState* state);
BOOL EnsureColorTagCapacity(ParseState* state);
BOOL EnsureFontTagCapacity(ParseState* state);
BOOL AppendMarkdownOutputSpan(ParseState* state, const wchar_t* text, size_t textLen);
BOOL AppendMarkdownOutputChar(ParseState* state, wchar_t ch);
void SyncMarkdownOutputPointer(const ParseState* state, wchar_t** dest);
void CleanupParseState(ParseState* state);
void DetachParseState(ParseState* state);
int GetInitialLinkCapacity(int estimatedCount);
int GetInitialHeadingCapacity(int estimatedCount);
int GetInitialStyleCapacity(int estimatedCount);
int GetInitialListItemCapacity(int estimatedCount);
int GetInitialBlockquoteCapacity(int estimatedCount);
int GetInitialColorTagCapacity(int estimatedCount);
int GetInitialFontTagCapacity(int estimatedCount);
BOOL ExtractWideString(const wchar_t* start, const wchar_t* end, wchar_t** output);
int CountMarkdownLinks(const wchar_t* input);
int CountMarkdownHeadings(const wchar_t* input);
int CountMarkdownStyles(const wchar_t* input);
int CountMarkdownListItems(const wchar_t* input);
int CountMarkdownBlockquotes(const wchar_t* input);
int CountMarkdownColorTags(const wchar_t* input);
int CountMarkdownFontTags(const wchar_t* input);
BOOL ExtractMarkdownLink(const wchar_t** src, ParseState* state);
BOOL ExtractMarkdownStyle(const wchar_t** src, ParseState* state);
BOOL ExtractMarkdownCode(const wchar_t** src, ParseState* state);
BOOL ExtractMarkdownStrikethrough(const wchar_t** src, ParseState* state);
BOOL ExtractMarkdownColorTag(const wchar_t** src, ParseState* state);
BOOL ExtractMarkdownFontTag(const wchar_t** src, ParseState* state);
BOOL ProcessInlineElements(const wchar_t** src, ParseState* state, wchar_t** dest);
BOOL ParseCodeBlock(const wchar_t** src, ParseState* state, wchar_t** dest, BOOL* inCodeBlock);
BOOL ParseCodeBlockContent(const wchar_t** src, ParseState* state, wchar_t** dest);
BOOL ParseHorizontalRule(const wchar_t** src, ParseState* state, wchar_t** dest);
BOOL ParseList(const wchar_t** src, ParseState* state, wchar_t** dest, BOOL* inListItem, int* currentListItemIndex);
BOOL ParseHeading(const wchar_t** src, ParseState* state, wchar_t** dest, BOOL* inHeading, int* currentHeadingIndex);
BOOL ParseBlockquote(const wchar_t** src, ParseState* state, wchar_t** dest);
BOOL ParseBlockquoteContent(const wchar_t** src, ParseState* state, wchar_t** dest, int blockquoteIndex);
#endif // MARKDOWN_PARSER_H
