/**
 * @file tray_animation.c
 * @brief RunCat-like tray icon animation implementation
 */

#include <windows.h> 
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <wincodec.h>
#include <propvarutil.h>
#include <objbase.h>

#include "../include/tray.h"
#include "../include/config.h"
#include "../include/tray_menu.h"
/** Forward declaration for timer callback used by SetTimer */
static void CALLBACK TrayAnimTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);

/**
 * @brief Timer ID for tray animation
 */
#define TRAY_ANIM_TIMER_ID 42420

/** @brief Max frames supported */
#define MAX_TRAY_FRAMES 64

/** @brief Loaded icon frames and state */
static HICON g_trayIcons[MAX_TRAY_FRAMES];
static int g_trayIconCount = 0;
static int g_trayIconIndex = 0;
static UINT g_trayInterval = 0;
static HWND g_trayHwnd = NULL;
static char g_animationName[64] = "cat"; /** current folder under animations */
static BOOL g_isPreviewActive = FALSE; /** preview mode flag */
static HICON g_previewIcons[MAX_TRAY_FRAMES];
static int g_previewCount = 0;
static int g_previewIndex = 0;
static BOOL g_isAnimated = FALSE; /** whether current animation source is a single GIF/WebP file */
static UINT g_frameDelaysMs[MAX_TRAY_FRAMES]; /** per-frame delay in ms for animated images */
static BOOL g_isPreviewAnimated = FALSE; /** whether current preview source is a single GIF/WebP file */
static UINT g_previewFrameDelaysMs[MAX_TRAY_FRAMES]; /** per-frame delay for animated image preview */

/** GIF composition canvas for proper frame blending */
static UINT g_animCanvasWidth = 0;
static UINT g_animCanvasHeight = 0;
static BYTE* g_animCanvas = NULL;  /** 32bpp PBGRA canvas for frame composition */
static BYTE* g_previewAnimCanvas = NULL;  /** 32bpp PBGRA canvas for preview composition */

/** @brief Context for directing decoded animation frames to tray or preview targets */
typedef struct {
    HICON* icons;
    int*   count;
    int*   index;
    UINT*  delays;
    BOOL*  isAnimatedFlag;
    BYTE** canvas;
} DecodeTarget;

/** @brief Build cat animation folder path: %LOCALAPPDATA%\Catime\resources\animations\cat */
static void BuildAnimationFolder(const char* name, char* path, size_t size) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    size_t len = strlen(base);
    if (len > 0 && (base[len-1] == '/' || base[len-1] == '\\')) {
        snprintf(path, size, "%s%s", base, name);
    } else {
        snprintf(path, size, "%s\\%s", base, name);
    }
}

/** @brief Free a set of icon resources, including the composition canvas */
static void FreeIconSet(HICON icons[], int* count, int* index, BOOL* isAnimated, BYTE** canvas, BOOL resetCanvasSize) {
    for (int i = 0; i < *count; ++i) {
        if (icons[i]) {
            DestroyIcon(icons[i]);
            icons[i] = NULL;
        }
    }
    *count = 0;
    *index = 0;
    *isAnimated = FALSE;
    
    if (*canvas) {
        free(*canvas);
        *canvas = NULL;
    }
    if (resetCanvasSize) {
        g_animCanvasWidth = 0;
        g_animCanvasHeight = 0;
    }
}

/** @brief Case-insensitive string ends-with helper */
static BOOL EndsWithIgnoreCase(const char* str, const char* suffix) {
    if (!str || !suffix) return FALSE;
    size_t ls = strlen(str), lsuf = strlen(suffix);
    if (lsuf > ls) return FALSE;
    return _stricmp(str + (ls - lsuf), suffix) == 0;
}

/** @brief Detect if current animation name is a single GIF file */
static BOOL IsGifSelection(const char* name) {
    return name && EndsWithIgnoreCase(name, ".gif");
}

/** @brief Detect if current animation name is a single WebP file */
static BOOL IsWebPSelection(const char* name) {
    return name && EndsWithIgnoreCase(name, ".webp");
}

/** @brief Alpha blend pixel onto canvas with proper compositing */
static void BlendPixel(BYTE* canvas, UINT canvasStride, UINT x, UINT y, BYTE r, BYTE g, BYTE b, BYTE a) {
    if (a == 0) return; /** fully transparent, no change */
    
    BYTE* pixel = canvas + (y * canvasStride) + (x * 4);
    if (a == 255) {
        /** fully opaque, direct copy */
        pixel[0] = b; /** B */
        pixel[1] = g; /** G */
        pixel[2] = r; /** R */
        pixel[3] = a; /** A */
    } else {
        /** alpha blend with existing pixel */
        UINT srcAlpha = a;
        UINT dstAlpha = pixel[3];
        
        if (dstAlpha == 0) {
            /** destination is transparent, just copy source */
            pixel[0] = b;
            pixel[1] = g;
            pixel[2] = r;
            pixel[3] = a;
        } else {
            /** proper alpha compositing: src over dst */
            UINT invSrcAlpha = 255 - srcAlpha;
            UINT newAlpha = srcAlpha + (dstAlpha * invSrcAlpha) / 255;
            
            if (newAlpha > 0) {
                pixel[0] = (BYTE)((b * srcAlpha + pixel[0] * dstAlpha * invSrcAlpha / 255) / newAlpha);
                pixel[1] = (BYTE)((g * srcAlpha + pixel[1] * dstAlpha * invSrcAlpha / 255) / newAlpha);
                pixel[2] = (BYTE)((r * srcAlpha + pixel[2] * dstAlpha * invSrcAlpha / 255) / newAlpha);
                pixel[3] = (BYTE)newAlpha;
            }
        }
    }
}

