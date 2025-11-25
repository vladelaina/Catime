/**
 * @file drawing_markdown_stb.h
 * @brief Markdown rendering logic using STB Truetype
 */

#ifndef DRAWING_MARKDOWN_STB_H
#define DRAWING_MARKDOWN_STB_H

#include <windows.h>
#include "markdown/markdown_parser.h"
#include "drawing/drawing_text_stb.h"

/**
 * @brief Measure multi-line Markdown text with headings
 */
BOOL MeasureMarkdownSTB(const wchar_t* text,
                        MarkdownHeading* headings, int headingCount,
                        int fontSize, int* width, int* height);

/**
 * @brief Render multi-line Markdown text
 */
void RenderMarkdownSTB(void* bits, int width, int height, const wchar_t* text,
                       MarkdownLink* links, int linkCount,
                       MarkdownHeading* headings, int headingCount,
                       MarkdownStyle* styles, int styleCount,
                       COLORREF color, int fontSize, float fontScale, BOOL isGradientMode);

#endif // DRAWING_MARKDOWN_STB_H
