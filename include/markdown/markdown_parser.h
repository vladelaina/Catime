/**
 * @file markdown_parser.h
 * @brief Single-pass markdown parser with O(n) rendering
 * 
 * Pre-allocation eliminates realloc overhead (counts links before allocation).
 * Combined render+rectangle calculation reduces complexity from O(2n) to O(n).
 * Unified text layout prevents position inconsistencies between passes.
 */

#ifndef MARKDOWN_PARSER_H
#define MARKDOWN_PARSER_H

#include <windows.h>

/**
 * @brief Parsed markdown link [text](url)
 */
typedef struct {
    wchar_t* linkText;
    wchar_t* linkUrl;
    RECT linkRect;
    int startPos;
    int endPos;
} MarkdownLink;

/**
 * @brief Parsed markdown heading (# ## ### ####)
 */
typedef struct {
    int level;
    int startPos;
    int endPos;
} MarkdownHeading;

/**
 * @brief Inline text style types
 */
typedef enum {
    STYLE_NONE = 0,
    STYLE_ITALIC = 1,
    STYLE_BOLD = 2,
    STYLE_BOLD_ITALIC = 3,
    STYLE_CODE = 4,
    STYLE_STRIKETHROUGH = 5
} MarkdownStyleType;

/**
 * @brief Parsed inline style (*italic*, **bold**, ***both***)
 */
typedef struct {
    MarkdownStyleType type;
    int startPos;
    int endPos;
} MarkdownStyle;

/**
 * @brief Parsed list item (- item or * item)
 */
typedef struct {
    int startPos;
    int endPos;
    int indentLevel;
    BOOL isChecked;  /* TRUE if this is a completed todo (- [x]) */
} MarkdownListItem;

/**
 * @brief Blockquote alert types (GitHub Flavored Markdown)
 */
typedef enum {
    BLOCKQUOTE_NORMAL = 0,
    BLOCKQUOTE_NOTE = 1,
    BLOCKQUOTE_TIP = 2,
    BLOCKQUOTE_IMPORTANT = 3,
    BLOCKQUOTE_WARNING = 4,
    BLOCKQUOTE_CAUTION = 5
} BlockquoteAlertType;

/**
 * @brief Parsed blockquote (> text or > [!TYPE])
 */
typedef struct {
    int startPos;
    int endPos;
    BlockquoteAlertType alertType;
} MarkdownBlockquote;

/**
 * @brief Maximum colors in a gradient color tag
 */
#define MAX_COLOR_TAG_COLORS 8

/**
 * @brief Parsed color tag <color:#RRGGBB>text</color> or <color:#c1_#c2_#c3>text</color>
 */
typedef struct {
    int startPos;
    int endPos;
    COLORREF colors[MAX_COLOR_TAG_COLORS];
    int colorCount;
} MarkdownColorTag;

/**
 * @brief Maximum font name length
 */
#define MAX_FONT_NAME_LENGTH 64

/**
 * @brief Parsed font tag <font:FontName>text</font>
 */
typedef struct {
    int startPos;
    int endPos;
    wchar_t fontName[MAX_FONT_NAME_LENGTH];
} MarkdownFontTag;

/**
 * @brief Internal parser state (used by parsing logic)
 */
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

/**
 * @brief Parse [text](url) links, # headings, inline styles, list items, and blockquotes from input
 * @param input Input text with markdown
 * @param displayText Output without markup (caller must free)
 * @param links Output link array (caller must free with FreeMarkdownLinks)
 * @param linkCount Output link count
 * @param headings Output heading array (caller must free)
 * @param headingCount Output heading count
 * @param styles Output style array (caller must free)
 * @param styleCount Output style count
 * @param listItems Output list item array (caller must free)
 * @param listItemCount Output list item count
 * @param blockquotes Output blockquote array (caller must free)
 * @param blockquoteCount Output blockquote count
 * @return TRUE on success, FALSE on allocation failure
 *
 * @details
 * Pre-allocates exact memory (scans once to count, eliminating realloc overhead).
 * Supports:
 * - Headings: # ## ### #### at line start
 * - Inline styles: *italic*, **bold**, ***bold+italic***, `code`
 * - List items: - item or * item at line start (supports nested lists with indentation)
 * - Links: [text](url) or [text]() (empty URL displays as plain text)
 * - Blockquotes: > text at line start
 *
 * @example
 * ```c
 * // "# Title\n> Quote\n- **Bold** item\n    - `code`" → "Title\nQuote\n• Bold item\n    • code"
 * if (ParseMarkdownLinks(text, &display, &links, &lcount, &headings, &hcount,
 *                        &styles, &scount, &items, &icount, &quotes, &qcount)) {
 *     FreeMarkdownLinks(links, lcount);
 *     free(headings);
 *     free(styles);
 *     free(items);
 *     free(quotes);
 *     free(display);
 * }
 * ```
 */