/** @brief Clear rectangle on canvas with background color (supports transparency) */
static void ClearCanvasRect(BYTE* canvas, UINT canvasWidth, UINT canvasHeight, 
                           UINT left, UINT top, UINT width, UINT height, 
                           BYTE bgR, BYTE bgG, BYTE bgB, BYTE bgA) {
    UINT canvasStride = canvasWidth * 4;
    UINT right = left + width;
    UINT bottom = top + height;
    
    /** Clamp to canvas bounds */
    if (left >= canvasWidth || top >= canvasHeight) return;
    if (right > canvasWidth) right = canvasWidth;
    if (bottom > canvasHeight) bottom = canvasHeight;
    
    for (UINT y = top; y < bottom; y++) {
        for (UINT x = left; x < right; x++) {
            BYTE* pixel = canvas + (y * canvasStride) + (x * 4);
            pixel[0] = bgB; /** B */
            pixel[1] = bgG; /** G */
            pixel[2] = bgR; /** R */
            pixel[3] = bgA; /** A */
        }
    }
}

/** @brief Create an HICON from any IWICBitmapSource by scaling to (cx, cy) */
static HICON CreateIconFromWICSource(IWICImagingFactory* pFactory,
                                     IWICBitmapSource* source,
                                     int cx,
                                     int cy) {
    if (!pFactory || !source || cx <= 0 || cy <= 0) return NULL;

    HICON hIcon = NULL;

    IWICBitmapScaler* pScaler = NULL;
    HRESULT hr = pFactory->lpVtbl->CreateBitmapScaler(pFactory, &pScaler);
    if (SUCCEEDED(hr) && pScaler) {
        hr = pScaler->lpVtbl->Initialize(pScaler, source, cx, cy, WICBitmapInterpolationModeFant);
        if (SUCCEEDED(hr)) {
            IWICFormatConverter* pConverter = NULL;
            hr = pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter);
            if (SUCCEEDED(hr) && pConverter) {
                hr = pConverter->lpVtbl->Initialize(pConverter,
                                                    (IWICBitmapSource*)pScaler,
                                                    &GUID_WICPixelFormat32bppPBGRA,
                                                    WICBitmapDitherTypeNone,
                                                    NULL,
                                                    0.0,
                                                    WICBitmapPaletteTypeCustom);
                if (SUCCEEDED(hr)) {
                    BITMAPINFO bi; ZeroMemory(&bi, sizeof(bi));
                    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bi.bmiHeader.biWidth = cx;
                    bi.bmiHeader.biHeight = -cy; /** top-down */
                    bi.bmiHeader.biPlanes = 1;
                    bi.bmiHeader.biBitCount = 32;
                    bi.bmiHeader.biCompression = BI_RGB;
                    VOID* pvBits = NULL;
                    HBITMAP hbmColor = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
                    if (hbmColor && pvBits) {
                        UINT stride = (UINT)(cx * 4);
                        UINT bufSize = (UINT)(cy * stride);
                        if (SUCCEEDED(pConverter->lpVtbl->CopyPixels(pConverter, NULL, stride, bufSize, (BYTE*)pvBits))) {
                            ICONINFO ii; ZeroMemory(&ii, sizeof(ii));
                            ii.fIcon = TRUE;
                            ii.hbmColor = hbmColor;

                            ii.hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);
                            if (ii.hbmMask) {
                                HDC hdcMem = GetDC(NULL);
                                HDC hdcColor = CreateCompatibleDC(hdcMem);
                                HDC hdcMask = CreateCompatibleDC(hdcMem);
                                SelectObject(hdcColor, hbmColor);
                                SelectObject(hdcMask, ii.hbmMask);

                                BitBlt(hdcMask, 0, 0, cx, cy, NULL, 0, 0, BLACKNESS);
                                SetBkColor(hdcColor, RGB(0,0,0));
                                BitBlt(hdcMask, 0, 0, cx, cy, hdcColor, 0, 0, SRCCOPY);
                                BitBlt(hdcMask, 0, 0, cx, cy, NULL, 0, 0, DSTINVERT);

                                DeleteDC(hdcColor);
                                DeleteDC(hdcMask);
                                ReleaseDC(NULL, hdcMem);
                            }

                            hIcon = CreateIconIndirect(&ii);
                            if (ii.hbmMask) DeleteObject(ii.hbmMask);
                        }
                        DeleteObject(hbmColor);
                    }
                }
                pConverter->lpVtbl->Release(pConverter);
            }
        }
        pScaler->lpVtbl->Release(pScaler);
    }

    return hIcon;
}

