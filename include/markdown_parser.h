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
 * @brief Parse [text](url) links from input
 * @param input Input text with markdown
 * @param displayText Output without markup (caller must free)
 * @param links Output link array (caller must free with FreeMarkdownLinks)
 * @param linkCount Output link count
 * @return TRUE on success, FALSE on allocation failure
 * 
 * @details
 * Pre-allocates exact memory (scans once to count, eliminating realloc overhead).
 * 
 * @example
 * ```c
 * // "Visit [GitHub](https://github.com)" → "Visit GitHub"
 * if (ParseMarkdownLinks(text, &display, &links, &count)) {
 *     FreeMarkdownLinks(links, count);
 *     free(display);
 * }
 * ```
 */
BOOL ParseMarkdownLinks(const wchar_t* input, wchar_t** displayText, MarkdownLink** links, int* linkCount);

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
 * @brief Render text with links (single-pass: O(n) instead of O(2n))
 * @param hdc Device context
 * @param displayText Text to render
 * @param links Link array (rectangles updated in-place)
 * @param linkCount Link count
 * @param drawRect Draw bounds
 * @param linkColor Link text color
 * @param normalColor Normal text color
 * 
 * @details
 * Combines rendering and rectangle calculation in one pass (eliminates
 * separate UpdateMarkdownLinkRects call). Unified text layout prevents
 * position inconsistencies.
 * 
 * @note Link rectangles ready for GetClickedLinkUrl() after return
 */
void RenderMarkdownText(HDC hdc, const wchar_t* displayText, MarkdownLink* links, int linkCount, 
                        RECT drawRect, COLORREF linkColor, COLORREF normalColor);

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
