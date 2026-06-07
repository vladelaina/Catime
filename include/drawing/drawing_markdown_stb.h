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
                        const MarkdownHeading* headings, int headingCount,
                        const MarkdownFontTag* fontTags, int fontTagCount,
                        int fontSize, int* width, int* height);

/**
 * @brief Render multi-line Markdown text
 */
void RenderMarkdownSTB(void* bits, int width, int height, const wchar_t* text,
                       MarkdownLink* links, int linkCount,
                       const MarkdownHeading* headings, int headingCount,
                       MarkdownStyle* styles, int styleCount,
                       MarkdownBlockquote* blockquotes, int blockquoteCount,
                       MarkdownColorTag* colorTags, int colorTagCount,
                       const MarkdownFontTag* fontTags, int fontTagCount,
                       COLORREF color, int fontSize, float fontScale, int gradientMode,
                       const GradientInfo* gradientInfo);

/**
 * @brief Render multi-line Markdown text using dimensions already calculated by MeasureMarkdownSTB
 */
void RenderMarkdownSTBMeasured(void* bits, int width, int height, const wchar_t* text,
                               MarkdownLink* links, int linkCount,
                               const MarkdownHeading* headings, int headingCount,
                               MarkdownStyle* styles, int styleCount,
                               MarkdownBlockquote* blockquotes, int blockquoteCount,
                               MarkdownColorTag* colorTags, int colorTagCount,
                               const MarkdownFontTag* fontTags, int fontTagCount,
                               COLORREF color, int fontSize, float fontScale, int gradientMode,
                               const GradientInfo* gradientInfo,
                               int measuredTextWidth, int measuredTextHeight);

#endif // DRAWING_MARKDOWN_STB_H