/** @brief Create an HICON from a 32bpp PBGRA memory canvas by scaling to (cx, cy) */
static HICON CreateIconFromPBGRA(IWICImagingFactory* pFactory,
                                 const BYTE* canvasPixels,
                                 UINT canvasWidth,
                                 UINT canvasHeight,
                                 int cx,
                                 int cy) {
    if (!pFactory || !canvasPixels || canvasWidth == 0 || canvasHeight == 0 || cx <= 0 || cy <= 0) return NULL;

    HICON hIcon = NULL;
    IWICBitmap* pBitmap = NULL;

    const UINT stride = canvasWidth * 4;
    const UINT size = canvasHeight * stride;

    HRESULT hr = pFactory->lpVtbl->CreateBitmapFromMemory(pFactory, canvasWidth, canvasHeight, &GUID_WICPixelFormat32bppPBGRA, stride, size, (BYTE*)canvasPixels, &pBitmap);
    if (SUCCEEDED(hr) && pBitmap) {
        hIcon = CreateIconFromWICSource(pFactory, (IWICBitmapSource*)pBitmap, cx, cy);
        pBitmap->lpVtbl->Release(pBitmap);
    }

    return hIcon;
}

/** @brief Generic animated image decoding routine for GIF and WebP */
static void LoadAnimatedImage(const char* utf8Path, DecodeTarget* target) {
    if (target->icons == g_trayIcons) {
        FreeIconSet(g_trayIcons, &g_trayIconCount, &g_trayIconIndex, &g_isAnimated, &g_animCanvas, TRUE);
    } else {
        FreeIconSet(g_previewIcons, &g_previewCount, &g_previewIndex, &g_isPreviewAnimated, &g_previewAnimCanvas, FALSE);
    }
    if (!utf8Path || !*utf8Path) return;

    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);

    HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* pFactory = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&pFactory);
    if (FAILED(hr) || !pFactory) {
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return;
    }

    IWICBitmapDecoder* pDecoder = NULL;
    hr = pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, wPath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr) || !pDecoder) {
        if (pFactory) pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return;
    }

    GUID containerFormat;
    BOOL isGif = FALSE;
    if (SUCCEEDED(pDecoder->lpVtbl->GetContainerFormat(pDecoder, &containerFormat))) {
        if (IsEqualGUID(&containerFormat, &GUID_ContainerFormatGif)) {
            isGif = TRUE;
        }
    }

    UINT canvasWidth = 0, canvasHeight = 0;

    /** Format-specific: Get canvas size */
    if (isGif) {
        IWICMetadataQueryReader* pGlobalMeta = NULL;
        if (SUCCEEDED(pDecoder->lpVtbl->GetMetadataQueryReader(pDecoder, &pGlobalMeta)) && pGlobalMeta) {
            PROPVARIANT var;
            PropVariantInit(&var);
            if (SUCCEEDED(pGlobalMeta->lpVtbl->GetMetadataByName(pGlobalMeta, L"/logscrdesc/Width", &var))) {
                if (var.vt == VT_UI2) canvasWidth = var.uiVal;
                else if (var.vt == VT_I2) canvasWidth = (UINT)var.iVal;
            }
            PropVariantClear(&var);

            PropVariantInit(&var);
            if (SUCCEEDED(pGlobalMeta->lpVtbl->GetMetadataByName(pGlobalMeta, L"/logscrdesc/Height", &var))) {
                if (var.vt == VT_UI2) canvasHeight = var.uiVal;
                else if (var.vt == VT_I2) canvasHeight = (UINT)var.iVal;
            }
            PropVariantClear(&var);
            pGlobalMeta->lpVtbl->Release(pGlobalMeta);
        }
    }

    /** Common fallback for canvas size */
    if (canvasWidth == 0 || canvasHeight == 0) {
        IWICBitmapFrameDecode* pFirstFrame = NULL;
        if (SUCCEEDED(pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFirstFrame)) && pFirstFrame) {
            pFirstFrame->lpVtbl->GetSize(pFirstFrame, &canvasWidth, &canvasHeight);
            pFirstFrame->lpVtbl->Release(pFirstFrame);
        }
    }

    if (canvasWidth == 0 || canvasHeight == 0) {
        pDecoder->lpVtbl->Release(pDecoder);
        if (pFactory) pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return;
    }

    g_animCanvasWidth = canvasWidth;
    g_animCanvasHeight = canvasHeight;

    UINT canvasStride = canvasWidth * 4;
    UINT canvasSize = canvasHeight * canvasStride;
    *(target->canvas) = (BYTE*)malloc(canvasSize);
    if (!*(target->canvas)) {
        pDecoder->lpVtbl->Release(pDecoder);
        if (pFactory) pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return;
    }
    memset(*(target->canvas), 0, canvasSize);

    UINT frameCount = 0;
    if (SUCCEEDED(pDecoder->lpVtbl->GetFrameCount(pDecoder, &frameCount))) {
        UINT prevDisposal = 0;
        UINT prevLeft = 0, prevTop = 0, prevWidth = 0, prevHeight = 0;

        for (UINT i = 0; i < frameCount && *(target->count) < MAX_TRAY_FRAMES; ++i) {
            IWICBitmapFrameDecode* pFrame = NULL;
            if (FAILED(pDecoder->lpVtbl->GetFrame(pDecoder, i, &pFrame)) || !pFrame) continue;

            /** GIF-specific disposal for previous frame */
            if (isGif && i > 0) {
                if (prevDisposal == 2) { /** Restore background */
                    ClearCanvasRect(*(target->canvas), canvasWidth, canvasHeight, prevLeft, prevTop, prevWidth, prevHeight, 0, 0, 0, 0);
                }
            } else if (!isGif) {
                /** WebP simple implementation: clear canvas for each frame */
                 memset(*(target->canvas), 0, canvasSize);
            }

            UINT delayMs = 100;
            UINT disposal = 0;
            UINT frameLeft = 0, frameTop = 0, frameWidth = 0, frameHeight = 0;

            IWICMetadataQueryReader* pMeta = NULL;
            if (SUCCEEDED(pFrame->lpVtbl->GetMetadataQueryReader(pFrame, &pMeta)) && pMeta) {
                PROPVARIANT var;
                if (isGif) {
                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/grctlext/Delay", &var))) {
                        if (var.vt == VT_UI2 || var.vt == VT_I2) {
                            USHORT cs = (var.vt == VT_UI2) ? var.uiVal : (USHORT)var.iVal;
                            if (cs == 0) cs = 10;
                            delayMs = (UINT)cs * 10U;
                        }
                    }
                    PropVariantClear(&var);

                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/grctlext/Disposal", &var))) {
                        if (var.vt == VT_UI1) disposal = var.bVal;
                    }
                    PropVariantClear(&var);

                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/imgdesc/Left", &var))) {
                        if (var.vt == VT_UI2) frameLeft = var.uiVal;
                    }
                    PropVariantClear(&var);
                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/imgdesc/Top", &var))) {
                        if (var.vt == VT_UI2) frameTop = var.uiVal;
                    }
                    PropVariantClear(&var);
                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/imgdesc/Width", &var))) {
                        if (var.vt == VT_UI2) frameWidth = var.uiVal;
                    }
                    PropVariantClear(&var);
                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/imgdesc/Height", &var))) {
                        if (var.vt == VT_UI2) frameHeight = var.uiVal;
                    }
                    PropVariantClear(&var);
                } else { /** Assume WebP */
                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/webp/delay", &var))) {
                        if (var.vt == VT_UI4) delayMs = var.ulVal;
                    }
                    PropVariantClear(&var);
                }
                pMeta->lpVtbl->Release(pMeta);
            }

            pFrame->lpVtbl->GetSize(pFrame, &frameWidth, &frameHeight);

            IWICFormatConverter* pConverter = NULL;
            if (SUCCEEDED(pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter)) && pConverter) {
                if (SUCCEEDED(pConverter->lpVtbl->Initialize(pConverter, (IWICBitmapSource*)pFrame, &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom))) {
                    UINT frameStride = frameWidth * 4;
                    UINT frameBufferSize = frameHeight * frameStride;
                    BYTE* frameBuffer = (BYTE*)malloc(frameBufferSize);
                    if (frameBuffer) {
                        if (SUCCEEDED(pConverter->lpVtbl->CopyPixels(pConverter, NULL, frameStride, frameBufferSize, frameBuffer))) {
                            if (isGif) {
                                for (UINT y = 0; y < frameHeight; y++) {
                                    for (UINT x = 0; x < frameWidth; x++) {
                                        if (frameLeft + x < canvasWidth && frameTop + y < canvasHeight) {
                                            BYTE* srcPixel = frameBuffer + (y * frameStride) + (x * 4);
                                            BlendPixel(*(target->canvas), canvasStride, frameLeft + x, frameTop + y, srcPixel[2], srcPixel[1], srcPixel[0], srcPixel[3]);
                                        }
                                    }
                                }
                            } else { /** Assume WebP */
                                if (frameWidth == canvasWidth && frameHeight == canvasHeight) {
                                    memcpy(*(target->canvas), frameBuffer, canvasSize);
                                } else {
                                    UINT offsetX = (canvasWidth > frameWidth) ? (canvasWidth - frameWidth) / 2 : 0;
                                    UINT offsetY = (canvasHeight > frameHeight) ? (canvasHeight - frameHeight) / 2 : 0;
                                    for (UINT y = 0; y < frameHeight && (offsetY + y) < canvasHeight; y++) {
                                        memcpy(*(target->canvas) + ((offsetY + y) * canvasStride) + (offsetX * 4), frameBuffer + (y * frameStride), frameStride);
                                    }
                                }
                            }
                        }
                        free(frameBuffer);
                    }
                }
                pConverter->lpVtbl->Release(pConverter);
            }
            
            HICON hIcon = CreateIconFromPBGRA(pFactory, *(target->canvas), canvasWidth, canvasHeight, cx, cy);
            if (hIcon) {
                target->icons[*(target->count)] = hIcon;
                target->delays[*(target->count)] = delayMs;
                (*(target->count))++;
            }

            if (isGif) {
                prevDisposal = disposal;
                prevLeft = frameLeft;
                prevTop = frameTop;
                prevWidth = frameWidth;
                prevHeight = frameHeight;
            }
            pFrame->lpVtbl->Release(pFrame);
        }
    }

    pDecoder->lpVtbl->Release(pDecoder);
    if (pFactory) pFactory->lpVtbl->Release(pFactory);
    if (SUCCEEDED(hrInit)) CoUninitialize();

    if (*(target->count) > 0) {
        *(target->isAnimatedFlag) = TRUE;
        *(target->index) = 0;
    }
}

