#include "drawing/drawing_image.h"
#include "log.h"
#include <limits.h>
#include <string.h>

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
static INIT_ONCE g_imageStateLockOnce = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_imageStateCS;

static FuncGdiplusStartup pGdiplusStartup = NULL;
static FuncGdiplusShutdown pGdiplusShutdown = NULL;
static FuncGdipCreateFromHDC pGdipCreateFromHDC = NULL;
static FuncGdipDeleteGraphics pGdipDeleteGraphics = NULL;
static FuncGdipCreateBitmapFromFile pGdipCreateBitmapFromFile = NULL;
static FuncGdipDisposeImage pGdipDisposeImage = NULL;
static FuncGdipDrawImageRectI pGdipDrawImageRectI = NULL;
static FuncGdipGetImageWidth pGdipGetImageWidth = NULL;
static FuncGdipGetImageHeight pGdipGetImageHeight = NULL;

static BOOL CALLBACK InitImageStateLock(PINIT_ONCE initOnce, PVOID parameter, PVOID* context) {
    (void)initOnce;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&g_imageStateCS);
    return TRUE;
}

static BOOL LockImageState(void) {
    if (!InitOnceExecuteOnce(&g_imageStateLockOnce, InitImageStateLock, NULL, NULL)) {
        return FALSE;
    }
    EnterCriticalSection(&g_imageStateCS);
    return TRUE;
}

static void UnlockImageState(void) {
    LeaveCriticalSection(&g_imageStateCS);
}

static void ResetGdiPlusProcPointers(void) {
    pGdiplusStartup = NULL;
    pGdiplusShutdown = NULL;
    pGdipCreateFromHDC = NULL;
    pGdipDeleteGraphics = NULL;
    pGdipCreateBitmapFromFile = NULL;
    pGdipDisposeImage = NULL;
    pGdipDrawImageRectI = NULL;
    pGdipGetImageWidth = NULL;
    pGdipGetImageHeight = NULL;
}

#define LOAD_GDIPLUS_PROC(module, name, target) \
    do { \
        FARPROC proc = GetProcAddress((module), (name)); \
        memcpy(&(target), &proc, sizeof(target)); \
    } while (0)

static BOOL AreRequiredGdiPlusProcPointersLoaded(void) {
    return pGdiplusStartup &&
           pGdiplusShutdown &&
           pGdipCreateFromHDC &&
           pGdipDeleteGraphics &&
           pGdipCreateBitmapFromFile &&
           pGdipDisposeImage &&
           pGdipDrawImageRectI &&
           pGdipGetImageWidth &&
           pGdipGetImageHeight;
}

#define MAX_CACHED_IMAGES 16
#define IMAGE_CACHE_REVALIDATE_MS 1000
#define MAX_FAILED_IMAGE_LOADS 256
#define IMAGE_FAILURE_RETRY_MS 3000
#define GDIPLUS_INIT_FAILURE_RETRY_MS 5000
#define MAX_SINGLE_CACHED_IMAGE_PIXELS (4096u * 4096u)
#define MAX_TOTAL_CACHED_IMAGE_PIXELS  (4096u * 4096u)
#define MAX_IMAGE_FILE_BYTES (32ull * 1024ull * 1024ull)

static BOOL ScaleUIntToInt(UINT value, float scale, int* outValue) {
    double scaled;
    if (!outValue || value == 0 || scale <= 0.0f) return FALSE;

    scaled = (double)value * (double)scale;
    if (!(scaled > 0.0) || scaled > (double)INT_MAX) return FALSE;

    *outValue = (scaled < 1.0) ? 1 : (int)scaled;
    return TRUE;
}

typedef struct {
    BOOL inUse;
    wchar_t path[MAX_PATH];
    FILETIME lastWriteTime;
    ULONGLONG fileSizeBytes;
    GpBitmap bitmap;
    UINT width;
    UINT height;
    size_t pixelCount;
    DWORD lastAccessTick;
    DWORD lastValidateTick;
} CachedImageEntry;

typedef struct {
    FILETIME lastWriteTime;
    ULONGLONG fileSizeBytes;
    BOOL isDirectory;
} ImageFileInfo;

