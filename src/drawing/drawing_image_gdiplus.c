#include "drawing/drawing_image.h"
#include "log.h"
#include <string.h>

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

#define LOAD_GDIPLUS_PROC(module, name, target) \
    do { \
        FARPROC proc = GetProcAddress((module), (name)); \
        memcpy(&(target), &proc, sizeof(target)); \
    } while (0)

#define MAX_CACHED_IMAGES 16

typedef struct {
    BOOL inUse;
    wchar_t path[MAX_PATH];
    FILETIME lastWriteTime;
    GpBitmap bitmap;
    UINT width;
    UINT height;
    DWORD lastAccessTick;
} CachedImageEntry;

static CachedImageEntry g_imageCache[MAX_CACHED_IMAGES] = {0};

static void ReleaseCachedImageEntry(CachedImageEntry* entry) {
    if (!entry) return;
    if (entry->bitmap && pGdipDisposeImage) {
        pGdipDisposeImage((GpImage)entry->bitmap);
    }
    ZeroMemory(entry, sizeof(*entry));
}

static BOOL GetImageWriteTime(const wchar_t* imagePath, FILETIME* writeTime) {
    if (!imagePath || !writeTime) return FALSE;

    HANDLE hFile = CreateFileW(imagePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    BOOL ok = GetFileTime(hFile, NULL, NULL, writeTime);
    CloseHandle(hFile);
    return ok;
}

static CachedImageEntry* GetCachedImageEntry(const wchar_t* imagePath) {
    if (!g_gdiplusToken || !imagePath || !pGdipCreateBitmapFromFile ||
        !pGdipGetImageWidth || !pGdipGetImageHeight) {
        return NULL;
    }

    FILETIME writeTime = {0};
    if (!GetImageWriteTime(imagePath, &writeTime)) {
        return NULL;
    }

    CachedImageEntry* freeSlot = NULL;
    CachedImageEntry* lruSlot = NULL;
    for (int i = 0; i < MAX_CACHED_IMAGES; i++) {
        CachedImageEntry* entry = &g_imageCache[i];
        if (!entry->inUse) {
            if (!freeSlot) freeSlot = entry;
            continue;
        }

        if (wcscmp(entry->path, imagePath) == 0) {
            if (CompareFileTime(&entry->lastWriteTime, &writeTime) == 0 && entry->bitmap) {
                entry->lastAccessTick = GetTickCount();
                return entry;
            }

            ReleaseCachedImageEntry(entry);
            freeSlot = entry;
            break;
        }

        if (!lruSlot || entry->lastAccessTick < lruSlot->lastAccessTick) {
            lruSlot = entry;
        }
    }

    CachedImageEntry* target = freeSlot ? freeSlot : lruSlot;
    if (!target) {
        return NULL;
    }

    if (target->inUse) {
        ReleaseCachedImageEntry(target);
    }

    GpBitmap bitmap = NULL;
    if (pGdipCreateBitmapFromFile(imagePath, &bitmap) != Ok || !bitmap) {
        return NULL;
    }

    UINT w = 0;
    UINT h = 0;
    pGdipGetImageWidth((GpImage)bitmap, &w);
    pGdipGetImageHeight((GpImage)bitmap, &h);

    if (w == 0 || h == 0) {
        pGdipDisposeImage((GpImage)bitmap);
        return NULL;
    }

    target->inUse = TRUE;
    target->bitmap = bitmap;
    target->width = w;
    target->height = h;
    target->lastWriteTime = writeTime;
    target->lastAccessTick = GetTickCount();
    wcsncpy(target->path, imagePath, MAX_PATH - 1);
    target->path[MAX_PATH - 1] = L'\0';
    return target;
}

void InitDrawingImage(void) {
    if (g_hGdiPlus) return;

    g_hGdiPlus = LoadLibraryA("gdiplus.dll");
    if (!g_hGdiPlus) {
        LOG_ERROR("Failed to load gdiplus.dll");
        return;
    }

    LOAD_GDIPLUS_PROC(g_hGdiPlus, "GdiplusStartup", pGdiplusStartup);
    LOAD_GDIPLUS_PROC(g_hGdiPlus, "GdiplusShutdown", pGdiplusShutdown);
    LOAD_GDIPLUS_PROC(g_hGdiPlus, "GdipCreateFromHDC", pGdipCreateFromHDC);
    LOAD_GDIPLUS_PROC(g_hGdiPlus, "GdipDeleteGraphics", pGdipDeleteGraphics);
    LOAD_GDIPLUS_PROC(g_hGdiPlus, "GdipCreateBitmapFromFile", pGdipCreateBitmapFromFile);
    LOAD_GDIPLUS_PROC(g_hGdiPlus, "GdipDisposeImage", pGdipDisposeImage);
    LOAD_GDIPLUS_PROC(g_hGdiPlus, "GdipDrawImageRectI", pGdipDrawImageRectI);
    LOAD_GDIPLUS_PROC(g_hGdiPlus, "GdipGetImageWidth", pGdipGetImageWidth);
    LOAD_GDIPLUS_PROC(g_hGdiPlus, "GdipGetImageHeight", pGdipGetImageHeight);

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
    for (int i = 0; i < MAX_CACHED_IMAGES; i++) {
        ReleaseCachedImageEntry(&g_imageCache[i]);
    }

    if (g_gdiplusToken && pGdiplusShutdown) {
        pGdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
    if (g_hGdiPlus) {
        FreeLibrary(g_hGdiPlus);
        g_hGdiPlus = NULL;
    }
}

BOOL GetImageDimensions(const wchar_t* imagePath, int* outWidth, int* outHeight) {
    if (!g_gdiplusToken || !imagePath || !outWidth || !outHeight) return FALSE;
    if (!pGdipGetImageWidth || !pGdipGetImageHeight) return FALSE;
    
    *outWidth = 0;
    *outHeight = 0;

    CachedImageEntry* entry = GetCachedImageEntry(imagePath);
    if (!entry) {
        return FALSE;
    }

    *outWidth = (int)entry->width;
    *outHeight = (int)entry->height;
    return TRUE;
}

BOOL RenderImageGDIPlus(HDC hdc, int x, int y, int width, int height, const wchar_t* imagePath) {
    if (!g_gdiplusToken || !hdc || !imagePath) return FALSE;
    if (!pGdipCreateBitmapFromFile || !pGdipCreateFromHDC || !pGdipDeleteGraphics || 
        !pGdipGetImageWidth || !pGdipGetImageHeight || !pGdipDrawImageRectI) return FALSE;

    CachedImageEntry* entry = GetCachedImageEntry(imagePath);
    if (!entry || !entry->bitmap) {
        // Silent failure is okay if file not found/locked
        return FALSE;
    }

    // Get graphics from HDC
    GpGraphics graphics = NULL;
    if (pGdipCreateFromHDC(hdc, &graphics) == Ok && graphics) {
        UINT imgW = entry->width;
        UINT imgH = entry->height;

        if (imgW > 0 && imgH > 0 && width > 0 && height > 0) {
            // Calculate scale to fit (Contain)
            float scaleX = (float)width / imgW;
            float scaleY = (float)height / imgH;
            float scale = (scaleX < scaleY) ? scaleX : scaleY;

            int drawW = (int)(imgW * scale);
            int drawH = (int)(imgH * scale);

            // Draw from top-left (no centering)
            pGdipDrawImageRectI(graphics, (GpImage)entry->bitmap, x, y, drawW, drawH);
        }
        
        pGdipDeleteGraphics(graphics);
    }

    return TRUE;
}
