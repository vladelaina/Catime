#ifndef DRAWING_IMAGE_GDIPLUS_INTERNAL_H
#define DRAWING_IMAGE_GDIPLUS_INTERNAL_H

#include <windows.h>
#include <stddef.h>

typedef void* GpGraphics;
typedef void* GpBitmap;
typedef void* GpImage;
typedef int GpStatus;

#define GDIPLUS_STATUS_OK 0

typedef struct {
    UINT32 GdiplusVersion;
    void* DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInput;

typedef GpStatus (WINAPI *GdiplusStartupProc)(
    ULONG_PTR*, const GdiplusStartupInput*, void*);
typedef void (WINAPI *GdiplusShutdownProc)(ULONG_PTR);
typedef GpStatus (WINAPI *GdipCreateFromHDCProc)(HDC, GpGraphics*);
typedef GpStatus (WINAPI *GdipDeleteGraphicsProc)(GpGraphics);
typedef GpStatus (WINAPI *GdipCreateBitmapFromFileProc)(
    const WCHAR*, GpBitmap*);
typedef GpStatus (WINAPI *GdipDisposeImageProc)(GpImage);
typedef GpStatus (WINAPI *GdipDrawImageRectIProc)(
    GpGraphics, GpImage, INT, INT, INT, INT);
typedef GpStatus (WINAPI *GdipGetImageWidthProc)(GpImage, UINT*);
typedef GpStatus (WINAPI *GdipGetImageHeightProc)(GpImage, UINT*);

typedef struct {
    HMODULE module;
    ULONG_PTR token;
    GdiplusStartupProc startup;
    GdiplusShutdownProc shutdown;
    GdipCreateFromHDCProc createFromHdc;
    GdipDeleteGraphicsProc deleteGraphics;
    GdipCreateBitmapFromFileProc createBitmapFromFile;
    GdipDisposeImageProc disposeImage;
    GdipDrawImageRectIProc drawImageRect;
    GdipGetImageWidthProc getImageWidth;
    GdipGetImageHeightProc getImageHeight;
} DrawingImageRuntime;

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

extern DrawingImageRuntime g_drawingImageRuntime;

BOOL DrawingImage_LockState(void);
void DrawingImage_UnlockState(void);
BOOL DrawingImage_EnsureInitializedLocked(void);

CachedImageEntry* DrawingImageCache_Get(const wchar_t* imagePath);
void DrawingImageCache_Clear(void);
BOOL DrawingImageCache_CopyPath(const wchar_t* source,
                                wchar_t destination[MAX_PATH]);
BOOL DrawingImageCache_GetFileInfo(const wchar_t* path,
                                   ImageFileInfo* info);
BOOL DrawingImageCache_IsFileSizeAllowed(const wchar_t* path,
                                         ULONGLONG fileSize);

BOOL DrawingImageFailure_IsCached(const wchar_t* path,
                                  const FILETIME* writeTime,
                                  BOOL hasWriteTime, DWORD now);
void DrawingImageFailure_Record(const wchar_t* path,
                                const FILETIME* writeTime,
                                BOOL hasWriteTime, DWORD now);
void DrawingImageFailure_Clear(const wchar_t* path);
void DrawingImageFailure_Reset(void);

#endif /* DRAWING_IMAGE_GDIPLUS_INTERNAL_H */
