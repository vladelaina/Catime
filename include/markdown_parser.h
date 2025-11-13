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
    STYLE_CODE = 4
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
} MarkdownListItem;

/**
 * @brief Parse [text](url) links, # headings, inline styles, and list items from input
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
 * @return TRUE on success, FALSE on allocation failure
 *
 * @details
 * Pre-allocates exact memory (scans once to count, eliminating realloc overhead).
 * Supports:
 * - Headings: # ## ### #### at line start
 * - Inline styles: *italic*, **bold**, ***bold+italic***, `code`
 * - List items: - item or * item at line start (supports nested lists with indentation)
 * - Links: [text](url) or [text]() (empty URL displays as plain text)
 *
 * @example
 * ```c
 * // "# Title\n- **Bold** item\n    - `code`" → "Title\n• Bold item\n    • code"
 * if (ParseMarkdownLinks(text, &display, &links, &lcount, &headings, &hcount,
 *                        &styles, &scount, &items, &icount)) {
 *     FreeMarkdownLinks(links, lcount);
 *     free(headings);
 *     free(styles);
 *     free(items);
 *     free(display);
 * }
 * ```
 */
BOOL ParseMarkdownLinks(const wchar_t* input, wchar_t** displayText,
                        MarkdownLink** links, int* linkCount,
                        MarkdownHeading** headings, int* headingCount,
                        MarkdownStyle** styles, int* styleCount,
                        MarkdownListItem** listItems, int* listItemCount);

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
 * @brief Render text with links, headings, inline styles, and list items (single-pass: O(n))
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
 *
 * @note Link rectangles ready for GetClickedLinkUrl() after return
 */
void RenderMarkdownText(HDC hdc, const wchar_t* displayText,
                        MarkdownLink* links, int linkCount,
                        MarkdownHeading* headings, int headingCount,
                        MarkdownStyle* styles, int styleCount,
                        MarkdownListItem* listItems, int listItemCount,
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
 * @param drawRect Available drawing area
 * @return Actual rendered height in pixels
 */
int CalculateMarkdownTextHeight(HDC hdc, const wchar_t* displayText,
                                  MarkdownHeading* headings, int headingCount,
                                  MarkdownStyle* styles, int styleCount,
                                  MarkdownListItem* listItems, int listItemCount,
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

#endif // MARKDOWN_PARSER_H