/** @brief Generic routine to load sequential icon frames from a folder */
static void LoadIconsFromFolder(const char* utf8Folder, HICON* icons, int* count) {
    wchar_t wFolder[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Folder, -1, wFolder, MAX_PATH);

    typedef struct { int hasNum; int num; wchar_t name[MAX_PATH]; wchar_t path[MAX_PATH]; } AnimFile;
    AnimFile files[MAX_TRAY_FRAMES];
    int fileCount = 0;

    void AddFilesWithPattern(const wchar_t* pattern) {
        WIN32_FIND_DATAW ffd;
        HANDLE hFind = FindFirstFileW(pattern, &ffd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                wchar_t* dot = wcsrchr(ffd.cFileName, L'.');
                if (!dot) continue;
                size_t nameLen = (size_t)(dot - ffd.cFileName);
                if (nameLen == 0 || nameLen >= MAX_PATH) continue;

                int hasNum = 0, numVal = 0;
                for (size_t i = 0; i < nameLen; ++i) {
                    if (iswdigit(ffd.cFileName[i])) {
                        hasNum = 1;
                        numVal = 0;
                        while (i < nameLen && iswdigit(ffd.cFileName[i])) {
                            numVal = numVal * 10 + (ffd.cFileName[i] - L'0');
                            i++;
                        }
                        break;
                    }
                }

                if (fileCount < MAX_TRAY_FRAMES) {
                    files[fileCount].hasNum = hasNum;
                    files[fileCount].num = numVal;
                    wcsncpy(files[fileCount].name, ffd.cFileName, nameLen);
                    files[fileCount].name[nameLen] = L'\0';
                    _snwprintf_s(files[fileCount].path, MAX_PATH, _TRUNCATE, L"%s\\%s", wFolder, ffd.cFileName);
                    fileCount++;
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);
        }
    }

    const wchar_t* patterns[] = { L"\\*.ico", L"\\*.png", L"\\*.bmp", L"\\*.jpg", L"\\*.jpeg", L"\\*.webp", L"\\*.tif", L"\\*.tiff" };
    for (int i = 0; i < sizeof(patterns)/sizeof(patterns[0]); ++i) {
        wchar_t wSearch[MAX_PATH] = {0};
        _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s%s", wFolder, patterns[i]);
        AddFilesWithPattern(wSearch);
    }

    if (fileCount == 0) return;

    int cmpAnimFile(const void* a, const void* b) {
        const AnimFile* fa = (const AnimFile*)a;
        const AnimFile* fb = (const AnimFile*)b;
        if (fa->hasNum && fb->hasNum) {
            if (fa->num != fb->num) return fa->num < fb->num ? -1 : 1;
        }
        return _wcsicmp(fa->name, fb->name);
    }
    qsort(files, (size_t)fileCount, sizeof(AnimFile), cmpAnimFile);

    for (int i = 0; i < fileCount; ++i) {
        HICON hIcon = NULL;
        const wchar_t* ext = wcsrchr(files[i].path, L'.');
        if (ext && (_wcsicmp(ext, L".ico") == 0)) {
            hIcon = (HICON)LoadImageW(NULL, files[i].path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        } else {
            int cx = GetSystemMetrics(SM_CXSMICON);
            int cy = GetSystemMetrics(SM_CYSMICON);
            
            IWICImagingFactory* pFactory = NULL;
            HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
            if (SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&pFactory)) && pFactory) {
                IWICBitmapDecoder* pDecoder = NULL;
                if (SUCCEEDED(pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, files[i].path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder)) && pDecoder) {
                    IWICBitmapFrameDecode* pFrame = NULL;
                    if (SUCCEEDED(pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame)) && pFrame) {
                        hIcon = CreateIconFromWICSource(pFactory, (IWICBitmapSource*)pFrame, cx, cy);
                        pFrame->lpVtbl->Release(pFrame);
                    }
                    pDecoder->lpVtbl->Release(pDecoder);
                }
                pFactory->lpVtbl->Release(pFactory);
            }
            if (SUCCEEDED(hrInit)) CoUninitialize();
        }
        if (hIcon) {
            icons[(*count)++] = hIcon;
        }
    }
}

