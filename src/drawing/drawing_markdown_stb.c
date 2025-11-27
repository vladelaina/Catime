/**
 * @file drawing_markdown_stb.c
 * @brief Implementation of Markdown rendering logic
 */

#include "drawing/drawing_markdown_stb.h"
#include "drawing/drawing_text_stb.h"
#include "markdown/markdown_parser.h"
#include "markdown/markdown_interactive.h"
#include "color/gradient.h"
#include "log.h"
#include <math.h>

/* Helper Functions */

static float GetScaleForHeading(int level, float baseScale) {
    switch (level) {
        case 1: return baseScale * 1.5f;
        case 2: return baseScale * 1.35f;
        case 3: return baseScale * 1.2f;
        case 4: return baseScale * 1.1f;
        case 5: return baseScale * 1.0f;
        case 6: return baseScale * 0.9f;
        default: return baseScale;
    }
}

static int GetLineHeight(float scale) {
    int ascent, descent, lineGap;
    if (!IsFontLoadedSTB()) return 0;
    stbtt_GetFontVMetrics(GetMainFontInfoSTB(), &ascent, &descent, &lineGap);
    return (int)((ascent - descent + lineGap) * scale);
}

/* Italic blend with per-row shear */
static void BlendCharBitmapItalicSTB(void* destBits, int destWidth, int destHeight,
                                      int x_pos, int y_pos,
                                      unsigned char* bitmap, int w, int h,
                                      int r, int g, int b, float slant) {
    DWORD* pixels = (DWORD*)destBits;
    for (int j = 0; j < h; ++j) {
        int shear = (int)((h - j) * slant);  // Top rows shift right more
        for (int i = 0; i < w; ++i) {
            int screen_x = x_pos + i + shear;
            int screen_y = y_pos + j;
            if (screen_x < 0 || screen_x >= destWidth || screen_y < 0 || screen_y >= destHeight) continue;
            unsigned char alpha = bitmap[j * w + i];
            if (alpha == 0) continue;
            DWORD* dest = &pixels[screen_y * destWidth + screen_x];
            DWORD existing = *dest;
            int er = (existing >> 16) & 0xFF;
            int eg = (existing >> 8) & 0xFF;
            int eb = existing & 0xFF;
            int ea = (existing >> 24) & 0xFF;
            int nr = er + ((r - er) * alpha) / 255;
            int ng = eg + ((g - eg) * alpha) / 255;
            int nb = eb + ((b - eb) * alpha) / 255;
            int na = ea + ((255 - ea) * alpha) / 255;
            *dest = (na << 24) | (nr << 16) | (ng << 8) | nb;
        }
    }
}