static CachedImageEntry g_imageCache[MAX_CACHED_IMAGES] = {0};
static size_t g_cachedImagePixels = 0;
static BOOL g_gdiPlusInitFailureRecorded = FALSE;
static DWORD g_gdiPlusInitFailureCooldownUntil = 0;

typedef struct {
    BOOL inUse;
    wchar_t path[MAX_PATH];
    FILETIME lastWriteTime;
    BOOL hasWriteTime;
    DWORD retryAfterFailureTick;
} FailedImageEntry;

static FailedImageEntry g_failedImageCache[MAX_FAILED_IMAGE_LOADS] = {0};

static void ReleaseCachedImageEntry(CachedImageEntry* entry) {
    if (!entry) return;
    if (entry->bitmap && pGdipDisposeImage) {
        pGdipDisposeImage((GpImage)entry->bitmap);
    }
    if (entry->inUse && entry->pixelCount <= g_cachedImagePixels) {
        g_cachedImagePixels -= entry->pixelCount;
    } else if (entry->inUse) {
        g_cachedImagePixels = 0;
    }
    ZeroMemory(entry, sizeof(*entry));
}

static BOOL CalculateImagePixelCount(UINT width, UINT height, size_t* outPixels) {
    if (!outPixels || width == 0 || height == 0) return FALSE;
    if ((size_t)width > ((size_t)-1) / (size_t)height) return FALSE;

    *outPixels = (size_t)width * (size_t)height;
    return TRUE;
}

static BOOL IsTickOlder(DWORD candidateTick, DWORD currentOldestTick, DWORD now) {
    return (DWORD)(now - candidateTick) > (DWORD)(now - currentOldestTick);
}

static size_t GetCachedPixelsAfterReservedRelease(const CachedImageEntry* reservedEntry) {
    if (!reservedEntry || !reservedEntry->inUse) {
        return g_cachedImagePixels;
    }
    if (reservedEntry->pixelCount > g_cachedImagePixels) {
        return 0;
    }
    return g_cachedImagePixels - reservedEntry->pixelCount;
}

static BOOL EvictCachedImagesForPixelBudget(const CachedImageEntry* reservedEntry,
                                            size_t newPixelCount) {
    if (newPixelCount > MAX_SINGLE_CACHED_IMAGE_PIXELS ||
        newPixelCount > MAX_TOTAL_CACHED_IMAGE_PIXELS) {
        return FALSE;
    }

    DWORD now = GetTickCount();
    while (GetCachedPixelsAfterReservedRelease(reservedEntry) >
           MAX_TOTAL_CACHED_IMAGE_PIXELS - newPixelCount) {
        CachedImageEntry* lruSlot = NULL;
        for (int i = 0; i < MAX_CACHED_IMAGES; i++) {
            CachedImageEntry* entry = &g_imageCache[i];
            if (!entry->inUse || entry == reservedEntry) {
                continue;
            }
            if (!lruSlot || IsTickOlder(entry->lastAccessTick,
                                        lruSlot->lastAccessTick,
                                        now)) {
                lruSlot = entry;
            }
        }

        if (!lruSlot) {
            return FALSE;
        }
        ReleaseCachedImageEntry(lruSlot);
    }

    return TRUE;
}

static BOOL GetImageFileInfo(const wchar_t* imagePath, ImageFileInfo* info) {
    if (!imagePath || !info) return FALSE;

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(imagePath, GetFileExInfoStandard, &attrs)) {
        return FALSE;
    }

    info->lastWriteTime = attrs.ftLastWriteTime;
    info->fileSizeBytes = ((ULONGLONG)attrs.nFileSizeHigh << 32) | attrs.nFileSizeLow;
    info->isDirectory = (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return TRUE;
}

static BOOL IsImageFileSizeAllowed(const wchar_t* imagePath, ULONGLONG fileSize) {
    if (!imagePath) return FALSE;

    if (fileSize > MAX_IMAGE_FILE_BYTES) {
        LOG_WARNING("Image file too large: %ls (%llu bytes, limit %llu bytes)",
                    imagePath, fileSize, (ULONGLONG)MAX_IMAGE_FILE_BYTES);
        return FALSE;
    }

    return TRUE;
}