BOOL ParseMarkdownLinks(const wchar_t* input, wchar_t** displayText,
                        MarkdownLink** links, int* linkCount,
                        MarkdownHeading** headings, int* headingCount,
                        MarkdownStyle** styles, int* styleCount,
                        MarkdownListItem** listItems, int* listItemCount,
                        MarkdownBlockquote** blockquotes, int* blockquoteCount,
                        MarkdownColorTag** colorTags, int* colorTagCount,
                        MarkdownFontTag** fontTags, int* fontTagCount);

/**
 * @brief Free parsed links (text, URLs, array)
 * @param links Link array
 * @param linkCount Link count
 */
void FreeMarkdownLinks(MarkdownLink* links, int linkCount);

/**
 * @brief Get URL if point intersects link
 * @param links Link array
 * @param linkCount Link count
 * @param point Point (client coords)
 * @return URL or NULL
 */
const wchar_t* GetClickedLinkUrl(MarkdownLink* links, int linkCount, POINT point);

/**
 * @brief Check if character position is in link (for styling)
 * @param links Link array
 * @param linkCount Link count
 * @param position Character position
 * @param linkIndex Output link index (optional)
 * @return TRUE if in link
 */
BOOL IsCharacterInLink(MarkdownLink* links, int linkCount, int position, int* linkIndex);

/**
 * @brief Check if character position is in heading (for styling)
 * @param headings Heading array
 * @param headingCount Heading count
 * @param position Character position
 * @param headingIndex Output heading index (optional)
 * @return TRUE if in heading
 */
BOOL IsCharacterInHeading(MarkdownHeading* headings, int headingCount, int position, int* headingIndex);

/**
 * @brief Check if character position is in inline style (for styling)
 * @param styles Style array
 * @param styleCount Style count
 * @param position Character position
 * @param styleIndex Output style index (optional)
 * @return TRUE if in styled text
 */
BOOL IsCharacterInStyle(MarkdownStyle* styles, int styleCount, int position, int* styleIndex);

/**
 * @brief Check if character position is in list item (for rendering)
 * @param listItems List item array
 * @param listItemCount List item count
 * @param position Character position
 * @param listItemIndex Output list item index (optional)
 * @return TRUE if in list item
 */
BOOL IsCharacterInListItem(MarkdownListItem* listItems, int listItemCount, int position, int* listItemIndex);

/**
 * @brief Check if character position is in blockquote (for rendering)
 * @param blockquotes Blockquote array
 * @param blockquoteCount Blockquote count
 * @param position Character position
 * @param blockquoteIndex Output blockquote index (optional)
 * @return TRUE if in blockquote
 */
BOOL IsCharacterInBlockquote(MarkdownBlockquote* blockquotes, int blockquoteCount, int position, int* blockquoteIndex);

/**
 * @brief Check if character position is in color tag (for rendering)
 * @param colorTags Color tag array
 * @param colorTagCount Color tag count
 * @param position Character position
 * @param colorTagIndex Output color tag index (optional)
 * @return TRUE if in color tag
 */
BOOL IsCharacterInColorTag(MarkdownColorTag* colorTags, int colorTagCount, int position, int* colorTagIndex);

/**
 * @brief Check if character position is in font tag (for rendering)
 * @param fontTags Font tag array
 * @param fontTagCount Font tag count
 * @param position Character position
 * @param fontTagIndex Output font tag index (optional)
 * @return TRUE if in font tag
 */
BOOL IsCharacterInFontTag(MarkdownFontTag* fontTags, int fontTagCount, int position, int* fontTagIndex);

/**
 * @brief Interpolate color for gradient at given position
 * @param colorTag Color tag with gradient colors
 * @param position Current character position
 * @return Interpolated COLORREF
 */
COLORREF InterpolateGradientColor(const MarkdownColorTag* colorTag, int position);

