/**
 * @file drawing_text_stb.h
 * @brief High-quality text rendering using stb_truetype
 */

#ifndef DRAWING_TEXT_STB_H
#define DRAWING_TEXT_STB_H

#include <windows.h>

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
 * @brief Free cached font resources
 */
void CleanupFontSTB(void);

#endif /* DRAWING_TEXT_STB_H */
