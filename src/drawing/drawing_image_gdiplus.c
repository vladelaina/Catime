#include "drawing/drawing_image.h"
#include "log.h"

/* GDI+ Flat API definitions for C compatibility */
#ifndef ULONG_PTR
#define ULONG_PTR unsigned long*
#endif

typedef void* GpGraphics;
typedef void* GpBitmap;
typedef void* GpImage;
typedef int GpStatus;

#define Ok 0

typedef struct GdiplusStartupInput {
    UINT32 GdiplusVersion;
    void* DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInput;

/* Function Pointers */
typedef GpStatus (WINAPI *FuncGdiplusStartup)(ULONG_PTR*, const GdiplusStartupInput*, void*);
typedef void (WINAPI *FuncGdiplusShutdown)(ULONG_PTR);
typedef GpStatus (WINAPI *FuncGdipCreateFromHDC)(HDC, GpGraphics*);
typedef GpStatus (WINAPI *FuncGdipDeleteGraphics)(GpGraphics);
typedef GpStatus (WINAPI *FuncGdipCreateBitmapFromFile)(const WCHAR*, GpBitmap*);
typedef GpStatus (WINAPI *FuncGdipDisposeImage)(GpImage);
typedef GpStatus (WINAPI *FuncGdipDrawImageRectI)(GpGraphics, GpImage, INT, INT, INT, INT);
typedef GpStatus (WINAPI *FuncGdipGetImageWidth)(GpImage, UINT*);
typedef GpStatus (WINAPI *FuncGdipGetImageHeight)(GpImage, UINT*);

/* Globals */
static HMODULE g_hGdiPlus = NULL;
static ULONG_PTR g_gdiplusToken = 0;

static FuncGdiplusStartup pGdiplusStartup = NULL;
static FuncGdiplusShutdown pGdiplusShutdown = NULL;
static FuncGdipCreateFromHDC pGdipCreateFromHDC = NULL;
static FuncGdipDeleteGraphics pGdipDeleteGraphics = NULL;
static FuncGdipCreateBitmapFromFile pGdipCreateBitmapFromFile = NULL;
static FuncGdipDisposeImage pGdipDisposeImage = NULL;
static FuncGdipDrawImageRectI pGdipDrawImageRectI = NULL;
static FuncGdipGetImageWidth pGdipGetImageWidth = NULL;
static FuncGdipGetImageHeight pGdipGetImageHeight = NULL;

void InitDrawingImage(void) {
    if (g_hGdiPlus) return;

    g_hGdiPlus = LoadLibraryA("gdiplus.dll");
    if (!g_hGdiPlus) {
        LOG_ERROR("Failed to load gdiplus.dll");
        return;
    }

    pGdiplusStartup = (FuncGdiplusStartup)GetProcAddress(g_hGdiPlus, "GdiplusStartup");
    pGdiplusShutdown = (FuncGdiplusShutdown)GetProcAddress(g_hGdiPlus, "GdiplusShutdown");
    pGdipCreateFromHDC = (FuncGdipCreateFromHDC)GetProcAddress(g_hGdiPlus, "GdipCreateFromHDC");
    pGdipDeleteGraphics = (FuncGdipDeleteGraphics)GetProcAddress(g_hGdiPlus, "GdipDeleteGraphics");
    pGdipCreateBitmapFromFile = (FuncGdipCreateBitmapFromFile)GetProcAddress(g_hGdiPlus, "GdipCreateBitmapFromFile");
    pGdipDisposeImage = (FuncGdipDisposeImage)GetProcAddress(g_hGdiPlus, "GdipDisposeImage");
    pGdipDrawImageRectI = (FuncGdipDrawImageRectI)GetProcAddress(g_hGdiPlus, "GdipDrawImageRectI");
    pGdipGetImageWidth = (FuncGdipGetImageWidth)GetProcAddress(g_hGdiPlus, "GdipGetImageWidth");
    pGdipGetImageHeight = (FuncGdipGetImageHeight)GetProcAddress(g_hGdiPlus, "GdipGetImageHeight");

    if (pGdiplusStartup) {
        GdiplusStartupInput input = {1, NULL, FALSE, FALSE};
        if (pGdiplusStartup(&g_gdiplusToken, &input, NULL) != Ok) {
            LOG_ERROR("GdiplusStartup failed");
            g_gdiplusToken = 0;
        } else {
            LOG_INFO("GDI+ initialized successfully");
        }
    }
}


void ShutdownDrawingImage(void) {
    if (g_gdiplusToken && pGdiplusShutdown) {
        pGdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
    if (g_hGdiPlus) {
        FreeLibrary(g_hGdiPlus);
        g_hGdiPlus = NULL;
    }
}

BOOL RenderImageGDIPlus(HDC hdc, int x, int y, int width, int height, const wchar_t* imagePath) {
    if (!g_gdiplusToken || !hdc || !imagePath) return FALSE;

    GpBitmap bitmap = NULL;
    if (pGdipCreateBitmapFromFile(imagePath, &bitmap) != Ok || !bitmap) {
        // Silent failure is okay if file not found/locked
        return FALSE;
    }

    // Get graphics from HDC
    GpGraphics graphics = NULL;
    if (pGdipCreateFromHDC(hdc, &graphics) == Ok && graphics) {
        
        UINT imgW = 0, imgH = 0;
        pGdipGetImageWidth((GpImage)bitmap, &imgW);
        pGdipGetImageHeight((GpImage)bitmap, &imgH);

        if (imgW > 0 && imgH > 0 && width > 0 && height > 0) {
            // Calculate scale to fit (Contain)
            float scaleX = (float)width / imgW;
            float scaleY = (float)height / imgH;
            float scale = (scaleX < scaleY) ? scaleX : scaleY;

            int drawW = (int)(imgW * scale);
            int drawH = (int)(imgH * scale);

            // Center the image
            int drawX = x + (width - drawW) / 2;
            int drawY = y + (height - drawH) / 2;

            // Draw
            pGdipDrawImageRectI(graphics, (GpImage)bitmap, drawX, drawY, drawW, drawH);
        }
        
        pGdipDeleteGraphics(graphics);
    }

    pGdipDisposeImage((GpImage)bitmap);
    return TRUE;
}
