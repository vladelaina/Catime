/**
 * @file drawing_text_stb.h
 * @brief High-quality text rendering using stb_truetype
 */

#ifndef DRAWING_TEXT_STB_H
#define DRAWING_TEXT_STB_H

#include <windows.h>
#include "markdown/markdown_parser.h"

/**
 * @brief Initialize STB font engine with a specific font file
 * @param fontFilePath Absolute path to the .ttf/.otf file
 * @return TRUE if loaded successfully
 */
BOOL InitFontSTB(const char* fontFilePath);

/**
 * @brief Render text to a 32-bit ARGB buffer using STB Truetype
 * @param bits Pointer to the pixel bits (BGRA/ARGB)
 * @param width Width of the bitmap
 * @param height Height of the bitmap
 * @param text Text to render
 * @param color Text color (RGB)
 * @param fontSize Font size in pixels
 * @param fontScale Scale factor (e.g., 1.0, 1.5)
 * @param editMode If TRUE, draws debug outlines or placeholders if needed
 */
void RenderTextSTB(void* bits, int width, int height, const wchar_t* text, 
                   COLORREF color, int fontSize, float fontScale, BOOL editMode);

/**
 * @brief Render Markdown text to a 32-bit ARGB buffer using STB Truetype
 * @param bits Pointer to the pixel bits (BGRA/ARGB)
 * @param width Width of the bitmap
 * @param height Height of the bitmap
 * @param text Clean text to render (from parser)
 * @param links Array of links
 * @param linkCount Number of links
 * @param headings Array of headings
 * @param headingCount Number of headings
 * @param styles Array of inline styles
 * @param styleCount Number of styles
 * @param color Base text color (RGB)
 * @param fontSize Base font size in pixels
 * @param fontScale Base font scale factor
 */
void RenderMarkdownSTB(void* bits, int width, int height, const wchar_t* text,
                       MarkdownLink* links, int linkCount,
                       MarkdownHeading* headings, int headingCount,
                       MarkdownStyle* styles, int styleCount,
                       COLORREF color, int fontSize, float fontScale);

/**
 * @brief Measure text dimensions using STB font (supports multiline)
 * @param text The text to measure
 * @param fontSize Font size in pixels
 * @param width Output pointer for width
 * @param height Output pointer for height
 * @return TRUE if successful
 */
BOOL MeasureTextSTB(const wchar_t* text, int fontSize, int* width, int* height);

/**
 * @brief Measure Markdown text dimensions
 */
BOOL MeasureMarkdownSTB(const wchar_t* text,
                        MarkdownHeading* headings, int headingCount,
                        int fontSize, int* width, int* height);

/**
 * @brief Free cached font resources
 */
void CleanupFontSTB(void);

#endif /* DRAWING_TEXT_STB_H */
