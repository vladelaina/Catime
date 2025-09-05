/**
 * @file markdown_parser.c
 * @brief Implementation of markdown link parser for text rendering
 * @author Catime Team
 */

#include "markdown_parser.h"
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>

/**
 * @brief Parse markdown-style links [text](url) from input text
 * 
 * Extracts markdown link patterns and creates clean display text while
 * preserving link information for rendering and interaction.
 */
void ParseMarkdownLinks(const wchar_t* input, wchar_t** displayText, MarkdownLink** links, int* linkCount) {
    if (!input || !displayText || !links || !linkCount) return;
    
    int inputLen = wcslen(input);
    *displayText = (wchar_t*)malloc((inputLen + 1) * sizeof(wchar_t));
    *links = NULL;
    *linkCount = 0;
    
    if (!*displayText) return;
    
    wchar_t* dest = *displayText;
    const wchar_t* src = input;
    int linkCapacity = 10;
    *links = (MarkdownLink*)malloc(linkCapacity * sizeof(MarkdownLink));
    
    if (!*links) {
        free(*displayText);
        *displayText = NULL;
        return;
    }
    
    int currentPos = 0;  // Track position in display text for link rectangles
    
    while (*src) {
        if (*src == L'[') {
            // Found potential link start
            const wchar_t* linkTextStart = src + 1;
            const wchar_t* linkTextEnd = wcschr(linkTextStart, L']');
            
            if (linkTextEnd && linkTextEnd[1] == L'(' ) {
                const wchar_t* urlStart = linkTextEnd + 2;
                const wchar_t* urlEnd = wcschr(urlStart, L')');
                
                if (urlEnd) {
                    // Valid markdown link found - expand array if needed
                    if (*linkCount >= linkCapacity) {
                        linkCapacity *= 2;
                        MarkdownLink* newLinks = (MarkdownLink*)realloc(*links, linkCapacity * sizeof(MarkdownLink));
                        if (!newLinks) {
                            // Failed to reallocate - clean up and return
                            FreeMarkdownLinks(*links, *linkCount);
                            free(*displayText);
                            *links = NULL;
                            *displayText = NULL;
                            *linkCount = 0;
                            return;
                        }
                        *links = newLinks;
                    }
                    
                    MarkdownLink* link = &(*links)[*linkCount];
                    
                    // Extract link text
                    int textLen = linkTextEnd - linkTextStart;
                    link->linkText = (wchar_t*)malloc((textLen + 1) * sizeof(wchar_t));
                    if (!link->linkText) {
                        FreeMarkdownLinks(*links, *linkCount);
                        free(*displayText);
                        *links = NULL;
                        *displayText = NULL;
                        *linkCount = 0;
                        return;
                    }
                    wcsncpy(link->linkText, linkTextStart, textLen);
                    link->linkText[textLen] = L'\0';
                    
                    // Extract URL
                    int urlLen = urlEnd - urlStart;
                    link->linkUrl = (wchar_t*)malloc((urlLen + 1) * sizeof(wchar_t));
                    if (!link->linkUrl) {
                        free(link->linkText);
                        FreeMarkdownLinks(*links, *linkCount);
                        free(*displayText);
                        *links = NULL;
                        *displayText = NULL;
                        *linkCount = 0;
                        return;
                    }
                    wcsncpy(link->linkUrl, urlStart, urlLen);
                    link->linkUrl[urlLen] = L'\0';
                    
                    // Store start and end position of link in display text
                    link->startPos = currentPos;
                    link->endPos = currentPos + textLen;
                    
                    // Initialize link rectangle (to be calculated later)
                    SetRect(&link->linkRect, 0, 0, 0, 0);
                    
                    // Copy link text to display text
                    wcsncpy(dest, link->linkText, textLen);
                    dest += textLen;
                    currentPos += textLen;
                    
                    (*linkCount)++;
                    src = urlEnd + 1;
                    continue;
                }
            }
        }
        
        // Copy regular character
        *dest++ = *src++;
        currentPos++;
    }
    
    *dest = L'\0';
}

/**
 * @brief Free memory allocated for parsed markdown links
 */
void FreeMarkdownLinks(MarkdownLink* links, int linkCount) {
    if (!links) return;
    
    for (int i = 0; i < linkCount; i++) {
        if (links[i].linkText) {
            free(links[i].linkText);
            links[i].linkText = NULL;
        }
        if (links[i].linkUrl) {
            free(links[i].linkUrl);
            links[i].linkUrl = NULL;
        }
    }
    free(links);
}