/** @brief Unified animation loading routine for tray and preview */
static void LoadAnimationByName(const char* name, BOOL isPreview) {
    HICON* icons;
    int* count;
    int* index;
    BOOL* isAnimatedFlag;
    UINT* delays;
    BYTE** canvas;

    if (isPreview) {
        FreeIconSet(g_previewIcons, &g_previewCount, &g_previewIndex, &g_isPreviewAnimated, &g_previewAnimCanvas, FALSE);
        icons = g_previewIcons;
        count = &g_previewCount;
        index = &g_previewIndex;
        isAnimatedFlag = &g_isPreviewAnimated;
        delays = g_previewFrameDelaysMs;
        canvas = &g_previewAnimCanvas;
    } else {
        FreeIconSet(g_trayIcons, &g_trayIconCount, &g_trayIconIndex, &g_isAnimated, &g_animCanvas, TRUE);
        icons = g_trayIcons;
        count = &g_trayIconCount;
        index = &g_trayIconIndex;
        isAnimatedFlag = &g_isAnimated;
        delays = g_frameDelaysMs;
        canvas = &g_animCanvas;
    }

    if (!name || !*name) return;

    if (_stricmp(name, "__logo__") == 0) {
        HICON hIcon = LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_CATIME));
        if (hIcon) {
            icons[(*count)++] = hIcon;
        }
    } else if (IsGifSelection(name) || IsWebPSelection(name)) {
        char filePath[MAX_PATH] = {0};
        BuildAnimationFolder(name, filePath, sizeof(filePath));
        DecodeTarget target = { icons, count, index, delays, isAnimatedFlag, canvas };
        LoadAnimatedImage(filePath, &target);
    } else {
        char folder[MAX_PATH] = {0};
        BuildAnimationFolder(name, folder, sizeof(folder));
        LoadIconsFromFolder(folder, icons, count);
    }
}