static BOOL CopyFixedCachePathW(const wchar_t* imagePath, wchar_t* outPath) {
    if (!outPath) return FALSE;
    outPath[0] = L'\0';
    if (!imagePath) return FALSE;

    size_t len = 0;
    while (len < MAX_PATH && imagePath[len] != L'\0') {
        len++;
    }
    if (len >= MAX_PATH) {
        return FALSE;
    }

    memcpy(outPath, imagePath, (len + 1) * sizeof(wchar_t));
    return TRUE;
}

static BOOL FailedImageEntryMatches(const FailedImageEntry* entry,
                                    const FILETIME* writeTime,
                                    BOOL hasWriteTime) {
    if (!entry || entry->hasWriteTime != hasWriteTime) {
        return FALSE;
    }
    if (!hasWriteTime) {
        return TRUE;
    }
    return writeTime && CompareFileTime(&entry->lastWriteTime, writeTime) == 0;
}

static BOOL IsImageLoadFailureCached(const wchar_t* imagePath,
                                     const FILETIME* writeTime,
                                     BOOL hasWriteTime,
                                     DWORD now) {
    wchar_t cachePath[MAX_PATH] = {0};
    if (!CopyFixedCachePathW(imagePath, cachePath)) return FALSE;

    for (int i = 0; i < MAX_FAILED_IMAGE_LOADS; i++) {
        FailedImageEntry* entry = &g_failedImageCache[i];
        if (!entry->inUse || wcscmp(entry->path, cachePath) != 0) {
            continue;
        }

        if ((LONG)(entry->retryAfterFailureTick - now) <= 0) {
            ZeroMemory(entry, sizeof(*entry));
            return FALSE;
        }

        if (!entry->hasWriteTime) {
            ImageFileInfo currentInfo = {0};
            if (GetImageFileInfo(cachePath, &currentInfo) && !currentInfo.isDirectory) {
                ZeroMemory(entry, sizeof(*entry));
                return FALSE;
            }
        }

        return FailedImageEntryMatches(entry, writeTime, hasWriteTime);
    }

    return FALSE;
}

static void RecordImageLoadFailure(const wchar_t* imagePath,
                                   const FILETIME* writeTime,
                                   BOOL hasWriteTime,
                                   DWORD now) {
    wchar_t cachePath[MAX_PATH] = {0};
    if (!CopyFixedCachePathW(imagePath, cachePath)) return;

    FailedImageEntry* freeSlot = NULL;
    FailedImageEntry* oldestSlot = NULL;
    FailedImageEntry* target = NULL;

    for (int i = 0; i < MAX_FAILED_IMAGE_LOADS; i++) {
        FailedImageEntry* entry = &g_failedImageCache[i];
        if (!entry->inUse) {
            if (!freeSlot) freeSlot = entry;
            continue;
        }

        if ((LONG)(entry->retryAfterFailureTick - now) <= 0) {
            ZeroMemory(entry, sizeof(*entry));
            if (!freeSlot) freeSlot = entry;
            continue;
        }

        if (wcscmp(entry->path, cachePath) == 0) {
            target = entry;
            break;
        }

        if (!oldestSlot ||
            (LONG)(oldestSlot->retryAfterFailureTick -
                   entry->retryAfterFailureTick) > 0) {
            oldestSlot = entry;
        }
    }

    if (!target) {
        target = freeSlot ? freeSlot : oldestSlot;
    }
    if (!target) return;

    target->inUse = TRUE;
    target->hasWriteTime = hasWriteTime;
    if (hasWriteTime && writeTime) {
        target->lastWriteTime = *writeTime;
    } else {
        ZeroMemory(&target->lastWriteTime, sizeof(target->lastWriteTime));
    }
    DWORD retryAfter = now + IMAGE_FAILURE_RETRY_MS;
    target->retryAfterFailureTick = retryAfter ? retryAfter : 1;
    memcpy(target->path, cachePath, sizeof(cachePath));
}

