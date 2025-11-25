/**
 * @file drawing_markdown_stb.c
 * @brief Implementation of Markdown rendering logic
 */

#include "drawing/drawing_markdown_stb.h"
#include "drawing/drawing_text_stb.h"
#include "log.h"
#include <math.h>

/* Helper Functions */

static float GetScaleForHeading(int level, float baseScale) {
    switch (level) {
        case 1: return baseScale * 2.0f;
        case 2: return baseScale * 1.5f;
        case 3: return baseScale * 1.25f;
        case 4: return baseScale * 1.1f;
        default: return baseScale;
    }
}

static int GetLineHeight(float scale) {
    int ascent, descent, lineGap;
    if (!IsFontLoadedSTB()) return 0;
    stbtt_GetFontVMetrics(GetMainFontInfoSTB(), &ascent, &descent, &lineGap);
    return (int)((ascent - descent + lineGap) * scale);
}

/* Public API */

BOOL MeasureMarkdownSTB(const wchar_t* text,
                        MarkdownHeading* headings, int headingCount,
                        int fontSize, int* width, int* height) {
    if (!IsFontLoadedSTB() || !text) return FALSE;

    stbtt_fontinfo* fontInfo = GetMainFontInfoSTB();
    stbtt_fontinfo* fallbackFontInfo = GetFallbackFontInfoSTB();
    BOOL fallbackLoaded = IsFallbackFontLoadedSTB();

    float baseScale = stbtt_ScaleForPixelHeight(fontInfo, (float)fontSize);
    float fallbackBaseScale = fallbackLoaded ? stbtt_ScaleForPixelHeight(fallbackFontInfo, (float)fontSize) : 0;

    int maxWidth = 0;
    int curLineWidth = 0;
    int totalHeight = 0;
    int curLineMaxHeight = GetLineHeight(baseScale); // Default to base height

    size_t len = wcslen(text);
    
    // Optimization: Track current heading index
    int curHeadingIdx = 0;

    for (size_t i = 0; i < len; i++) {
        if (text[i] == L'\n') {
            if (curLineWidth > maxWidth) maxWidth = curLineWidth;
            curLineWidth = 0;
            totalHeight += curLineMaxHeight;
            curLineMaxHeight = GetLineHeight(baseScale); // Reset to base
            continue;
        }
        if (text[i] == L'\r') continue;

        // Determine style
        float scale = baseScale;
        float fallbackScale = fallbackBaseScale;
        
        // Check heading
        while (curHeadingIdx < headingCount && i >= headings[curHeadingIdx].endPos) {
            curHeadingIdx++;
        }
        if (curHeadingIdx < headingCount && i >= headings[curHeadingIdx].startPos) {
            scale = GetScaleForHeading(headings[curHeadingIdx].level, baseScale);
            if (fallbackLoaded) {
                fallbackScale = GetScaleForHeading(headings[curHeadingIdx].level, fallbackBaseScale);
            }
        }

        // Update line height if this char is taller
        int h = GetLineHeight(scale);
        if (h > curLineMaxHeight) curLineMaxHeight = h;

        GlyphMetrics gm;
        GetCharMetricsSTB(text[i], (i < len - 1) ? text[i+1] : 0, scale, fallbackScale, &gm);
        curLineWidth += gm.advance + gm.kern;
    }
    if (curLineWidth > maxWidth) maxWidth = curLineWidth;
    totalHeight += curLineMaxHeight;

    if (width) *width = maxWidth;
    if (height) *height = totalHeight;
    
    return TRUE;
}

