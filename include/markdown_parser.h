/**
 * @file markdown_parser.h
 * @brief Markdown link parser for text rendering with clickable links
 * @author Catime Team
 */

#ifndef MARKDOWN_PARSER_H
#define MARKDOWN_PARSER_H

#include <windows.h>

/**
 * @brief Structure to store parsed markdown links
 * 
 * This structure represents a single markdown link [text](url) that has been
 * parsed from input text, including its position and display information.
 */
typedef struct {
    wchar_t* linkText;    /**< Display text for the link */
    wchar_t* linkUrl;     /**< URL to open when link is clicked */
    RECT linkRect;        /**< Rectangle for click detection in UI */
    int startPos;         /**< Start position in display text */
    int endPos;           /**< End position in display text */
} MarkdownLink;

/**
 * @brief Parse markdown-style links [text](url) from input text
 * 
 * This function scans input text for markdown link patterns and extracts them
 * into separate structures while creating clean display text without markup.
 * 
 * @param input Input text containing markdown links
 * @param displayText Output text with markdown removed (caller must free)
 * @param links Output array of parsed links (caller must free with FreeMarkdownLinks)
 * @param linkCount Output number of links found
 * 
 * @example
 * ```c
 * wchar_t* text = L"Visit [GitHub](https://github.com) for more info.";
 * wchar_t* display;
 * MarkdownLink* links;
 * int count;
 * ParseMarkdownLinks(text, &display, &links, &count);
 * // display will be: "Visit GitHub for more info."
 * // links[0].linkText will be: "GitHub"
 * // links[0].linkUrl will be: "https://github.com"
 * FreeMarkdownLinks(links, count);
 * free(display);
 * ```
 */
void ParseMarkdownLinks(const wchar_t* input, wchar_t** displayText, MarkdownLink** links, int* linkCount);

/**
 * @brief Free memory allocated for parsed markdown links
 * 
 * This function properly frees all memory allocated by ParseMarkdownLinks
 * including the link text, URLs, and the array itself.
 * 
 * @param links Array of MarkdownLink structures to free
 * @param linkCount Number of links in the array
 */
void FreeMarkdownLinks(MarkdownLink* links, int linkCount);

/**
 * @brief Check if a point is within any link and return the link URL
 * 
 * Helper function to determine if a mouse click position intersects
 * with any parsed markdown link rectangles.
 * 
 * @param links Array of MarkdownLink structures
 * @param linkCount Number of links in the array
 * @param point Point to test (in client coordinates)
 * @return URL of the clicked link, or NULL if no link was clicked
 */
const wchar_t* GetClickedLinkUrl(MarkdownLink* links, int linkCount, POINT point);

/**
 * @brief Update link rectangles based on text metrics and positions
 * 
 * This function calculates the actual screen rectangles for each link
 * based on font metrics and text positioning, enabling accurate click detection.
 * 
 * @param links Array of MarkdownLink structures to update
 * @param linkCount Number of links in the array
 * @param displayText The display text containing the links
 * @param hdc Device context for text measurement
 * @param drawRect Rectangle where text is drawn
 */
void UpdateMarkdownLinkRects(MarkdownLink* links, int linkCount, const wchar_t* displayText, HDC hdc, RECT drawRect);

/**
 * @brief Check if a character position is within a link
 * 
 * Helper function to determine if a character at a given position
 * is part of a clickable link for styling purposes.
 * 
 * @param links Array of MarkdownLink structures
 * @param linkCount Number of links in the array
 * @param position Character position in display text
 * @param linkIndex Output parameter for the link index (optional)
 * @return TRUE if position is within a link, FALSE otherwise
 */
BOOL IsCharacterInLink(MarkdownLink* links, int linkCount, int position, int* linkIndex);

/**
 * @brief Render markdown text with clickable links
 * 
 * This function renders text with markdown links, applying appropriate colors
 * and calculating link rectangles for click detection.
 * 
 * @param hdc Device context for drawing
 * @param displayText The display text to render
 * @param links Array of MarkdownLink structures
 * @param linkCount Number of links in the array
 * @param drawRect Rectangle to draw the text within
 * @param linkColor Color for link text (RGB value)
 * @param normalColor Color for normal text (RGB value)
 */
void RenderMarkdownText(HDC hdc, const wchar_t* displayText, MarkdownLink* links, int linkCount, 
                        RECT drawRect, COLORREF linkColor, COLORREF normalColor);

/**
 * @brief Handle click on markdown text and open URLs
 * 
 * This function checks if a click point intersects with any link
 * and automatically opens the URL using ShellExecute.
 * 
 * @param links Array of MarkdownLink structures
 * @param linkCount Number of links in the array
 * @param clickPoint Click position in client coordinates
 * @return TRUE if a link was clicked and opened, FALSE otherwise
 */
BOOL HandleMarkdownClick(MarkdownLink* links, int linkCount, POINT clickPoint);

/**
 * @brief Default colors for markdown rendering
 */
#define MARKDOWN_DEFAULT_LINK_COLOR RGB(0, 100, 200)      /**< Default blue color for links */
#define MARKDOWN_DEFAULT_TEXT_COLOR GetSysColor(COLOR_WINDOWTEXT)  /**< Default system text color */

#endif // MARKDOWN_PARSER_H