/* Italic blend with gradient and per-row shear */
static void BlendCharBitmapItalicGradientSTB(void* destBits, int destWidth, int destHeight,
                                              int x_pos, int y_pos,
                                              unsigned char* bitmap, int w, int h,
                                              float slant, int gradientMode, int timeOffset, int totalWidth) {
    DWORD* pixels = (DWORD*)destBits;
    const GradientInfo* info = GetGradientInfo((GradientType)gradientMode);
    if (!info) return;
    
    for (int j = 0; j < h; ++j) {
        int shear = (int)((h - j) * slant);
        for (int i = 0; i < w; ++i) {
            int screen_x = x_pos + i + shear;
            int screen_y = y_pos + j;
            if (screen_x < 0 || screen_x >= destWidth || screen_y < 0 || screen_y >= destHeight) continue;
            unsigned char alpha = bitmap[j * w + i];
            if (alpha == 0) continue;
            
            // Calculate gradient color
            float t = (totalWidth > 0) ? (float)screen_x / (float)totalWidth : 0.0f;
            if (info->isAnimated) {
                float animOffset = (float)timeOffset / (float)(GRADIENT_LUT_SIZE * 2);
                t = t - animOffset;
                while (t < 0) t += 1.0f;
                while (t >= 1.0f) t -= 1.0f;
            }
            
            int r, g, b;
            if (info->palette && info->paletteCount > 2) {
                float scaledT = t * (info->paletteCount - 1);
                int idx1 = (int)scaledT;
                int idx2 = idx1 + 1;
                if (idx2 >= info->paletteCount) idx2 = 0;
                float localT = scaledT - idx1;
                COLORREF c1 = info->palette[idx1];
                COLORREF c2 = info->palette[idx2];
                r = (int)(GetRValue(c1) + (GetRValue(c2) - GetRValue(c1)) * localT);
                g = (int)(GetGValue(c1) + (GetGValue(c2) - GetGValue(c1)) * localT);
                b = (int)(GetBValue(c1) + (GetBValue(c2) - GetBValue(c1)) * localT);
            } else {
                r = (int)(GetRValue(info->startColor) + (GetRValue(info->endColor) - GetRValue(info->startColor)) * t);
                g = (int)(GetGValue(info->startColor) + (GetGValue(info->endColor) - GetGValue(info->startColor)) * t);
                b = (int)(GetBValue(info->startColor) + (GetBValue(info->endColor) - GetBValue(info->startColor)) * t);
            }
            
            DWORD* dest = &pixels[screen_y * destWidth + screen_x];
            DWORD finalR = (r * alpha) / 255;
            DWORD finalG = (g * alpha) / 255;
            DWORD finalB = (b * alpha) / 255;
            *dest = (alpha << 24) | (finalR << 16) | (finalG << 8) | finalB;
        }
    }
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
        
        // Skip horizontal rule markers (they span full width, don't affect max width)
        if (text[i] == L'\x2500') continue;

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

/* Alert type colors (GitHub style) */
static const struct {
    BlockquoteAlertType type;
    COLORREF color;
    const wchar_t* icon;
} g_alertColors[] = {
    {BLOCKQUOTE_NOTE,      RGB(31, 136, 229),  L"\x24D8 "},  /* ⓘ Blue */
    {BLOCKQUOTE_TIP,       RGB(26, 127, 55),   L"\x1F4A1 "}, /* 💡 Green */
    {BLOCKQUOTE_IMPORTANT, RGB(130, 80, 223),  L"\x2139 "},  /* ℹ Purple */
    {BLOCKQUOTE_WARNING,   RGB(191, 135, 0),   L"\x26A0 "},  /* ⚠ Yellow */
    {BLOCKQUOTE_CAUTION,   RGB(207, 34, 46),   L"\x26D4 "},  /* ⛔ Red */
};

static COLORREF GetAlertColor(BlockquoteAlertType type) {
    for (int i = 0; i < sizeof(g_alertColors)/sizeof(g_alertColors[0]); i++) {
        if (g_alertColors[i].type == type) return g_alertColors[i].color;
    }
    return RGB(128, 128, 128);  /* Default gray for normal blockquote */
}

void RenderMarkdownSTB(void* bits, int width, int height, const wchar_t* text,
                       MarkdownLink* links, int linkCount,
                       MarkdownHeading* headings, int headingCount,
                       MarkdownStyle* styles, int styleCount,
                       MarkdownBlockquote* blockquotes, int blockquoteCount,
                       COLORREF color, int fontSize, float fontScale, int gradientMode) {
    if (!IsFontLoadedSTB() || !text || !bits) return;

    /* Clear previous clickable regions before rendering */
    ClearClickableRegions();

    stbtt_fontinfo* fontInfo = GetMainFontInfoSTB();
    stbtt_fontinfo* fallbackFontInfo = GetFallbackFontInfoSTB();
    BOOL fallbackLoaded = IsFallbackFontLoadedSTB();

    float baseScale = stbtt_ScaleForPixelHeight(fontInfo, (float)(fontSize * fontScale));
    float fallbackBaseScale = fallbackLoaded ? stbtt_ScaleForPixelHeight(fallbackFontInfo, (float)(fontSize * fontScale)) : 0;
    
    int baseAscent, baseDescent, baseLineGap;
    stbtt_GetFontVMetrics(fontInfo, &baseAscent, &baseDescent, &baseLineGap);

    /* Checkbox tracking */
    int checkboxIndex = 0;

    // Calculate total layout to center vertically
    int totalTextHeight = 0;
    int maxLineWidth = 0;
    MeasureMarkdownSTB(text, headings, headingCount, (int)(fontSize * fontScale), &maxLineWidth, &totalTextHeight);
    
    int currentY = (height - totalTextHeight) / 2;
    int blockLeftX = (width - maxLineWidth) / 2;  // Left edge of centered text block
    
    size_t len = wcslen(text);
    int currentLineStart = 0;
    
    // State trackers
    int curHeadingIdx = 0;
    int curLinkIdx = 0;
    int curStyleIdx = 0;
    int curBlockquoteIdx = 0;

    /* Calculate global time offset for animated gradient once per frame */
    int timeOffset = 0;
    if (IsGradientAnimated((GradientType)gradientMode)) {
        DWORD now = GetTickCount();
        float progress = (float)(now % 2000) / 2000.0f;
        timeOffset = (int)(progress * GRADIENT_LUT_SIZE * 2);
    }

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

            int currentX = blockLeftX;  // All lines start from same left edge
            // Baseline align: Y + maxAscent
            int baselineY = currentY + maxAscent;

            // Check for horizontal rule (─── = \x2500\x2500\x2500)
            if (i - currentLineStart >= 3 && 
                text[currentLineStart] == L'\x2500' && 
                text[currentLineStart + 1] == L'\x2500' && 
                text[currentLineStart + 2] == L'\x2500') {
                // Draw horizontal line within text block area only
                int lineY = currentY + lineMaxHeight / 2;
                DWORD* pixels = (DWORD*)bits;
                
                int hrLeft = blockLeftX;
                int hrRight = blockLeftX + maxLineWidth;
                int hrWidth = hrRight - hrLeft;
                
                const GradientInfo* hrGradInfo = (gradientMode != GRADIENT_NONE) ? 
                    GetGradientInfo((GradientType)gradientMode) : NULL;
                
                for (int x = hrLeft; x < hrRight && x < width; x++) {
                    if (lineY >= 0 && lineY < height && x >= 0) {
                        DWORD lineColor;
                        if (hrGradInfo && hrWidth > 0) {
                            float t = (float)(x - hrLeft) / (float)hrWidth;
                            int r, g, b;
                            
                            if (hrGradInfo->palette && hrGradInfo->paletteCount > 2) {
                                // Animated multi-color gradient
                                float animOffset = (float)timeOffset / (float)(GRADIENT_LUT_SIZE * 2);
                                t = t - animOffset;
                                while (t < 0) t += 1.0f;
                                while (t >= 1.0f) t -= 1.0f;
                                
                                float scaledT = t * (hrGradInfo->paletteCount - 1);
                                int idx1 = (int)scaledT;
                                int idx2 = idx1 + 1;
                                if (idx2 >= hrGradInfo->paletteCount) idx2 = 0;
                                float localT = scaledT - idx1;
                                
                                COLORREF c1 = hrGradInfo->palette[idx1];
                                COLORREF c2 = hrGradInfo->palette[idx2];
                                r = (int)(GetRValue(c1) + (GetRValue(c2) - GetRValue(c1)) * localT);
                                g = (int)(GetGValue(c1) + (GetGValue(c2) - GetGValue(c1)) * localT);
                                b = (int)(GetBValue(c1) + (GetBValue(c2) - GetBValue(c1)) * localT);
                            } else {
                                // 2-color static gradient using startColor/endColor
                                COLORREF c1 = hrGradInfo->startColor;
                                COLORREF c2 = hrGradInfo->endColor;
                                r = (int)(GetRValue(c1) + (GetRValue(c2) - GetRValue(c1)) * t);
                                g = (int)(GetGValue(c1) + (GetGValue(c2) - GetGValue(c1)) * t);
                                b = (int)(GetBValue(c1) + (GetBValue(c2) - GetBValue(c1)) * t);
                            }
                            lineColor = 0xFF000000 | (r << 16) | (g << 8) | b;
                        } else {
                            lineColor = 0xFF000000 | (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
                        }
                        pixels[lineY * width + x] = lineColor;
                    }
                }
                currentY += lineMaxHeight;
                currentLineStart = i + 1;
                continue;
            }

            // Check if this line is inside a blockquote
            while (curBlockquoteIdx < blockquoteCount && 
                   (int)currentLineStart >= blockquotes[curBlockquoteIdx].endPos) {
                curBlockquoteIdx++;
            }
            
            BlockquoteAlertType activeAlertType = BLOCKQUOTE_NORMAL;
            BOOL inBlockquote = FALSE;
            if (curBlockquoteIdx < blockquoteCount && 
                (int)currentLineStart >= blockquotes[curBlockquoteIdx].startPos) {
                inBlockquote = TRUE;
                activeAlertType = blockquotes[curBlockquoteIdx].alertType;
            }
            
            // Draw left colored bar for alert blockquotes only (GitHub style)
            if (inBlockquote && activeAlertType != BLOCKQUOTE_NORMAL) {
                COLORREF barColor = GetAlertColor(activeAlertType);
                DWORD barColorDW = 0xFF000000 | (GetRValue(barColor) << 16) | 
                                   (GetGValue(barColor) << 8) | GetBValue(barColor);
                DWORD* pixels = (DWORD*)bits;
                
                int barX = blockLeftX - 8;  // 8 pixels left of text
                int barWidth = 3;           // 3 pixels wide
                
                for (int y = currentY; y < currentY + lineMaxHeight && y < height; y++) {
                    if (y >= 0) {
                        for (int x = barX; x < barX + barWidth && x >= 0 && x < width; x++) {
                            pixels[y * width + x] = barColorDW;
                        }
                    }
                }
            }
            
            // Check if this is an alert title line (first line with "NOTE:", etc.)
            BOOL isAlertTitleLine = FALSE;
            if (inBlockquote && activeAlertType != BLOCKQUOTE_NORMAL) {
                const wchar_t* lineText = &text[currentLineStart];
                if (wcsstr(lineText, L"NOTE:") == lineText ||
                    wcsstr(lineText, L"TIP:") == lineText ||
                    wcsstr(lineText, L"IMPORTANT:") == lineText ||
                    wcsstr(lineText, L"WARNING:") == lineText ||
                    wcsstr(lineText, L"CAUTION:") == lineText) {
                    isAlertTitleLine = TRUE;
                }
            }
            
            // Check if this is a completed todo line (starts with ■)
            BOOL isCompletedTodo = (text[currentLineStart] == L'\x25A0');
            
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
                
                // Apply alert color only to title line (NOTE:, TIP:, etc.)
                if (isAlertTitleLine) {
                    drawColor = GetAlertColor(activeAlertType);
                }

                // Link - track region for click detection
                BOOL inLink = FALSE;
                int activeLinkIdx = -1;
                while (curLinkIdx < linkCount && j >= links[curLinkIdx].endPos) curLinkIdx++;
                if (curLinkIdx < linkCount && j >= links[curLinkIdx].startPos) {
                    drawColor = RGB(0, 175, 255); // Link color #00AFFF
                    inLink = TRUE;
                    activeLinkIdx = curLinkIdx;
                    
                    /* Update link rect for first char */
                    if (j == links[curLinkIdx].startPos) {
                        links[curLinkIdx].linkRect.left = currentX;
                        links[curLinkIdx].linkRect.top = currentY;
                        links[curLinkIdx].linkRect.bottom = currentY + lineMaxHeight;
                    }
                    links[curLinkIdx].linkRect.right = currentX;
                }
                
                // Style handling
                BOOL isBold = isAlertTitleLine;  // Alert title is bold
                BOOL isItalic = FALSE;
                BOOL isStrikethrough = FALSE;
                
                // Apply strikethrough for completed todo (skip checkbox symbol itself)
                if (isCompletedTodo && j > currentLineStart) {
                    isStrikethrough = TRUE;
                }
                
                while (curStyleIdx < styleCount && j >= styles[curStyleIdx].endPos) curStyleIdx++;
                if (curStyleIdx < styleCount && j >= styles[curStyleIdx].startPos) {
                    MarkdownStyleType styleType = styles[curStyleIdx].type;
                    if (styleType == STYLE_CODE) {
                        drawColor = RGB(100, 100, 100);
                    } else if (styleType == STYLE_BOLD) {
                        isBold = TRUE;
                    } else if (styleType == STYLE_ITALIC) {
                        isItalic = TRUE;
                    } else if (styleType == STYLE_BOLD_ITALIC) {
                        isBold = TRUE;
                        isItalic = TRUE;
                    } else if (styleType == STYLE_STRIKETHROUGH) {
                        isStrikethrough = TRUE;
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
                        float slant = isItalic ? 0.35f : 0.0f;
                        
                        if (gradientMode != GRADIENT_NONE && drawColor == color) {
                            if (isItalic) {
                                // Use proper per-row shear for italic with gradient
                                BlendCharBitmapItalicGradientSTB(bits, width, height, 
                                    currentX + xoff, baselineY + yoff, 
                                    bitmap, w, h, slant, gradientMode, timeOffset, width);
                                if (isBold) {
                                    BlendCharBitmapItalicGradientSTB(bits, width, height, 
                                        currentX + xoff + 1, baselineY + yoff, 
                                        bitmap, w, h, slant, gradientMode, timeOffset, width);
                                }
                            } else {
                                BlendCharBitmapGradientSTB(bits, width, height, 
                                    currentX + xoff, baselineY + yoff, 
                                    bitmap, w, h, 0, width, gradientMode, timeOffset);
                                if (isBold) {
                                    BlendCharBitmapGradientSTB(bits, width, height, 
                                        currentX + xoff + 1, baselineY + yoff, 
                                        bitmap, w, h, 0, width, gradientMode, timeOffset);
                                    BlendCharBitmapGradientSTB(bits, width, height, 
                                        currentX + xoff, baselineY + yoff + 1, 
                                        bitmap, w, h, 0, width, gradientMode, timeOffset);
                                }
                            }
                        } else if (isItalic) {
                            // Use proper per-row shear for italic
                            BlendCharBitmapItalicSTB(bits, width, height, 
                                currentX + xoff, baselineY + yoff,  
                                bitmap, w, h, 
                                GetRValue(drawColor), GetGValue(drawColor), GetBValue(drawColor), slant);
                            if (isBold) {
                                BlendCharBitmapItalicSTB(bits, width, height, 
                                    currentX + xoff + 1, baselineY + yoff,  
                                    bitmap, w, h, 
                                    GetRValue(drawColor), GetGValue(drawColor), GetBValue(drawColor), slant);
                            }
                        } else {
                            BlendCharBitmapSTB(bits, width, height, 
                                currentX + xoff, baselineY + yoff,  
                                bitmap, w, h, 
                                GetRValue(drawColor), GetGValue(drawColor), GetBValue(drawColor));
                            if (isBold) {
                                BlendCharBitmapSTB(bits, width, height, 
                                    currentX + xoff + 1, baselineY + yoff,  
                                    bitmap, w, h, 
                                    GetRValue(drawColor), GetGValue(drawColor), GetBValue(drawColor));
                                BlendCharBitmapSTB(bits, width, height, 
                                    currentX + xoff, baselineY + yoff + 1,  
                                    bitmap, w, h, 
                                    GetRValue(drawColor), GetGValue(drawColor), GetBValue(drawColor));
                            }
                        }
                        stbtt_FreeBitmap(bitmap, NULL);
                        
                        // Draw strikethrough line
                        if (isStrikethrough) {
                            int lineY = baselineY - h / 3;  // Position at ~1/3 from baseline
                            DWORD* pixels = (DWORD*)bits;
                            
                            // Get line color from gradient or solid color
                            DWORD lineColor;
                            if (gradientMode != GRADIENT_NONE && drawColor == color) {
                                const GradientInfo* stInfo = GetGradientInfo((GradientType)gradientMode);
                                if (stInfo && stInfo->palette && stInfo->paletteCount > 0) {
                                    COLORREF c = stInfo->palette[0];
                                    lineColor = 0xFF000000 | (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c);
                                } else if (stInfo) {
                                    lineColor = 0xFF000000 | (GetRValue(stInfo->startColor) << 16) | 
                                                (GetGValue(stInfo->startColor) << 8) | GetBValue(stInfo->startColor);
                                } else {
                                    lineColor = 0xFF000000 | (GetRValue(drawColor) << 16) | 
                                                (GetGValue(drawColor) << 8) | GetBValue(drawColor);
                                }
                            } else {
                                lineColor = 0xFF000000 | (GetRValue(drawColor) << 16) | 
                                            (GetGValue(drawColor) << 8) | GetBValue(drawColor);
                            }
                            
                            // Draw horizontal line through character
                            for (int sx = currentX; sx < currentX + gm.advance && sx < width; sx++) {
                                if (lineY >= 0 && lineY < height && sx >= 0) {
                                    pixels[lineY * width + sx] = lineColor;
                                }
                            }
                        }
                    }
                    /* Record checkbox region (□ = 0x25A1, ■ = 0x25A0) */
                    if (text[j] == L'\x25A1' || text[j] == L'\x25A0') {
                        /* Use character advance width for click area */
                        RECT cbRect = {
                            currentX, currentY,
                            currentX + gm.advance, currentY + lineMaxHeight
                        };
                        AddCheckboxRegion(&cbRect, checkboxIndex, text[j] == L'\x25A0');
                        checkboxIndex++;
                    }
                }
                currentX += gm.advance + gm.kern;
                
                /* Update link rect right edge after advancing */
                if (inLink && activeLinkIdx >= 0) {
                    links[activeLinkIdx].linkRect.right = currentX;
                }
            }

            currentY += lineMaxHeight;
            currentLineStart = i + 1;
        }
    }
    
    /* Register all link regions for click detection */
    for (int i = 0; i < linkCount; i++) {
        if (links[i].linkUrl && links[i].linkRect.right > links[i].linkRect.left) {
            AddLinkRegion(&links[i].linkRect, links[i].linkUrl);
        }
    }
}