/**
 * @brief Check if a point is within any link and return the link URL
 */
const wchar_t* GetClickedLinkUrl(MarkdownLink* links, int linkCount, POINT point) {
    if (!links) return NULL;
    
    for (int i = 0; i < linkCount; i++) {
        if (PtInRect(&links[i].linkRect, point)) {
            return links[i].linkUrl;
        }
    }
    return NULL;
}

/**
 * @brief Update link rectangles based on text metrics and positions
 * 
 * This function calculates the actual screen rectangles for each link
 * by measuring text positions and font metrics.
 */
void UpdateMarkdownLinkRects(MarkdownLink* links, int linkCount, const wchar_t* displayText, HDC hdc, RECT drawRect) {
    if (!links || !displayText || !hdc) return;
    
    // Get text metrics for line height calculation
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    int lineHeight = tm.tmHeight;
    
    int textLen = wcslen(displayText);
    int x = drawRect.left;
    int y = drawRect.top;
    
    // Calculate positions for each character
    for (int i = 0; i < textLen; i++) {
        wchar_t ch = displayText[i];
        
        // Handle newlines
        if (ch == L'\n') {
            x = drawRect.left;
            y += lineHeight;
            continue;
        }
        
        // Get character width
        SIZE charSize;
        GetTextExtentPoint32W(hdc, &ch, 1, &charSize);
        
        // Check if this character is part of any link
        for (int j = 0; j < linkCount; j++) {
            if (i >= links[j].startPos && i < links[j].endPos) {
                // Update link rectangle bounds
                if (i == links[j].startPos) {
                    // First character of link - set initial bounds
                    links[j].linkRect.left = x;
                    links[j].linkRect.top = y;
                    links[j].linkRect.bottom = y + lineHeight;
                }
                
                if (i == links[j].endPos - 1) {
                    // Last character of link - set right bound
                    links[j].linkRect.right = x + charSize.cx;
                }
                break;
            }
        }
        
        x += charSize.cx;
    }
}

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
BOOL IsCharacterInLink(MarkdownLink* links, int linkCount, int position, int* linkIndex) {
    if (!links) return FALSE;
    
    for (int i = 0; i < linkCount; i++) {
        if (position >= links[i].startPos && position < links[i].endPos) {
            if (linkIndex) *linkIndex = i;
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief Render markdown text with clickable links
 * 
 * This function renders text with markdown links, applying appropriate colors
 * and calculating link rectangles for click detection.
 */
void RenderMarkdownText(HDC hdc, const wchar_t* displayText, MarkdownLink* links, int linkCount, 
                        RECT drawRect, COLORREF linkColor, COLORREF normalColor) {
    if (!hdc || !displayText) return;
    
    int textLen = wcslen(displayText);
    int x = drawRect.left;
    int y = drawRect.top;
    
    // Get text metrics for line height
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    int lineHeight = tm.tmHeight;
    
    for (int i = 0; i < textLen; i++) {
        wchar_t ch = displayText[i];
        
        // Check if this character is part of a link
        int linkIndex = -1;
        BOOL isLink = IsCharacterInLink(links, linkCount, i, &linkIndex);
        
        // Set color
        if (isLink) {
            SetTextColor(hdc, linkColor);
        } else {
            SetTextColor(hdc, normalColor);
        }
        
        // Handle newlines
        if (ch == L'\n') {
            x = drawRect.left;
            y += lineHeight;
            continue;
        }
        
        // Draw character
        TextOutW(hdc, x, y, &ch, 1);
        
        // Move to next character position
        SIZE charSize;
        GetTextExtentPoint32W(hdc, &ch, 1, &charSize);
        x += charSize.cx;
        
        // Wrap text if needed (assuming we want to wrap at right edge)
        if (x > drawRect.right - 10) {
            x = drawRect.left;
            y += lineHeight;
        }
    }
    
    // Update link rectangles for click detection
    UpdateMarkdownLinkRects(links, linkCount, displayText, hdc, drawRect);
}

/**
 * @brief Handle click on markdown text and open URLs
 * 
 * This function checks if a click point intersects with any link
 * and automatically opens the URL using ShellExecute.
 */
BOOL HandleMarkdownClick(MarkdownLink* links, int linkCount, POINT clickPoint) {
    const wchar_t* clickedUrl = GetClickedLinkUrl(links, linkCount, clickPoint);
    if (clickedUrl) {
        ShellExecuteW(NULL, L"open", clickedUrl, NULL, NULL, SW_SHOWNORMAL);
        return TRUE;
    }
    return FALSE;
}
