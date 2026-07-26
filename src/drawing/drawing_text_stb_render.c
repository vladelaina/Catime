/**
 * @file drawing_text_stb_render.c
 * @brief Text measurement and line-by-line rendering.
 */

#include "drawing_text_stb_internal.h"

BOOL MeasureTextSTB(const wchar_t* text, int fontSize, int* width, int* height) {
    if (!BeginFontUseSTB()) return FALSE;
    BOOL result = FALSE;

    if (!g_fontLoaded || !text) goto done;

    float scale = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)fontSize);
    float fallbackScale = g_fallbackFontLoaded ? stbtt_ScaleForPixelHeight(&g_fallbackFontInfo, (float)fontSize) : 0;

    int maxWidth = 0;
    int curLineWidth = 0;
    int lineCount = 1;
    size_t len = wcslen(text);

    for (size_t i = 0; i < len; i++) {
        if (text[i] == L'\n') {
            if (curLineWidth > maxWidth) maxWidth = curLineWidth;
            curLineWidth = 0;
            lineCount++;
            continue;
        }
        if (text[i] == L'\r') continue;

        GlyphMetrics gm;
        GetCharMetricsSTB(text[i], (i < len - 1) ? text[i+1] : 0, scale, fallbackScale, &gm);
        curLineWidth = AddTextIntClamped(curLineWidth, gm.advance + gm.kern);
    }
    if (curLineWidth > maxWidth) maxWidth = curLineWidth;

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_fontInfo, &ascent, &descent, &lineGap);
    int lineHeight = ScaleTextMetricClamped(ascent - descent + lineGap, scale);

    if (width) *width = maxWidth;
    if (height) *height = MulTextIntClamped(lineCount, lineHeight);
    result = TRUE;

done:
    EndFontUseSTB();
    return result;
}

void RenderTextSTB(void* bits, int width, int height, const wchar_t* text,
                   COLORREF color, int fontSize, float fontScale, BOOL editMode) {
    UNREFERENCED_PARAMETER(editMode);

    if (!BeginFontUseSTB()) return;
    if (!g_fontLoaded || !text || !bits) goto done;

    float scale = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)(fontSize * fontScale));
    float fallbackScale = g_fallbackFontLoaded ? stbtt_ScaleForPixelHeight(&g_fallbackFontInfo, (float)(fontSize * fontScale)) : 0;

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_fontInfo, &ascent, &descent, &lineGap);
    int lineHeight = ScaleTextMetricClamped(ascent - descent + lineGap, scale);
    int baselineOffset = ScaleTextMetricClamped(ascent, scale);

    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    EffectType effect = GetActiveEffect();
    int timeOffset = (int)GetTickCount();

    // Pre-calculate line widths for centering
    // We can do a quick pass or re-use Measure logic per line
    size_t len = wcslen(text);
    int currentY = 0;

    // Calculate total text height to vertically center the whole block.
    // Width metrics are computed per line below, so only line count is needed here.
    int lineCount = 1;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == L'\n') {
            lineCount++;
        }
    }
    int totalTextHeight = MulTextIntClamped(lineCount, lineHeight);

    int startY = (height - totalTextHeight) / 2;
    size_t currentLineStart = 0;

    for (size_t i = 0; i <= len; i++) {
        if (text[i] == L'\n' || text[i] == L'\0') {
            // Line complete, render it
            int lineWidth = 0;
            // Calculate width of this line
            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;
                GlyphMetrics gm;
                GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);
                lineWidth = AddTextIntClamped(lineWidth, gm.advance + gm.kern);
            }

            int currentX = (width - lineWidth) / 2;
            int lineY = AddTextIntClamped(AddTextIntClamped(startY,
                                                            MulTextIntClamped(currentY, lineHeight)),
                                          baselineOffset);

            // Render line
            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;

                GlyphMetrics gm;
                GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);

                if (gm.index != 0 && text[j] != L' ' && text[j] != L'\t') {
                    int w, h, xoff, yoff;
                    unsigned char* bitmap = NULL;

                    const stbtt_fontinfo* glyphFontInfo = gm.isFallback ? &g_fallbackFontInfo : &g_fontInfo;
                    float glyphScale = gm.isFallback ? fallbackScale : scale;
                    int glyphMargin = (effect != EFFECT_TYPE_NONE) ? 24 : 0;
                    bitmap = CreateVisibleGlyphBitmapSTB(glyphFontInfo, gm.index,
                                                         glyphScale, glyphScale,
                                                         currentX, lineY,
                                                         width, height,
                                                         glyphMargin,
                                                         &w, &h, &xoff, &yoff);

                    if (bitmap) {
                        int glyphX = AddTextIntClamped(currentX, xoff);
                        int glyphY = AddTextIntClamped(lineY, yoff);
                        BlendCharBitmapSTBWithEffect(bits, width, height,
                                                     glyphX, glyphY,
                                                     bitmap, w, h, r, g, b,
                                                     effect, timeOffset);
                        stbtt_FreeBitmap(bitmap, NULL);
                    }
                }
                currentX = AddTextIntClamped(currentX, gm.advance + gm.kern);
            }

            currentY++;
            currentLineStart = i + 1;
        }
    }

done:
    EndFontUseSTB();
}

/* ============================================================================
 * Font Cache Implementation for <font:> Tags
 * ============================================================================ */

/**
 * @brief Resolve font path from tag value
 *
 * Supports:
 * - Absolute paths: C:\Fonts\my.ttf, \\server\share\font.ttf
 * - Environment variables: %WINDIR%\Fonts\arial.ttf
 * - Relative paths: fonts/custom.ttf (resolved relative to plugins directory)
 *
 * @param fontPath Font path from <font:> tag (wide string)
 * @param outPath Output buffer for resolved path (wide string)
 * @param pathSize Buffer size
 * @return TRUE if path resolved and file exists
 */