/** @brief Load sequential icon frames from .ico and .png files */
static void LoadTrayIcons(void) {
    LoadAnimationByName(g_animationName, FALSE);
}

/** @brief Advance to next icon frame and apply to tray */
static void AdvanceTrayFrame(void) {
    if (!g_trayHwnd) return;
    int count = g_isPreviewActive ? g_previewCount : g_trayIconCount;
    if (count <= 0) return;
    if (g_isPreviewActive) {
        if (g_previewIndex >= g_previewCount) g_previewIndex = 0;
    } else {
        if (g_trayIconIndex >= g_trayIconCount) g_trayIconIndex = 0;
    }

    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_trayHwnd;
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON;
    nid.hIcon = g_isPreviewActive ? g_previewIcons[g_previewIndex] : g_trayIcons[g_trayIconIndex];

    Shell_NotifyIconW(NIM_MODIFY, &nid);

    if (g_isPreviewActive) {
        g_previewIndex = (g_previewIndex + 1) % g_previewCount;

        /** For GIF preview, honor per-frame delay */
        if (g_isPreviewAnimated && g_trayHwnd) {
            int nextPrev = g_previewIndex;
            UINT delayPrev = g_previewFrameDelaysMs[nextPrev];
            if (delayPrev == 0) delayPrev = g_trayInterval > 0 ? g_trayInterval : 150;
            KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
            SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, delayPrev, (TIMERPROC)TrayAnimTimerProc);
        }
    } else {
        g_trayIconIndex = (g_trayIconIndex + 1) % g_trayIconCount;
    }

    /** If current animation is GIF, adjust timer to next frame delay */
    if (!g_isPreviewActive && g_isAnimated && g_trayHwnd) {
        int nextIndex = g_trayIconIndex;
        UINT delay = g_frameDelaysMs[nextIndex];
        if (delay == 0) delay = g_trayInterval > 0 ? g_trayInterval : 150;
        KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
        SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, delay, (TIMERPROC)TrayAnimTimerProc);
    }
}

/** @brief Window-proc level timer callback shim */
static void CALLBACK TrayAnimTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)msg; (void)id; (void)time;
    AdvanceTrayFrame();
}

void StartTrayAnimation(HWND hwnd, UINT intervalMs) {
    g_trayHwnd = hwnd;
    g_trayInterval = intervalMs > 0 ? intervalMs : 150; /** default ~6-7 fps */
    g_isPreviewActive = FALSE;
    g_previewCount = 0;
    g_previewIndex = 0;

    /** Read current animation name from config */
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char nameBuf[64] = {0};
    ReadIniString(INI_SECTION_OPTIONS, "ANIMATION_NAME", "%LOCALAPPDATA%\\Catime\\resources\\animations\\cat", nameBuf, sizeof(nameBuf), config_path);
    if (nameBuf[0] != '\0') {
        const char* prefix = "%LOCALAPPDATA%\\Catime\\resources\\animations\\";
        if (_stricmp(nameBuf, "__logo__") == 0) {
            strncpy(g_animationName, "__logo__", sizeof(g_animationName) - 1);
            g_animationName[sizeof(g_animationName) - 1] = '\0';
        } else if (_strnicmp(nameBuf, prefix, (int)strlen(prefix)) == 0) {
            const char* rel = nameBuf + strlen(prefix);
            if (*rel) {
                strncpy(g_animationName, rel, sizeof(g_animationName) - 1);
                g_animationName[sizeof(g_animationName) - 1] = '\0';
            }
        } else {
            strncpy(g_animationName, nameBuf, sizeof(g_animationName) - 1);
            g_animationName[sizeof(g_animationName) - 1] = '\0';
        }
    }

    LoadTrayIcons();

    if (g_trayIconCount > 0) {
        AdvanceTrayFrame();
        /** For GIF, honor first frame delay if available */
        UINT firstDelay = (g_isAnimated && g_frameDelaysMs[0] > 0) ? g_frameDelaysMs[0] : g_trayInterval;
        SetTimer(hwnd, TRAY_ANIM_TIMER_ID, firstDelay, (TIMERPROC)TrayAnimTimerProc);
    }
}