static void ClearImageLoadFailure(const wchar_t* imagePath) {
    wchar_t cachePath[MAX_PATH] = {0};
    if (!CopyFixedCachePathW(imagePath, cachePath)) return;

    for (int i = 0; i < MAX_FAILED_IMAGE_LOADS; i++) {
        FailedImageEntry* entry = &g_failedImageCache[i];
        if (entry->inUse && wcscmp(entry->path, cachePath) == 0) {
            ZeroMemory(entry, sizeof(*entry));
            return;
        }
    }
}

static CachedImageEntry* GetCachedImageEntry(const wchar_t* imagePath) {
    if (!g_gdiplusToken || !imagePath || !pGdipCreateBitmapFromFile ||
        !pGdipGetImageWidth || !pGdipGetImageHeight) {
        return NULL;
    }

    wchar_t cachePath[MAX_PATH] = {0};
    if (!CopyFixedCachePathW(imagePath, cachePath)) {
        return NULL;
    }

    DWORD now = GetTickCount();
    ImageFileInfo fileInfo = {0};
    BOOL hasFileInfo = FALSE;

    if (IsImageLoadFailureCached(cachePath, NULL, FALSE, now)) {
        return NULL;
    }

    CachedImageEntry* replacementSlot = NULL;
    CachedImageEntry* freeSlot = NULL;
    CachedImageEntry* lruSlot = NULL;
    for (int i = 0; i < MAX_CACHED_IMAGES; i++) {
        CachedImageEntry* entry = &g_imageCache[i];
        if (!entry->inUse) {
            if (!freeSlot) freeSlot = entry;
            continue;
        }

        if (wcscmp(entry->path, cachePath) == 0) {
            if (entry->bitmap &&
                (DWORD)(now - entry->lastValidateTick) < IMAGE_CACHE_REVALIDATE_MS) {
                entry->lastAccessTick = now;
                return entry;
            }

            if (!GetImageFileInfo(cachePath, &fileInfo)) {
                ReleaseCachedImageEntry(entry);
                RecordImageLoadFailure(cachePath, NULL, FALSE, now);
                return NULL;
            }
            hasFileInfo = TRUE;

            if (fileInfo.isDirectory) {
                ReleaseCachedImageEntry(entry);
                RecordImageLoadFailure(cachePath, &fileInfo.lastWriteTime, TRUE, now);
                return NULL;
            }

            if (CompareFileTime(&entry->lastWriteTime, &fileInfo.lastWriteTime) == 0 &&
                entry->fileSizeBytes == fileInfo.fileSizeBytes &&
                entry->bitmap) {
                entry->lastAccessTick = now;
                entry->lastValidateTick = now;
                return entry;
            }

            replacementSlot = entry;
            break;
        }

        if (!lruSlot || IsTickOlder(entry->lastAccessTick,
                                    lruSlot->lastAccessTick,
                                    now)) {
            lruSlot = entry;
        }
    }

    CachedImageEntry* target = replacementSlot ? replacementSlot :
                               (freeSlot ? freeSlot : lruSlot);
    if (!target) {
        return NULL;
    }

    if (!hasFileInfo) {
        if (!GetImageFileInfo(cachePath, &fileInfo)) {
            RecordImageLoadFailure(cachePath, NULL, FALSE, now);
            return NULL;
        }
    }

    if (fileInfo.isDirectory) {
        RecordImageLoadFailure(cachePath, &fileInfo.lastWriteTime, TRUE, now);
        return NULL;
    }

    if (IsImageLoadFailureCached(cachePath, &fileInfo.lastWriteTime, TRUE, now)) {
        return NULL;
    }

    if (!IsImageFileSizeAllowed(cachePath, fileInfo.fileSizeBytes)) {
        RecordImageLoadFailure(cachePath, &fileInfo.lastWriteTime, TRUE, now);
        return NULL;
    }

    GpBitmap bitmap = NULL;
    if (pGdipCreateBitmapFromFile(cachePath, &bitmap) != Ok || !bitmap) {
        RecordImageLoadFailure(cachePath, &fileInfo.lastWriteTime, TRUE, now);
        return NULL;
    }

    UINT w = 0;
    UINT h = 0;
    if (pGdipGetImageWidth((GpImage)bitmap, &w) != Ok ||
        pGdipGetImageHeight((GpImage)bitmap, &h) != Ok) {
        pGdipDisposeImage((GpImage)bitmap);
        RecordImageLoadFailure(cachePath, &fileInfo.lastWriteTime, TRUE, now);
        return NULL;
    }

    if (w == 0 || h == 0 || w > INT_MAX || h > INT_MAX) {
        pGdipDisposeImage((GpImage)bitmap);
        RecordImageLoadFailure(cachePath, &fileInfo.lastWriteTime, TRUE, now);
        return NULL;
    }

    size_t pixelCount = 0;
    if (!CalculateImagePixelCount(w, h, &pixelCount) ||
        !EvictCachedImagesForPixelBudget(target, pixelCount)) {
        pGdipDisposeImage((GpImage)bitmap);
        RecordImageLoadFailure(cachePath, &fileInfo.lastWriteTime, TRUE, now);
        return NULL;
    }

    ClearImageLoadFailure(cachePath);
    if (target->inUse) {
        ReleaseCachedImageEntry(target);
    }
    target->inUse = TRUE;
    target->bitmap = bitmap;
    target->width = w;
    target->height = h;
    target->pixelCount = pixelCount;
    target->lastWriteTime = fileInfo.lastWriteTime;
    target->fileSizeBytes = fileInfo.fileSizeBytes;
    target->lastAccessTick = now;
    target->lastValidateTick = now;
    g_cachedImagePixels += pixelCount;
    memcpy(target->path, cachePath, sizeof(cachePath));
    return target;
}