void RenderMarkdownSTB(void* bits, int width, int height, const wchar_t* text,
                       MarkdownLink* links, int linkCount,
                       MarkdownHeading* headings, int headingCount,
                       MarkdownStyle* styles, int styleCount,
                       COLORREF color, int fontSize, float fontScale, BOOL isGradientMode) {
    if (!IsFontLoadedSTB() || !text || !bits) return;

    stbtt_fontinfo* fontInfo = GetMainFontInfoSTB();
    stbtt_fontinfo* fallbackFontInfo = GetFallbackFontInfoSTB();
    BOOL fallbackLoaded = IsFallbackFontLoadedSTB();

    float baseScale = stbtt_ScaleForPixelHeight(fontInfo, (float)(fontSize * fontScale));
    float fallbackBaseScale = fallbackLoaded ? stbtt_ScaleForPixelHeight(fallbackFontInfo, (float)(fontSize * fontScale)) : 0;
    
    int baseAscent, baseDescent, baseLineGap;
    stbtt_GetFontVMetrics(fontInfo, &baseAscent, &baseDescent, &baseLineGap);

    // Calculate total layout to center vertically
    int totalTextHeight = 0;
    int w_dummy, h_dummy;
    MeasureMarkdownSTB(text, headings, headingCount, (int)(fontSize * fontScale), &w_dummy, &h_dummy);
    totalTextHeight = h_dummy;
    
    int currentY = (height - totalTextHeight) / 2;
    
    size_t len = wcslen(text);
    int currentLineStart = 0;
    
    // State trackers
    int curHeadingIdx = 0;
    int curLinkIdx = 0;
    int curStyleIdx = 0;

    for (size_t i = 0; i <= len; i++) {
        if (text[i] == L'\n' || text[i] == L'\0') {
            // 1. Measure this line to center horizontally AND find max height
            int lineWidth = 0;
            int lineMaxHeight = GetLineHeight(baseScale);
            int maxAscent = (int)(baseAscent * baseScale);

            // Temp indices for measurement pass
            int tmpHeadingIdx = curHeadingIdx;

            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;
                
                float scale = baseScale;
                float fallbackScale = fallbackBaseScale;

                while (tmpHeadingIdx < headingCount && j >= headings[tmpHeadingIdx].endPos) tmpHeadingIdx++;
                if (tmpHeadingIdx < headingCount && j >= headings[tmpHeadingIdx].startPos) {
                    scale = GetScaleForHeading(headings[tmpHeadingIdx].level, baseScale);
                    if (fallbackLoaded) fallbackScale = GetScaleForHeading(headings[tmpHeadingIdx].level, fallbackBaseScale);
                }

                int h = GetLineHeight(scale);
                if (h > lineMaxHeight) lineMaxHeight = h;
                
                int asc = (int)(baseAscent * scale); // Approximation
                if (asc > maxAscent) maxAscent = asc;

                GlyphMetrics gm;
                GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);
                lineWidth += gm.advance + gm.kern;
            }

            int currentX = (width - lineWidth) / 2;
            // Baseline align: Y + maxAscent
            int baselineY = currentY + maxAscent;

            // 2. Render this line
            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;

                // Determine styles
                float scale = baseScale;
                float fallbackScale = fallbackBaseScale;
                COLORREF drawColor = color;

                // Heading
                while (curHeadingIdx < headingCount && j >= headings[curHeadingIdx].endPos) curHeadingIdx++;
                if (curHeadingIdx < headingCount && j >= headings[curHeadingIdx].startPos) {
                    scale = GetScaleForHeading(headings[curHeadingIdx].level, baseScale);
                    if (fallbackLoaded) fallbackScale = GetScaleForHeading(headings[curHeadingIdx].level, fallbackBaseScale);
                }

                // Link
                while (curLinkIdx < linkCount && j >= links[curLinkIdx].endPos) curLinkIdx++;
                if (curLinkIdx < linkCount && j >= links[curLinkIdx].startPos) {
                    drawColor = RGB(9, 105, 218); // Link blue
                }
                
                // Style (Code) - only background for now, color override if needed
                while (curStyleIdx < styleCount && j >= styles[curStyleIdx].endPos) curStyleIdx++;
                if (curStyleIdx < styleCount && j >= styles[curStyleIdx].startPos) {
                    if (styles[curStyleIdx].type == STYLE_CODE) {
                        drawColor = RGB(100, 100, 100);
                    }
                }

                GlyphMetrics gm;
                GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);
                
                if (gm.index != 0 && text[j] != L' ' && text[j] != L'\t') {
                    int w, h, xoff, yoff;
                    unsigned char* bitmap = NULL;
                    
                    if (gm.isFallback) {
                        bitmap = stbtt_GetGlyphBitmap(fallbackFontInfo, fallbackScale, fallbackScale, gm.index, &w, &h, &xoff, &yoff);
                    } else {
                        bitmap = stbtt_GetGlyphBitmap(fontInfo, scale, scale, gm.index, &w, &h, &xoff, &yoff);
                    }
                    
                    if (bitmap) {
                        if (isGradientMode && drawColor == color) { /* Only apply gradient to default colored text */
                            BlendCharBitmapGradientSTB(bits, width, height, 
                                currentX + xoff, baselineY + yoff, 
                                bitmap, w, h, 0, width); /* Use total width for gradient mapping */
                        } else {
                            BlendCharBitmapSTB(bits, width, height, 
                                currentX + xoff, baselineY + yoff, 
                                bitmap, w, h, 
                                GetRValue(drawColor), GetGValue(drawColor), GetBValue(drawColor));
                        }
                        stbtt_FreeBitmap(bitmap, NULL);
                    }
                }
                currentX += gm.advance + gm.kern;
            }

            currentY += lineMaxHeight;
            currentLineStart = i + 1;
        }
    }
}