void StopTrayAnimation(HWND hwnd) {
    KillTimer(hwnd, TRAY_ANIM_TIMER_ID);
    FreeIconSet(g_trayIcons, &g_trayIconCount, &g_trayIconIndex, &g_isAnimated, &g_animCanvas, TRUE);
    FreeIconSet(g_previewIcons, &g_previewCount, &g_previewIndex, &g_isPreviewAnimated, &g_previewAnimCanvas, FALSE);
    g_trayHwnd = NULL;
}

/**
 * @brief Get current animation folder name
 */
const char* GetCurrentAnimationName(void) {
    return g_animationName;
}

/**
 * @brief Set and persist current animation folder; reload frames
 */
BOOL SetCurrentAnimationName(const char* name) {
    if (!name || !*name) return FALSE;

    /** Validate selection: either a folder with images, or a single .gif file existing */
    char folder[MAX_PATH] = {0};
    if (_stricmp(name, "__logo__") == 0) {
        strncpy(g_animationName, name, sizeof(g_animationName) - 1);
        g_animationName[sizeof(g_animationName) - 1] = '\0';
        char config_path[MAX_PATH] = {0};
        GetConfigPath(config_path, sizeof(config_path));
        WriteIniString(INI_SECTION_OPTIONS, "ANIMATION_NAME", "__logo__", config_path);
        LoadTrayIcons();
        g_trayIconIndex = 0;
        if (g_trayHwnd && g_trayIconCount > 0) {
            AdvanceTrayFrame();
            if (!IsWindow(g_trayHwnd)) return TRUE;
            KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
            SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, g_trayInterval ? g_trayInterval : 150, (TIMERPROC)TrayAnimTimerProc);
        }
        return TRUE;
    }
    BuildAnimationFolder(name, folder, sizeof(folder));
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, folder, -1, wPath, MAX_PATH);

    DWORD attrs = GetFileAttributesW(wPath);
    BOOL valid = FALSE;
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            wchar_t wSearch[MAX_PATH] = {0};
            _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", wPath);
            WIN32_FIND_DATAW ffd; HANDLE hFind = FindFirstFileW(wSearch, &ffd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                    wchar_t* ext = wcsrchr(ffd.cFileName, L'.');
                    if (ext && (_wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0 || _wcsicmp(ext, L".tif") == 0 || _wcsicmp(ext, L".tiff") == 0)) {
                        valid = TRUE; break;
                    }
                } while (FindNextFileW(hFind, &ffd));
                FindClose(hFind);
            }
        } else if (IsGifSelection(name)) {
            valid = TRUE; /** a valid single GIF file */
        } else if (IsWebPSelection(name)) {
            valid = TRUE; /** a valid single WebP file */
        }
    }
    if (!valid) return FALSE;

    strncpy(g_animationName, name, sizeof(g_animationName) - 1);
    g_animationName[sizeof(g_animationName) - 1] = '\0';

    /** Persist to config */
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char animPath[MAX_PATH];
    snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", g_animationName);
    WriteIniString(INI_SECTION_OPTIONS, "ANIMATION_NAME", animPath, config_path);

    /** Reload frames and reset index; ensure timer is running */
    LoadTrayIcons();
    g_trayIconIndex = 0;
    if (g_trayHwnd && g_trayIconCount > 0) {
        AdvanceTrayFrame();
        if (!IsWindow(g_trayHwnd)) return TRUE;
        KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
        UINT firstDelay = (g_isAnimated && g_frameDelaysMs[0] > 0) ? g_frameDelaysMs[0] : g_trayInterval;
        SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, firstDelay, (TIMERPROC)TrayAnimTimerProc);
    }
    return TRUE;
}


/** Load preview icons for folder and enable preview mode (no persistence) */
void StartAnimationPreview(const char* name) {
    if (!name || !*name) return;

    LoadAnimationByName(name, TRUE);

    if (g_previewCount > 0) {
        g_isPreviewActive = TRUE;
        g_previewIndex = 0;
        if (g_trayHwnd) {
            AdvanceTrayFrame();
            if (g_isPreviewAnimated) {
                UINT firstDelay = g_previewFrameDelaysMs[0] > 0 ? g_previewFrameDelaysMs[0] : (g_trayInterval ? g_trayInterval : 150);
                KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
                SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, firstDelay, (TIMERPROC)TrayAnimTimerProc);
            }
        }
    }
}

void CancelAnimationPreview(void) {
    if (!g_isPreviewActive) return;
    g_isPreviewActive = FALSE;
    FreeIconSet(g_previewIcons, &g_previewCount, &g_previewIndex, &g_isPreviewAnimated, &g_previewAnimCanvas, FALSE);
    if (g_trayHwnd) {
        AdvanceTrayFrame();
        /** Restore timer for normal animation if needed */
        if (g_isAnimated) {
            UINT firstDelay = g_frameDelaysMs[g_trayIconIndex] > 0 ? g_frameDelaysMs[g_trayIconIndex] : (g_trayInterval ? g_trayInterval : 150);
            KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
            SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, firstDelay, (TIMERPROC)TrayAnimTimerProc);
        } else {
            KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
            SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, g_trayInterval ? g_trayInterval : 150, (TIMERPROC)TrayAnimTimerProc);
        }
    }
}