/**
 * @brief Render text with links, headings, inline styles, list items, and blockquotes (single-pass: O(n))
 * @param hdc Device context
 * @param displayText Text to render
 * @param links Link array (rectangles updated in-place)
 * @param linkCount Link count
 * @param headings Heading array
 * @param headingCount Heading count
 * @param styles Inline style array
 * @param styleCount Style count
 * @param listItems List item array
 * @param listItemCount List item count
 * @param blockquotes Blockquote array
 * @param blockquoteCount Blockquote count
 * @param drawRect Draw bounds
 * @param linkColor Link text color
 * @param normalColor Normal text color
 *
 * @details
 * Combines rendering and rectangle calculation in one pass (eliminates
 * separate UpdateMarkdownLinkRects call). Unified text layout prevents
 * position inconsistencies. Supports:
 * - Headings: bold and larger font sizes
 * - Inline styles: italic, bold, or both
 * - List items: indented with bullet points
 * - Blockquotes: indented with italic style
 *
 * @note Link rectangles ready for GetClickedLinkUrl() after return
 */
void RenderMarkdownText(HDC hdc, const wchar_t* displayText,
                        MarkdownLink* links, int linkCount,
                        MarkdownHeading* headings, int headingCount,
                        MarkdownStyle* styles, int styleCount,
                        MarkdownListItem* listItems, int listItemCount,
                        MarkdownBlockquote* blockquotes, int blockquoteCount,
                        RECT drawRect, COLORREF linkColor, COLORREF normalColor);

/**
 * @brief Calculate actual rendered height of markdown text
 * @param hdc Device context
 * @param displayText Text to measure
 * @param headings Heading array
 * @param headingCount Heading count
 * @param styles Style array
 * @param styleCount Style count
 * @param listItems List item array
 * @param listItemCount List item count
 * @param blockquotes Blockquote array
 * @param blockquoteCount Blockquote count
 * @param drawRect Available drawing area
 * @return Actual rendered height in pixels
 */
int CalculateMarkdownTextHeight(HDC hdc, const wchar_t* displayText,
                                  MarkdownHeading* headings, int headingCount,
                                  MarkdownStyle* styles, int styleCount,
                                  MarkdownListItem* listItems, int listItemCount,
                                  MarkdownBlockquote* blockquotes, int blockquoteCount,
                                  RECT drawRect);

/**
 * @brief Handle click and open URL via ShellExecute
 * @param links Link array
 * @param linkCount Link count
 * @param clickPoint Click position (client coords)
 * @return TRUE if link clicked and opened
 */
BOOL HandleMarkdownClick(MarkdownLink* links, int linkCount, POINT clickPoint);

/**
 * @brief Default colors
 */
#define MARKDOWN_DEFAULT_LINK_COLOR RGB(0, 100, 200)
#define MARKDOWN_DEFAULT_TEXT_COLOR GetSysColor(COLOR_WINDOWTEXT)

/* Internal API (state management - markdown_state.c) */

BOOL EnsureLinkCapacity(ParseState* state);
BOOL EnsureHeadingCapacity(ParseState* state);
BOOL EnsureStyleCapacity(ParseState* state);
BOOL EnsureListItemCapacity(ParseState* state);
BOOL EnsureBlockquoteCapacity(ParseState* state);
BOOL EnsureColorTagCapacity(ParseState* state);
BOOL EnsureFontTagCapacity(ParseState* state);
void CleanupParseState(ParseState* state);
int GetInitialLinkCapacity(int estimatedCount);
int GetInitialHeadingCapacity(int estimatedCount);
int GetInitialStyleCapacity(int estimatedCount);
int GetInitialListItemCapacity(int estimatedCount);
int GetInitialBlockquoteCapacity(int estimatedCount);
int GetInitialColorTagCapacity(int estimatedCount);
int GetInitialFontTagCapacity(int estimatedCount);

/* Internal API (inline elements - markdown_inline.c) */

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

/* Internal API (block elements - markdown_block.c) */

BOOL ParseCodeBlock(const wchar_t** src, ParseState* state, wchar_t** dest, BOOL* inCodeBlock);
BOOL ParseCodeBlockContent(const wchar_t** src, ParseState* state, wchar_t** dest);
BOOL ParseHorizontalRule(const wchar_t** src, ParseState* state, wchar_t** dest);
BOOL ParseList(const wchar_t** src, ParseState* state, wchar_t** dest, BOOL* inListItem, int* currentListItemIndex);
BOOL ParseHeading(const wchar_t** src, ParseState* state, wchar_t** dest, BOOL* inHeading, int* currentHeadingIndex);
BOOL ParseBlockquote(const wchar_t** src, ParseState* state, wchar_t** dest);
void ParseBlockquoteContent(const wchar_t** src, ParseState* state, wchar_t** dest, int blockquoteIndex);

#endif // MARKDOWN_PARSER_H
