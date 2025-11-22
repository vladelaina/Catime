/**
 * @file drawing_render.c
 * @brief GDI rendering pipeline with double-buffering
 */

#include <stdio.h>
#include <windows.h>
#include "drawing/drawing_render.h"
#include "drawing/drawing_time_format.h"
#include "drawing/drawing_text_stb.h" // Added STB header
#include "drawing.h"
#include "font.h"
#include "color/color.h"
#include "timer/timer.h"
#include "config.h"
#include "window_procedure/window_procedure.h"
#include "menu_preview.h"
#include "font/font_path_manager.h" // Added for path resolution
#include "log.h" // Added logging

extern char FONT_FILE_NAME[MAX_PATH];
extern char FONT_INTERNAL_NAME[MAX_PATH];
extern char CLOCK_TEXT_COLOR[10];
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
    
    static char fontFileName[100];
    static char fontInternalName[100];
    static char colorStr[10];
    
    extern void GetActiveFont(char*, char*, size_t);
    extern void GetActiveColor(char*, size_t);
    
    GetActiveFont(fontFileName, fontInternalName, sizeof(fontFileName));
    GetActiveColor(colorStr, sizeof(colorStr));
    
    ctx.fontFileName = fontFileName;
    ctx.fontInternalName = fontInternalName;
    ctx.textColor = ParseColorString(colorStr);
    ctx.fontScaleFactor = CLOCK_FONT_SCALE_FACTOR;
    
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

/** @param editMode TRUE for border to indicate edit mode while keeping background transparent for acrylic effect
 */
static void FillBackground(HDC hdc, const RECT* rect, BOOL editMode) {
    // Always fill with black first (which will be transparent via color key)
    HBRUSH hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, rect, hBlackBrush);
    DeleteObject(hBlackBrush);
}


/**
 * @brief Draw advanced glass lighting effects (highlights and shadows) directly to DIB
 * @param bits Pointer to DIB pixel data
 * @param width Bitmap width
 * @param height Bitmap height
 * @param radius Corner radius
 */
static void DrawGlassLighting(void* bits, int width, int height, int radius) {
    if (!bits || width <= 0 || height <= 0) return;
    
    DWORD* pixels = (DWORD*)bits;
    
    // Helper macro to blend pixel with color
    // We use a simple alpha blend: New = (Source * Alpha + Dest * (255 - Alpha)) / 255
    #define BLEND_PIXEL(x, y, r, g, b, a) \
        do { \
            int _px = (x); int _py = (y); \
            if (_px >= 0 && _px < width && _py >= 0 && _py < height) { \
                DWORD _dest = pixels[_py * width + _px]; \
                BYTE _da = (_dest >> 24) & 0xFF; \
                BYTE _dr = (_dest >> 16) & 0xFF; \
                BYTE _dg = (_dest >> 8) & 0xFF; \
                BYTE _db = (_dest) & 0xFF; \
                \
                /* If dest alpha is 0, just overwrite (it's transparent background) */ \
                /* Otherwise blend. For glass border, we usually want to add to it. */ \
                \
                DWORD _newR = ((r) * (a) + _dr * (255 - (a))) / 255; \
                DWORD _newG = ((g) * (a) + _dg * (255 - (a))) / 255; \
                DWORD _newB = ((b) * (a) + _db * (255 - (a))) / 255; \
                DWORD _newA = (_da > (a)) ? _da : (a); /* Keep max alpha or blend? Let's just set it for border */ \
                _newA = (a) + _da - ((a) * _da) / 255; /* Standard alpha composite */ \
                 \
                /* Force high alpha for the border itself to be visible against blur */ \
                if (_newA < (a)) _newA = (a); \
                \
                pixels[_py * width + _px] = (_newA << 24) | (_newR << 16) | (_newG << 8) | _newB; \
            } \
        } while(0)

    // 1. Top-Left Highlight (White, fading)
    // Simulates light coming from top-left
    for (int i = 0; i < width; i++) {
        // Top edge gradient (fades out towards right)
        int alpha = 150 - (i * 150 / width); 
        if (alpha < 0) alpha = 0;
        if (i < radius || i > width - radius) continue; // Skip corners for now
        BLEND_PIXEL(i, 0, 255, 255, 255, alpha);
        BLEND_PIXEL(i, 1, 255, 255, 255, alpha / 2); // Softer inner edge
    }
    
    for (int i = 0; i < height; i++) {
        // Left edge gradient (fades out towards bottom)
        int alpha = 150 - (i * 150 / height);
        if (alpha < 0) alpha = 0;
        if (i < radius || i > height - radius) continue;
        BLEND_PIXEL(0, i, 255, 255, 255, alpha);
        BLEND_PIXEL(1, i, 255, 255, 255, alpha / 2);
    }

    // 2. Bottom-Right Shadow/Refraction (Darker/Subtle)
    // Simulates thickness/occlusion
    for (int i = radius; i < width - radius; i++) {
        int alpha = (i * 100 / width); // Fades in towards right
        if (alpha > 100) alpha = 100;
        BLEND_PIXEL(i, height - 1, 200, 200, 200, alpha); // Light gray reflection
    }
    
    for (int i = radius; i < height - radius; i++) {
        int alpha = (i * 100 / height);
        if (alpha > 100) alpha = 100;
        BLEND_PIXEL(width - 1, i, 200, 200, 200, alpha);
    }
    
    // 3. Corner Highlights (Simplified)
    // Top-Left Corner (Brightest)
    for (int y = 0; y < radius; y++) {
        for (int x = 0; x < radius; x++) {
            int dx = radius - x;
            int dy = radius - y;
            // Outer rim
            if (dx*dx + dy*dy <= radius*radius && dx*dx + dy*dy >= (radius-1.5)*(radius-1.5)) {
                 BLEND_PIXEL(x, y, 255, 255, 255, 180);
            }
        }
    }
    
    // Bottom-Right Corner (Reflective)
    int cx = width - radius;
    int cy = height - radius;
    for (int y = 0; y < radius; y++) {
        for (int x = 0; x < radius; x++) {
            int dx = x;
            int dy = y;
             if (dx*dx + dy*dy <= radius*radius && dx*dx + dy*dy >= (radius-1.5)*(radius-1.5)) {
                 BLEND_PIXEL(cx + x, cy + y, 220, 220, 220, 80);
            }
        }
    }

    #undef BLEND_PIXEL
}