static BOOL ShouldRetryGdiPlusInit(void) {
    if (!g_gdiPlusInitFailureRecorded) {
        return TRUE;
    }

    return (LONG)(g_gdiPlusInitFailureCooldownUntil - GetTickCount()) <= 0;
}

static void RecordGdiPlusInitFailure(void) {
    DWORD cooldownUntil = GetTickCount() + GDIPLUS_INIT_FAILURE_RETRY_MS;
    g_gdiPlusInitFailureRecorded = TRUE;
    g_gdiPlusInitFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

static void InitDrawingImageLocked(void) {
    if (g_hGdiPlus) return;
    if (!ShouldRetryGdiPlusInit()) return;

    g_hGdiPlus = LoadLibraryA("gdiplus.dll");
    if (!g_hGdiPlus) {
        LOG_ERROR("Failed to load gdiplus.dll");
        RecordGdiPlusInitFailure();
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

    if (!AreRequiredGdiPlusProcPointersLoaded()) {
        LOG_ERROR("Required GDI+ entry points not found");
        FreeLibrary(g_hGdiPlus);
        g_hGdiPlus = NULL;
        ResetGdiPlusProcPointers();
        RecordGdiPlusInitFailure();
        return;
    }

    GdiplusStartupInput input = {1, NULL, FALSE, FALSE};
    if (pGdiplusStartup(&g_gdiplusToken, &input, NULL) != Ok) {
        LOG_ERROR("GdiplusStartup failed");
        g_gdiplusToken = 0;
        FreeLibrary(g_hGdiPlus);
        g_hGdiPlus = NULL;
        ResetGdiPlusProcPointers();
        RecordGdiPlusInitFailure();
        return;
    }

    g_gdiPlusInitFailureRecorded = FALSE;
    g_gdiPlusInitFailureCooldownUntil = 0;
    LOG_INFO("GDI+ initialized successfully");
}

void InitDrawingImage(void) {
    if (!LockImageState()) return;
    InitDrawingImageLocked();
    UnlockImageState();
}

static BOOL EnsureDrawingImageInitializedLocked(void) {
    if (!g_gdiplusToken) {
        InitDrawingImageLocked();
    }
    return g_gdiplusToken != 0;
}

void ShutdownDrawingImage(void) {
    if (!LockImageState()) return;

    for (int i = 0; i < MAX_CACHED_IMAGES; i++) {
        ReleaseCachedImageEntry(&g_imageCache[i]);
    }
    g_cachedImagePixels = 0;
    ZeroMemory(g_failedImageCache, sizeof(g_failedImageCache));

    if (g_gdiplusToken && pGdiplusShutdown) {
        pGdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
    if (g_hGdiPlus) {
        FreeLibrary(g_hGdiPlus);
        g_hGdiPlus = NULL;
    }
    ResetGdiPlusProcPointers();
    g_gdiPlusInitFailureRecorded = FALSE;
    g_gdiPlusInitFailureCooldownUntil = 0;

    UnlockImageState();
}

BOOL GetImageDimensions(const wchar_t* imagePath, int* outWidth, int* outHeight) {
    if (!imagePath || !outWidth || !outHeight) return FALSE;

    *outWidth = 0;
    *outHeight = 0;
    if (!LockImageState()) return FALSE;

    if (!EnsureDrawingImageInitializedLocked() ||
        !pGdipGetImageWidth || !pGdipGetImageHeight) {
        UnlockImageState();
        return FALSE;
    }

    const CachedImageEntry* entry = GetCachedImageEntry(imagePath);
    if (!entry) {
        UnlockImageState();
        return FALSE;
    }

    *outWidth = (int)entry->width;
    *outHeight = (int)entry->height;
    UnlockImageState();
    return TRUE;
}

BOOL BeginImageRenderContext(HDC hdc, ImageRenderContext* ctx) {
    if (!ctx) return FALSE;
    ctx->graphics = NULL;
    ctx->stateLocked = FALSE;

    if (!hdc) return FALSE;
    if (!LockImageState()) return FALSE;
    ctx->stateLocked = TRUE;

    if (!EnsureDrawingImageInitializedLocked() ||
        !pGdipCreateFromHDC || !pGdipDeleteGraphics || !pGdipDrawImageRectI) {
        ctx->stateLocked = FALSE;
        UnlockImageState();
        return FALSE;
    }

    GpGraphics graphics = NULL;
    if (pGdipCreateFromHDC(hdc, &graphics) != Ok || !graphics) {
        ctx->stateLocked = FALSE;
        UnlockImageState();
        return FALSE;
    }

    ctx->graphics = graphics;
    return TRUE;
}

void EndImageRenderContext(ImageRenderContext* ctx) {
    if (!ctx) return;

    if (ctx->stateLocked && ctx->graphics && pGdipDeleteGraphics) {
        pGdipDeleteGraphics((GpGraphics)ctx->graphics);
    }
    ctx->graphics = NULL;
    if (ctx->stateLocked) {
        ctx->stateLocked = FALSE;
        UnlockImageState();
    }
}

BOOL RenderImageGDIPlusWithContext(ImageRenderContext* ctx, int x, int y,
                                   int width, int height, const wchar_t* imagePath) {
    if (!ctx || !ctx->stateLocked || !ctx->graphics || !imagePath) return FALSE;
    if (!pGdipCreateBitmapFromFile || !pGdipGetImageWidth ||
        !pGdipGetImageHeight || !pGdipDrawImageRectI) return FALSE;

    CachedImageEntry* entry = GetCachedImageEntry(imagePath);
    if (!entry || !entry->bitmap) {
        // Silent failure is okay if file not found/locked
        return FALSE;
    }

    UINT imgW = entry->width;
    UINT imgH = entry->height;
    if (imgW == 0 || imgH == 0 || width <= 0 || height <= 0) {
        return FALSE;
    }

    float scaleX = (float)width / imgW;
    float scaleY = (float)height / imgH;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;

    int drawW = 0;
    int drawH = 0;
    if (!ScaleUIntToInt(imgW, scale, &drawW) ||
        !ScaleUIntToInt(imgH, scale, &drawH)) {
        return FALSE;
    }

    GpStatus drawStatus = pGdipDrawImageRectI((GpGraphics)ctx->graphics,
                                              (GpImage)entry->bitmap,
                                              x, y, drawW, drawH);
    return drawStatus == Ok;
}

BOOL RenderImageGDIPlus(HDC hdc, int x, int y, int width, int height, const wchar_t* imagePath) {
    ImageRenderContext ctx;
    if (!BeginImageRenderContext(hdc, &ctx)) {
        return FALSE;
    }

    BOOL result = RenderImageGDIPlusWithContext(&ctx, x, y, width, height, imagePath);
    EndImageRenderContext(&ctx);
    return result;
}