static void OpenAnimationsFolder(void) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, base, -1, wPath, MAX_PATH);
    ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
}

BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id) {
    if (id == CLOCK_IDM_ANIMATIONS_OPEN_DIR) {
        OpenAnimationsFolder();
        return TRUE;
    }
    if (id == CLOCK_IDM_ANIMATIONS_USE_LOGO) {
        SetCurrentAnimationName("__logo__");
        return TRUE;
    }
    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_BASE + 1000) {
        char animRootUtf8[MAX_PATH] = {0};
        GetAnimationsFolderPath(animRootUtf8, sizeof(animRootUtf8));
        wchar_t wRoot[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, animRootUtf8, -1, wRoot, MAX_PATH);
        wchar_t wSearch[MAX_PATH] = {0};
        _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", wRoot);

        /** Recursive helper function to handle menu commands */
        BOOL HandleAnimationByIdRecursive(const wchar_t* folderPathW, const char* folderPathUtf8, UINT* nextIdPtr, UINT targetId) {
            wchar_t wSearch[MAX_PATH] = {0};
            _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
            
            WIN32_FIND_DATAW ffd;
            HANDLE hFind = FindFirstFileW(wSearch, &ffd);
            if (hFind == INVALID_HANDLE_VALUE) return FALSE;
            
            do {
                if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
                
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    /** Recursively handle subdirectories */
                    char subFolderUtf8[MAX_PATH] = {0};
                    WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, subFolderUtf8, MAX_PATH, NULL, NULL);
                    
                    wchar_t wSubFolderPath[MAX_PATH] = {0};
                    _snwprintf_s(wSubFolderPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPathW, ffd.cFileName);
                    
                    char subFolderPathUtf8[MAX_PATH] = {0};
                    _snprintf_s(subFolderPathUtf8, MAX_PATH, _TRUNCATE, "%s\\%s", folderPathUtf8, subFolderUtf8);
                    
                    if (HandleAnimationByIdRecursive(wSubFolderPath, subFolderPathUtf8, nextIdPtr, targetId)) {
                        FindClose(hFind);
                        return TRUE;
                    }
                    
                    /** Check if this is a simple folder option (no content) */
                    if (*nextIdPtr == targetId) {
                        BOOL result = SetCurrentAnimationName(subFolderPathUtf8);
                        FindClose(hFind);
                        return result;
                    }
                    (*nextIdPtr)++;
                } else {
                    /** Handle GIF and WebP files */
                    wchar_t* ext = wcsrchr(ffd.cFileName, L'.');
                    if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0)) {
                        if (*nextIdPtr == targetId) {
                            char fileUtf8[MAX_PATH] = {0};
                            WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, fileUtf8, MAX_PATH, NULL, NULL);
                            
                            char relPath[MAX_PATH] = {0};
                            _snprintf_s(relPath, MAX_PATH, _TRUNCATE, "%s\\%s", folderPathUtf8, fileUtf8);
                            BOOL result = SetCurrentAnimationName(relPath);
                            FindClose(hFind);
                            return result;
                        }
                        (*nextIdPtr)++;
                    }
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);
            
            return FALSE;
        }

        /** Process folders and their contents for menu command handling */
        WIN32_FIND_DATAW ffd; HANDLE hFind = FindFirstFileW(wSearch, &ffd);
        UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
        BOOL changed = FALSE;
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    char folderUtf8[MAX_PATH] = {0};
                    WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, folderUtf8, MAX_PATH, NULL, NULL);

                    wchar_t wFolderPath[MAX_PATH] = {0};
                    _snwprintf_s(wFolderPath, MAX_PATH, _TRUNCATE, L"%s\\%s", wRoot, ffd.cFileName);
                    
                    if (HandleAnimationByIdRecursive(wFolderPath, folderUtf8, &nextId, id)) {
                        FindClose(hFind);
                        return TRUE;
                    }
                    
                    /** Check if this is a simple folder option (no content) */
                    if (nextId == id) {
                        changed = SetCurrentAnimationName(folderUtf8);
                        FindClose(hFind);
                        return changed ? TRUE : FALSE;
                    }
                    nextId++;
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);
        }

        /** Process standalone .gif and .webp files for menu command handling */
        WIN32_FIND_DATAW ffd2; HANDLE hFind2 = FindFirstFileW(wSearch, &ffd2);
        if (hFind2 != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(ffd2.cFileName, L".") == 0 || wcscmp(ffd2.cFileName, L"..") == 0) continue;
                if (!(ffd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    wchar_t* ext = wcsrchr(ffd2.cFileName, L'.');
                    if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0)) {
                        if (nextId == id) {
                            char fileUtf8[MAX_PATH] = {0};
                            WideCharToMultiByte(CP_UTF8, 0, ffd2.cFileName, -1, fileUtf8, MAX_PATH, NULL, NULL);
                            changed = SetCurrentAnimationName(fileUtf8);
                            FindClose(hFind2);
                            return changed ? TRUE : FALSE;
                        }
                        nextId++;
                    }
                }
            } while (FindNextFileW(hFind2, &ffd2));
            FindClose(hFind2);
        }
        return changed ? TRUE : FALSE;
    }
    return FALSE;
}

