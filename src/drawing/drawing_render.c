/**
 * @file drawing_render.c
 * @brief GDI rendering pipeline with double-buffering
 */

#include <stdio.h>
#include <windows.h>
#include "drawing/drawing_render.h"
#include "drawing/drawing_time_format.h"
#include "drawing/drawing_text_stb.h"
#include "drawing/drawing_markdown_stb.h"
#include "drawing.h"
#include "font.h"
#include "color/color.h"
#include "timer/timer.h"
#include "config.h"
#include "window_procedure/window_procedure.h"
#include "menu_preview.h"
#include "font/font_path_manager.h"
#include "log.h"
#include "plugin/plugin_data.h"
#include "drawing/drawing_image.h"
#include "markdown/markdown_parser.h"
#include "markdown/markdown_image.h"
#include "color/gradient.h"
#include "color/color_parser.h"

extern char FONT_FILE_NAME[MAX_PATH];
extern char FONT_INTERNAL_NAME[MAX_PATH];
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];
extern int CLOCK_BASE_FONT_SIZE;
extern float CLOCK_FONT_SCALE_FACTOR;

/**
 * @param colorStr "#RRGGBB" or "R,G,B" format
 * @return COLORREF value, white on parse failure
 */
static COLORREF ParseColorString(const char* colorStr) {
    if (!colorStr || strlen(colorStr) == 0) {
        return RGB(255, 255, 255);
    }
    
    int r = 255, g = 255, b = 255;
    
    if (colorStr[0] == '#' && strlen(colorStr) == 7) {
        sscanf(colorStr + 1, "%02x%02x%02x", &r, &g, &b);
    } else {
        sscanf(colorStr, "%d,%d,%d", &r, &g, &b);
    }
    
    return RGB(r, g, b);
}

/**
 * @return Render context with preview or config settings
 * @note Static buffers avoid per-frame allocation
 */
static RenderContext CreateRenderContext(void) {
    RenderContext ctx;
    
    static char fontFileName[MAX_PATH];
    static char fontInternalName[MAX_PATH];
    static char colorStr[COLOR_HEX_BUFFER];
    
    extern void GetActiveFont(char*, char*, size_t);
    extern void GetActiveColor(char*, size_t);
    
    GetActiveFont(fontFileName, fontInternalName, sizeof(fontFileName));
    GetActiveColor(colorStr, sizeof(colorStr));
    
    ctx.fontFileName = fontFileName;
    ctx.fontInternalName = fontInternalName;
    ctx.textColor = ParseColorString(colorStr);
    ctx.fontScaleFactor = CLOCK_FONT_SCALE_FACTOR;
    
    ctx.gradientMode = (int)GetGradientTypeByName(colorStr);
    
    return ctx;
}

/**
 * @param ctx Font configuration
 * @return GDI font handle (must be deleted by caller)
 * @note Negative height = character height (not pixel height)
 */
static HFONT CreateTimerFont(const RenderContext* ctx) {
    wchar_t fontNameW[FONT_NAME_MAX_LEN];
    MultiByteToWideChar(CP_UTF8, 0, ctx->fontInternalName, -1, fontNameW, FONT_NAME_MAX_LEN);
    
    return CreateFontW(
        -(int)(CLOCK_BASE_FONT_SIZE * ctx->fontScaleFactor),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        VARIABLE_PITCH | FF_SWISS,
        fontNameW
    );
}

static BOOL ResolveFontPath(const RenderContext* ctx, char* outPath) {
    // Check if the configured path is a managed font path (starts with %LOCALAPPDATA% prefix)
    const char* relPath = ExtractRelativePath(ctx->fontFileName);
    if (relPath) {
        // It has the prefix, so extract the filename part and build full path
        return BuildFullFontPath(relPath, outPath, MAX_PATH);
    }
    
    // It might be a direct absolute path or a simple filename
    // First try to expand environment strings
    if (ExpandEnvironmentStringsA(ctx->fontFileName, outPath, MAX_PATH) > 0) {
        // If it doesn't contain a drive separator, assume it's a filename in fonts folder
        if (!strchr(outPath, ':')) {
            char simpleName[MAX_PATH];
            strcpy_s(simpleName, MAX_PATH, outPath);
            return BuildFullFontPath(simpleName, outPath, MAX_PATH);
        }
        return TRUE;
    }
    return FALSE;
}