/** @note Multiple passes simulate bold when font lacks native bold variant */
static void RenderTextBold(HDC hdc, const wchar_t* text, int x, int y, COLORREF color) {
    size_t textLen = wcslen(text);
    
    // Prevent pure black text from becoming transparent
    if (color == RGB(0, 0, 0)) {
        color = RGB(1, 1, 1);
    }
    
    SetTextColor(hdc, color);
    
    for (int i = 0; i < TEXT_RENDER_PASSES; i++) {
        TextOutW(hdc, x, y, text, (int)textLen);
    }
}

static BOOL RenderText(HDC hdc, const RECT* rect, const wchar_t* text, const RenderContext* ctx, BOOL editMode, void* bits) {
    // Use STB Truetype for high-quality rendering
    char absoluteFontPath[MAX_PATH];
    BOOL pathResolved = FALSE;
    
    // Check if the configured path is a managed font path (starts with %LOCALAPPDATA% prefix)
    const char* relPath = ExtractRelativePath(ctx->fontFileName);
    if (relPath) {
        // It has the prefix, so extract the filename part and build full path
        pathResolved = BuildFullFontPath(relPath, absoluteFontPath, MAX_PATH);
    } else {
        // It might be a direct absolute path or a simple filename
        // First try to expand environment strings
        if (ExpandEnvironmentStringsA(ctx->fontFileName, absoluteFontPath, MAX_PATH) > 0) {
            // If it doesn't contain a drive separator, assume it's a filename in fonts folder
            if (!strchr(absoluteFontPath, ':')) {
                char simpleName[MAX_PATH];
                strcpy(simpleName, absoluteFontPath);
                pathResolved = BuildFullFontPath(simpleName, absoluteFontPath, MAX_PATH);
            } else {
                pathResolved = TRUE;
            }
        }
    }
    
    // Resolve font path to absolute path for STB
    if (pathResolved) {
        if (InitFontSTB(absoluteFontPath)) {
            // Clear the area where text will be drawn if needed, 
            // but FillBackground already did that.
            
            RenderTextSTB(bits, rect->right, rect->bottom, text, 
                         ctx->textColor, 
                         (int)(CLOCK_BASE_FONT_SIZE * ctx->fontScaleFactor), 
                         1.0f, // Internal scale is handled by font size
                         editMode);
            return TRUE;
        } else {
            WriteLog(LOG_LEVEL_ERROR, "InitFontSTB failed for path: %s", absoluteFontPath);
        }
    } else {
        WriteLog(LOG_LEVEL_ERROR, "Failed to resolve font path: %s", ctx->fontFileName);
    }

    // Force STB only - return FALSE if STB fails
    // This allows testing the pure STB implementation
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

    if (wcslen(timeText) == 0) {
        GetPreviewTimeText(timeText, TIME_TEXT_MAX_LEN);
    }

    RenderContext ctx = CreateRenderContext();
    HFONT hFont = CreateTimerFont(&ctx);

    // Measure text and resize window BEFORE creating the buffer
    // This prevents buffer overflow if the window grows
    if (wcslen(timeText) > 0) {
        HFONT oldFontHdc = (HFONT)SelectObject(hdc, hFont);
        SIZE textSize;
        GetTextExtentPoint32W(hdc, timeText, (int)wcslen(timeText), &textSize);
        SelectObject(hdc, oldFontHdc);

        AdjustWindowSize(hwnd, &textSize, &rect);
    }
    
    HDC memDC;
    HBITMAP memBitmap, oldBitmap;
    void* pBits = NULL;
    
    // Create buffer with the final correct size
    if (!SetupDoubleBufferDIB(hdc, &rect, &memDC, &memBitmap, &oldBitmap, &pBits)) {
        DeleteObject(hFont);
        return;
    }
    
    // Select font into memDC for drawing
    HFONT oldFontMem = (HFONT)SelectObject(memDC, hFont);
    
    // Manually clear background
    // Edit Mode: Alpha=1 to capture mouse click on background
    // Normal Mode: Alpha=0 for full transparency
    int numPixels = rect.right * rect.bottom;
    DWORD* pixels = (DWORD*)pBits;
    DWORD clearColor = CLOCK_EDIT_MODE ? 0x01000000 : 0x00000000;
    
    // Simple loop is fast enough for small window
    for (int i = 0; i < numPixels; i++) {
        pixels[i] = clearColor;
    }
    
    // Skip text rendering during transition to avoid black artifacts
    if (!g_IsTransitioning && wcslen(timeText) > 0) {
        // We already have textSize and rect is updated
        BOOL usedSTB = RenderText(memDC, &rect, timeText, &ctx, CLOCK_EDIT_MODE, pBits);
        
        // If STB was not used (e.g. font load failure), we might need to fix alpha for GDI text
        if (!usedSTB && CLOCK_EDIT_MODE) {
             FixAlphaChannel(pBits, rect.right, rect.bottom);
        }
    } else if (CLOCK_EDIT_MODE) {
        FixAlphaChannel(pBits, rect.right, rect.bottom);
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