static BOOL MeasureTextMarkdown(const wchar_t* text, const RenderContext* ctx, SIZE* outSize,
                               MarkdownHeading* headings, int headingCount) {
    char absoluteFontPath[MAX_PATH];
    if (ResolveFontPath(ctx, absoluteFontPath)) {
        if (InitFontSTB(absoluteFontPath)) {
            int w, h;
            if (MeasureMarkdownSTB(text, headings, headingCount, 
                                  (int)(CLOCK_BASE_FONT_SIZE * ctx->fontScaleFactor), &w, &h)) {
                outSize->cx = w;
                outSize->cy = h;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static BOOL RenderTextMarkdown(HDC hdc, const RECT* rect, const wchar_t* text, const RenderContext* ctx, BOOL editMode, void* bits,
                              MarkdownLink* links, int linkCount,
                              MarkdownHeading* headings, int headingCount,
                              MarkdownStyle* styles, int styleCount,
                              MarkdownBlockquote* blockquotes, int blockquoteCount) {
    // Use STB Truetype for high-quality rendering
    char absoluteFontPath[MAX_PATH];
    
    // Resolve font path to absolute path for STB
    if (ResolveFontPath(ctx, absoluteFontPath)) {
        if (InitFontSTB(absoluteFontPath)) {
            RenderMarkdownSTB(bits, rect->right, rect->bottom, text,
                             links, linkCount,
                             headings, headingCount,
                             styles, styleCount,
                             blockquotes, blockquoteCount,
                             ctx->textColor, 
                             (int)(CLOCK_BASE_FONT_SIZE * ctx->fontScaleFactor), 
                             1.0f,
                             ctx->gradientMode); // Internal scale is handled by font size
            return TRUE;
        }
    }

    return FALSE;
}

/** @note GM_ADVANCED + HALFTONE improve text quality on high-DPI displays */
static BOOL SetupDoubleBufferDIB(HDC hdc, const RECT* rect, HDC* memDC, HBITMAP* memBitmap, HBITMAP* oldBitmap, void** ppvBits) {
    *memDC = CreateCompatibleDC(hdc);
    if (!*memDC) {
        return FALSE;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = rect->right;
    // Negative height creates a top-down DIB, matching STB's coordinate system
    bmi.bmiHeader.biHeight = -rect->bottom;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    *memBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, ppvBits, NULL, 0);
    if (!*memBitmap) {
        DeleteDC(*memDC);
        return FALSE;
    }

    *oldBitmap = (HBITMAP)SelectObject(*memDC, *memBitmap);

    SetGraphicsMode(*memDC, GM_ADVANCED);
    SetBkMode(*memDC, TRANSPARENT);
    SetStretchBltMode(*memDC, HALFTONE);
    SetBrushOrgEx(*memDC, 0, 0, NULL);
    SetTextAlign(*memDC, TA_LEFT | TA_TOP);
    SetTextCharacterExtra(*memDC, 0);
    SetMapMode(*memDC, MM_TEXT);
    SetICMMode(*memDC, ICM_ON);
    SetLayout(*memDC, 0);

    return TRUE;
}

/** 
 * @brief Manually set alpha channel to opaque for non-black pixels
 * @details GDI text drawing leaves alpha channel as 0, which DWM treats as transparent.
 *          We iterate pixels to set Alpha=255 where RGB != 0.
 */
static void FixAlphaChannel(void* bits, int width, int height) {
    if (!bits) return;
    
    DWORD* pixels = (DWORD*)bits;
    int count = width * height;
    
    for (int i = 0; i < count; i++) {
        // Check if RGB is not black (0x00RRGGBB)
        if ((pixels[i] & 0x00FFFFFF) != 0) {
            // Only set Alpha to 255 if it's currently 0 (meaning it was drawn by GDI without alpha)
            if ((pixels[i] & 0xFF000000) == 0) {
                pixels[i] |= 0xFF000000;
            }
        } else {
            // Ensure black background is transparent
            pixels[i] &= 0x00FFFFFF;
        }
    }
}

/** @note Skips resize if size unchanged to reduce SetWindowPos overhead */
static void AdjustWindowSize(HWND hwnd, const SIZE* textSize, RECT* rect) {
    if (textSize->cx == (rect->right - rect->left) && 
        textSize->cy == (rect->bottom - rect->top)) {
        return;
    }
    
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    
    SetWindowPos(hwnd, NULL,
        windowRect.left, windowRect.top,
        textSize->cx + WINDOW_HORIZONTAL_PADDING,
        textSize->cy + WINDOW_VERTICAL_PADDING,
        SWP_NOZORDER | SWP_NOACTIVATE);
    
    GetClientRect(hwnd, rect);
}

// Global flag to suppress rendering during mode transitions
BOOL g_IsTransitioning = FALSE;

void HandleWindowPaint(HWND hwnd, PAINTSTRUCT* ps) {
    wchar_t timeText[TIME_TEXT_MAX_LEN];
    HDC hdc = ps->hdc;
    RECT rect;
    GetClientRect(hwnd, &rect);

    // If transitioning, skip text generation to avoid artifacts
    // We still need to clear the window to transparent, so we proceed to SetupDoubleBufferDIB
    // but we will skip RenderText later.
    
    GetTimeText(timeText, TIME_TEXT_MAX_LEN);

    // Check for plugin data
    wchar_t pluginText[TIME_TEXT_MAX_LEN] = {0};
    MarkdownImage* images = NULL;
    int imageCount = 0;
    
    if (PluginData_GetText(pluginText, TIME_TEXT_MAX_LEN)) {
        // Get current time text once
        wchar_t savedTime[256];
        GetTimeText(savedTime, 256);
        
        // First pass: extract images and count them
        int imgCapacity = CountMarkdownImages(pluginText);
        if (imgCapacity > 0) {
            images = (MarkdownImage*)calloc(imgCapacity, sizeof(MarkdownImage));
        }
        
        // Replace ALL <catime></catime> tags and extract ![](path) images
        wchar_t result[TIME_TEXT_MAX_LEN] = {0};
        wchar_t* src = pluginText;
        wchar_t* dst = result;
        size_t remaining = TIME_TEXT_MAX_LEN - 1;
        
        while (*src && remaining > 0) {
            // Check for image tag first: ![...](...)
            if (*src == L'!' && *(src + 1) == L'[' && images && imageCount < imgCapacity) {
                const wchar_t* imgSrc = src;
                if (ExtractMarkdownImage(&imgSrc, images, &imageCount, imgCapacity, (int)(dst - result))) {
                    src = (wchar_t*)imgSrc;
                    continue;
                }
            }
            
            // Check for <catime> tag
            wchar_t* tagStart = wcsstr(src, L"<catime>");
            wchar_t* tagEnd = tagStart ? wcsstr(tagStart, L"</catime>") : NULL;
            
            if (tagStart && tagEnd && tagEnd > tagStart && tagStart == src) {
                // Insert time text
                size_t timeLen = wcslen(savedTime);
                if (timeLen > remaining) timeLen = remaining;
                wcsncpy(dst, savedTime, timeLen);
                dst += timeLen;
                remaining -= timeLen;
                
                // Move past </catime>
                src = tagEnd + 9;
            } else if (tagStart && tagEnd && tagEnd > tagStart) {
                // Copy text before tag
                size_t beforeLen = tagStart - src;
                if (beforeLen > remaining) beforeLen = remaining;
                wcsncpy(dst, src, beforeLen);
                dst += beforeLen;
                remaining -= beforeLen;
                
                // Insert time text
                size_t timeLen = wcslen(savedTime);
                if (timeLen > remaining) timeLen = remaining;
                wcsncpy(dst, savedTime, timeLen);
                dst += timeLen;
                remaining -= timeLen;
                
                // Move past </catime>
                src = tagEnd + 9;
            } else {
                // Copy single character
                *dst++ = *src++;
                remaining--;
            }
        }
        *dst = L'\0';
        
        wcscpy_s(timeText, TIME_TEXT_MAX_LEN, result);
    }

    if (wcslen(timeText) == 0) {
        GetPreviewTimeText(timeText, TIME_TEXT_MAX_LEN);
    }

    RenderContext ctx = CreateRenderContext();
    HFONT hFont = CreateTimerFont(&ctx);

    // Parse Markdown
    wchar_t* mdText = NULL;
    MarkdownLink* links = NULL; int linkCount = 0;
    MarkdownHeading* headings = NULL; int headingCount = 0;
    MarkdownStyle* styles = NULL; int styleCount = 0;
    MarkdownListItem* listItems = NULL; int listItemCount = 0;
    MarkdownBlockquote* blockquotes = NULL; int blockquoteCount = 0;

    BOOL isMarkdown = ParseMarkdownLinks(timeText, &mdText, 
                                         &links, &linkCount, 
                                         &headings, &headingCount, 
                                         &styles, &styleCount,
                                         &listItems, &listItemCount,
                                         &blockquotes, &blockquoteCount);
                                         
    const wchar_t* textToRender = isMarkdown ? mdText : timeText;

    // Measure text and resize window BEFORE creating the buffer
    // This prevents buffer overflow if the window grows
    SIZE textSize = {0};
    BOOL hasContent = (wcslen(textToRender) > 0) || (images && imageCount > 0);
    
    if (hasContent) {
        BOOL measured = FALSE;
        
        // Measure text if any
        if (wcslen(textToRender) > 0) {
            if (isMarkdown) {
                measured = MeasureTextMarkdown(textToRender, &ctx, &textSize, headings, headingCount);
            } else {
                measured = MeasureTextMarkdown(textToRender, &ctx, &textSize, NULL, 0);
            }

            if (!measured) {
                HFONT oldFontHdc = (HFONT)SelectObject(hdc, hFont);
                GetTextExtentPoint32W(hdc, textToRender, (int)wcslen(textToRender), &textSize);
                SelectObject(hdc, oldFontHdc);
            }
        }
        
        // Add image dimensions to total size
        if (images && imageCount > 0) {
            textSize.cy += 5;  // Small gap between text and first image
            
            for (int i = 0; i < imageCount; i++) {
                int renderW = 0, renderH = 0;
                // Use large max values to get natural scaled size (not constrained by window)
                if (CalculateImageRenderSize(&images[i], 10000, 10000, &renderW, &renderH)) {
                    renderW += 10;  // Add padding
                    renderH += 5;
                    
                    if (renderW > textSize.cx) textSize.cx = renderW;
                    textSize.cy += renderH;
                } else if (images[i].isNetworkImage && !images[i].isDownloaded) {
                    // Reserve space for "Loading..." text
                    textSize.cy += 25;  // Approximate height for loading text
                }
            }
        }

        AdjustWindowSize(hwnd, &textSize, &rect);
    }
    
    HDC memDC;
    HBITMAP memBitmap, oldBitmap;
    void* pBits = NULL;
    
    // Create buffer with the final correct size
    if (!SetupDoubleBufferDIB(hdc, &rect, &memDC, &memBitmap, &oldBitmap, &pBits)) {
        DeleteObject(hFont);
        if (isMarkdown) {
            FreeMarkdownLinks(links, linkCount);
            free(headings); free(styles); free(listItems); free(blockquotes);
            free(mdText);
        }
        if (images) {
            FreeMarkdownImages(images, imageCount);
        }
        return;
    }
    
    // Select font into memDC for drawing
    HFONT oldFontMem = (HFONT)SelectObject(memDC, hFont);
    
    // Manually clear background
    // Edit Mode: Alpha=5 to capture mouse click on background
    // Normal Mode: Alpha=0 for full transparency (clickable regions filled later)
    int numPixels = rect.right * rect.bottom;
    DWORD* pixels = (DWORD*)pBits;
    DWORD clearColor = CLOCK_EDIT_MODE ? 0x05000000 : 0x00000000;
    
    // Simple loop is fast enough for small window
    for (int i = 0; i < numPixels; i++) {
        pixels[i] = clearColor;
    }
    
    // Skip rendering during transition to avoid black artifacts
    if (!g_IsTransitioning && hasContent) {
        BOOL usedSTB = FALSE;
        int textHeight = 0;
        
        // Render text if any
        if (wcslen(textToRender) > 0) {
            // Get text height first
            SIZE textOnlySize = {0};
            MeasureTextMarkdown(textToRender, &ctx, &textOnlySize, headings, headingCount);
            textHeight = textOnlySize.cy;
            
            // Create adjusted rect for text rendering (only text area, not including images)
            RECT textRect = rect;
            if (images && imageCount > 0) {
                // Set text area to exactly text height (no extra space = no centering offset)
                textRect.bottom = textHeight;
            }
            
            if (isMarkdown) {
                usedSTB = RenderTextMarkdown(memDC, &textRect, textToRender, &ctx, CLOCK_EDIT_MODE, pBits,
                                            links, linkCount, headings, headingCount, styles, styleCount,
                                            blockquotes, blockquoteCount);
            } else {
                usedSTB = RenderTextMarkdown(memDC, &textRect, textToRender, &ctx, CLOCK_EDIT_MODE, pBits,
                                            NULL, 0, NULL, 0, NULL, 0, NULL, 0);
            }
        }
        
        // If STB was not used (e.g. font load failure), we might need to fix alpha for GDI text
        if (!usedSTB && CLOCK_EDIT_MODE && wcslen(textToRender) > 0) {
             FixAlphaChannel(pBits, rect.right, rect.bottom);
        }
        
        // Fill clickable regions with minimal alpha for mouse hit-testing (non-edit mode only)
        if (!CLOCK_EDIT_MODE) {
            extern void FillClickableRegionsAlpha(DWORD* pixels, int width, int height);
            FillClickableRegionsAlpha(pixels, rect.right, rect.bottom);
        }
        
        // Render images below text (centered horizontally like text)
        if (images && imageCount > 0) {
            int imgY = textHeight > 0 ? textHeight + 5 : 5;
            int maxW = rect.right - 10;
            if (maxW <= 0) maxW = rect.right;  // Fallback if window too narrow
            
            for (int i = 0; i < imageCount; i++) {
                int maxH = rect.bottom - imgY - 5;
                if (maxH <= 0) break;  // No more space for images
                
                // Check if network image needs async download
                if (images[i].isNetworkImage && !images[i].isDownloaded && !images[i].isDownloading) {
                    StartAsyncImageDownload(&images[i], hwnd);
                }
                
                // If downloading, show "Loading..." text
                if (images[i].isDownloading || (images[i].isNetworkImage && !images[i].isDownloaded)) {
                    // Draw "Loading..." centered with same color as text
                    const wchar_t* loadingText = L"Loading...";
                    SetBkMode(memDC, TRANSPARENT);
                    SetTextColor(memDC, ParseColorString(CLOCK_TEXT_COLOR));
                    SIZE textSize;
                    GetTextExtentPoint32W(memDC, loadingText, (int)wcslen(loadingText), &textSize);
                    int textX = (rect.right - textSize.cx) / 2;
                    TextOutW(memDC, textX, imgY, loadingText, (int)wcslen(loadingText));
                    imgY += textSize.cy + 5;
                    continue;
                }
                
                // Get render size for centering
                int imgRenderW = 0, imgRenderH = 0;
                if (!CalculateImageRenderSize(&images[i], maxW, maxH, &imgRenderW, &imgRenderH)) {
                    continue;  // Skip this image if calculation fails
                }
                
                // Center horizontally
                int imgX = (rect.right - imgRenderW) / 2;
                if (imgX < 5) imgX = 5;
                
                int imgHeight = RenderMarkdownImage(memDC, &images[i], imgX, imgY, maxW, maxH);
                if (imgHeight > 0) {
                    imgY += imgHeight + 5;
                }
            }
        }
    } else if (CLOCK_EDIT_MODE) {
        FixAlphaChannel(pBits, rect.right, rect.bottom);
    }
    
    // Free markdown resources
    if (isMarkdown) {
        FreeMarkdownLinks(links, linkCount);
        free(headings); free(styles); free(listItems); free(blockquotes);
        free(mdText);
    }
    
    // Free image resources
    if (images) {
        FreeMarkdownImages(images, imageCount);
    }
    
    HDC hdcScreen = GetDC(NULL);
    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {rect.right, rect.bottom};
    POINT ptDst = {0, 0};
    
    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);
    ptDst.x = rcWindow.left;
    ptDst.y = rcWindow.top;
    
    extern int CLOCK_WINDOW_OPACITY;
    BYTE alpha = (BYTE)((CLOCK_WINDOW_OPACITY * 255) / 100);
    
    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = alpha;
    blend.AlphaFormat = AC_SRC_ALPHA;

    if (!UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA)) {
        DWORD err = GetLastError();
        if (err == ERROR_INVALID_PARAMETER) {
            // Error 87 often implies conflict between SetLayeredWindowAttributes and UpdateLayeredWindow
            // Reset WS_EX_LAYERED style to clear the internal state
            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
            
            // Retry update
            if (!UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA)) {
                err = GetLastError();
                WriteLog(LOG_LEVEL_ERROR, "UpdateLayeredWindow failed retry! Error code: %lu", err);
            }
        } else {
            WriteLog(LOG_LEVEL_ERROR, "UpdateLayeredWindow failed! Error code: %lu", err);
        }
    }
    
    ReleaseDC(NULL, hdcScreen);
    
    SelectObject(memDC, oldFontMem);
    DeleteObject(hFont);
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

