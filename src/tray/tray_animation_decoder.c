/**
 * @file tray_animation_decoder.c
 * @brief Image decoding implementation using WIC
 */

#include "tray/tray_animation_decoder.h"
#include <propvarutil.h>
#include <objbase.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#define MAX_ANIMATION_PIXELS (4096u * 4096u)
#define MAX_ANIMATION_FRAMES 512u
#define MIN_FRAME_DELAY_MS 20u
#define MAX_FRAME_DELAY_MS 60000u
#define ICON_MASK_STACK_BYTES 2048u
#define ICON_PIXEL_STACK_BYTES 4096u
#define DECODED_ICON_FALLBACK_SIZE 16
#define DECODED_ICON_MAX_SIZE 256

static BOOL IsDecodeCancelRequested(HANDLE cancelEvent) {
    return cancelEvent && WaitForSingleObject(cancelEvent, 0) == WAIT_OBJECT_0;
}

static int ClampDecodedIconDimension(int value) {
    if (value <= 0) return DECODED_ICON_FALLBACK_SIZE;
    if (value > DECODED_ICON_MAX_SIZE) return DECODED_ICON_MAX_SIZE;
    return value;
}

static void NormalizeDecodedIconSize(int* iconWidth, int* iconHeight) {
    if (iconWidth) {
        *iconWidth = ClampDecodedIconDimension(*iconWidth);
    }
    if (iconHeight) {
        *iconHeight = ClampDecodedIconDimension(*iconHeight);
    }
}

static UINT ClampDecodedFrameDelayMs(UINT delayMs) {
    if (delayMs < MIN_FRAME_DELAY_MS) return MIN_FRAME_DELAY_MS;
    if (delayMs > MAX_FRAME_DELAY_MS) return MAX_FRAME_DELAY_MS;
    return delayMs;
}

static HBITMAP CreateAlphaIconMask(const BYTE* bgraPixels, int cx, int cy) {
    if (!bgraPixels || cx <= 0 || cy <= 0) return NULL;

    SIZE_T stride = (SIZE_T)(((cx + 15) / 16) * 2);
    SIZE_T size = stride * (SIZE_T)cy;
    BYTE stackMaskBits[ICON_MASK_STACK_BYTES];
    BYTE* maskBits = size <= sizeof(stackMaskBits)
        ? stackMaskBits
        : (BYTE*)malloc(size);
    if (!maskBits) return NULL;
    memset(maskBits, 0, size);

    for (int y = 0; y < cy; y++) {
        const BYTE* row = bgraPixels + ((SIZE_T)y * (SIZE_T)cx * 4u);
        for (int x = 0; x < cx; x++) {
            BYTE alpha = row[(SIZE_T)x * 4u + 3u];
            if (alpha == 0) {
                maskBits[(SIZE_T)y * stride + (SIZE_T)x / 8u] |= (BYTE)(0x80u >> (x & 7));
            }
        }
    }

    HBITMAP hMask = CreateBitmap(cx, cy, 1, 1, maskBits);
    if (maskBits != stackMaskBits) free(maskBits);
    return hMask;
}

static HBITMAP CreateOpaqueIconMask(int cx, int cy) {
    if (cx <= 0 || cy <= 0) return NULL;

    SIZE_T stride = (SIZE_T)(((cx + 15) / 16) * 2);
    SIZE_T size = stride * (SIZE_T)cy;
    BYTE stackMaskBits[ICON_MASK_STACK_BYTES];
    BYTE* maskBits = size <= sizeof(stackMaskBits)
        ? stackMaskBits
        : (BYTE*)malloc(size);
    if (!maskBits) return NULL;
    memset(maskBits, 0, size);

    HBITMAP hMask = CreateBitmap(cx, cy, 1, 1, maskBits);
    if (maskBits != stackMaskBits) free(maskBits);
    return hMask;
}

static BOOL CheckedImageBufferSize(UINT width, UINT height, UINT* stride, UINT* size) {
    if (!stride || !size || width == 0 || height == 0) return FALSE;
    if (width > MAX_ANIMATION_PIXELS / height) return FALSE;
    if (width > UINT32_MAX / 4u) return FALSE;

    UINT checkedStride = width * 4u;
    if (height > UINT32_MAX / checkedStride) return FALSE;

    *stride = checkedStride;
    *size = height * checkedStride;
    return TRUE;
}

static BOOL IsImageDimensionAllowed(UINT width, UINT height) {
    UINT stride = 0;
    UINT size = 0;
    return CheckedImageBufferSize(width, height, &stride, &size);
}

static HICON CreateIconFromExactPBGRA(const BYTE* pixels, UINT width, UINT height) {
    UINT stride = 0;
    UINT size = 0;
    if (!pixels || !CheckedImageBufferSize(width, height, &stride, &size)) {
        return NULL;
    }

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = (LONG)width;
    bi.bmiHeader.biHeight = -(LONG)height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    VOID* pvBits = NULL;
    HBITMAP hbmColor = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    if (!hbmColor || !pvBits) {
        if (hbmColor) DeleteObject(hbmColor);
        return NULL;
    }

    memcpy(pvBits, pixels, size);

    ICONINFO ii;
    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = CreateAlphaIconMask((const BYTE*)pvBits, (int)width, (int)height);
    if (!ii.hbmMask) {
        ii.hbmMask = CreateOpaqueIconMask((int)width, (int)height);
    }

    HICON hIcon = CreateIconIndirect(&ii);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    DeleteObject(hbmColor);
    return hIcon;
}

/**
 * @brief Initialize decoded animation structure
 */
void DecodedAnimation_Init(DecodedAnimation* anim) {
    if (!anim) return;
    anim->icons = NULL;
    anim->count = 0;
    anim->delays = NULL;
    anim->isAnimated = FALSE;
    anim->canvasWidth = 0;
    anim->canvasHeight = 0;
    anim->canvas = NULL;
}

/**
 * @brief Free all resources
 */
void DecodedAnimation_Free(DecodedAnimation* anim) {
    if (!anim) return;
    
    if (anim->icons) {
        for (int i = 0; i < anim->count; i++) {
            if (anim->icons[i]) {
                DestroyIcon(anim->icons[i]);
            }
        }
        free(anim->icons);
        anim->icons = NULL;
    }
    
    if (anim->delays) {
        free(anim->delays);
        anim->delays = NULL;
    }
    
    if (anim->canvas) {
        free(anim->canvas);
        anim->canvas = NULL;
    }
    
    anim->count = 0;
    anim->isAnimated = FALSE;
    anim->canvasWidth = 0;
    anim->canvasHeight = 0;
}

/**
 * @brief Blend pixel with alpha compositing (source over)
 */
static void BlendPixelInto(BYTE* pixel, BYTE r, BYTE g, BYTE b, BYTE a) {
    if (a == 0) return;

    if (a == 255) {
        pixel[0] = b;
        pixel[1] = g;
        pixel[2] = r;
        pixel[3] = a;
    } else {
        UINT srcAlpha = a;
        UINT dstAlpha = pixel[3];

        if (dstAlpha == 0) {
            pixel[0] = b;
            pixel[1] = g;
            pixel[2] = r;
            pixel[3] = a;
        } else {
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

void BlendPixel(BYTE* canvas, UINT canvasStride, UINT x, UINT y,
                BYTE r, BYTE g, BYTE b, BYTE a) {
    BYTE* pixel = canvas + (y * canvasStride) + (x * 4);
    BlendPixelInto(pixel, r, g, b, a);
}

/**
 * @brief Clear canvas rectangle (GIF disposal mode 2)
 */
void ClearCanvasRect(BYTE* canvas, UINT canvasWidth, UINT canvasHeight,
                     UINT left, UINT top, UINT width, UINT height,
                     BYTE bgR, BYTE bgG, BYTE bgB, BYTE bgA) {
    if (!canvas || canvasWidth == 0 || canvasHeight == 0 || width == 0 || height == 0) return;
    if (left >= canvasWidth || top >= canvasHeight) return;

    UINT clearWidth = width;
    UINT clearHeight = height;
    if (clearWidth > canvasWidth - left) {
        clearWidth = canvasWidth - left;
    }
    if (clearHeight > canvasHeight - top) {
        clearHeight = canvasHeight - top;
    }

    SIZE_T canvasStride = (SIZE_T)canvasWidth * 4u;
    SIZE_T clearBytes = (SIZE_T)clearWidth * 4u;
    BYTE* row = canvas + ((SIZE_T)top * canvasStride) + ((SIZE_T)left * 4u);

    if (bgR == 0 && bgG == 0 && bgB == 0 && bgA == 0) {
        for (UINT y = 0; y < clearHeight; y++) {
            memset(row + ((SIZE_T)y * canvasStride), 0, clearBytes);
        }
    } else {
        DWORD pixelValue = ((DWORD)bgB) |
                           ((DWORD)bgG << 8) |
                           ((DWORD)bgR << 16) |
                           ((DWORD)bgA << 24);
        for (UINT y = 0; y < clearHeight; y++) {
            DWORD* pixel = (DWORD*)(row + ((SIZE_T)y * canvasStride));
            for (UINT x = 0; x < clearWidth; x++) {
                pixel[x] = pixelValue;
            }
        }
    }
}

/**
 * @brief Create icon from WIC bitmap source
 */
HICON CreateIconFromWICSource(IWICImagingFactory* pFactory,
                               IWICBitmapSource* source,
                               int cx, int cy) {
    if (!pFactory || !source) return NULL;
    NormalizeDecodedIconSize(&cx, &cy);

    UINT srcW = 0, srcH = 0;
    if (FAILED(source->lpVtbl->GetSize(source, &srcW, &srcH)) || srcW == 0 || srcH == 0) {
        srcW = (UINT)cx;
        srcH = (UINT)cy;
    } else if (!IsImageDimensionAllowed(srcW, srcH)) {
        return NULL;
    }

    HICON hIcon = NULL;
    IWICBitmapScaler* pScaler = NULL;
    IWICFormatConverter* pConverter = NULL;

    HRESULT hr = pFactory->lpVtbl->CreateBitmapScaler(pFactory, &pScaler);
    if (SUCCEEDED(hr) && pScaler) {
        /* Aspect-preserving scale */
        double scaleX = (double)cx / (double)srcW;
        double scaleY = (double)cy / (double)srcH;
        double scale = scaleX < scaleY ? scaleX : scaleY;
        if (scale <= 0.0) scale = 1.0;

        UINT dstW = (UINT)((double)srcW * scale + 0.5);
        UINT dstH = (UINT)((double)srcH * scale + 0.5);
        if (dstW == 0) dstW = 1;
        if (dstH == 0) dstH = 1;

        hr = pScaler->lpVtbl->Initialize(pScaler, source, dstW, dstH, WICBitmapInterpolationModeFant);
        if (SUCCEEDED(hr)) {
            hr = pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter);
            if (SUCCEEDED(hr) && pConverter) {
                hr = pConverter->lpVtbl->Initialize(pConverter,
                                                    (IWICBitmapSource*)pScaler,
                                                    &GUID_WICPixelFormat32bppPBGRA,
                                                    WICBitmapDitherTypeNone,
                                                    NULL, 0.0,
                                                    WICBitmapPaletteTypeCustom);
                if (SUCCEEDED(hr)) {
                    UINT colorStride = 0;
                    UINT colorSize = 0;
                    if (CheckedImageBufferSize((UINT)cx, (UINT)cy, &colorStride, &colorSize)) {
                        UINT scaledStride = 0;
                        UINT scaledSize = 0;
                        if (CheckedImageBufferSize(dstW, dstH, &scaledStride, &scaledSize)) {
                            BITMAPINFO bi;
                            ZeroMemory(&bi, sizeof(bi));
                            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                            bi.bmiHeader.biWidth = cx;
                            bi.bmiHeader.biHeight = -cy;
                            bi.bmiHeader.biPlanes = 1;
                            bi.bmiHeader.biBitCount = 32;
                            bi.bmiHeader.biCompression = BI_RGB;

                            VOID* pvBits = NULL;
                            HBITMAP hbmColor = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
                            if (hbmColor && pvBits) {
                                ZeroMemory(pvBits, colorSize);

                                BYTE stackPixels[ICON_PIXEL_STACK_BYTES];
                                BYTE* tmp = scaledSize <= sizeof(stackPixels)
                                    ? stackPixels
                                    : (BYTE*)malloc(scaledSize);
                                BOOL pixelsCopied = FALSE;
                                if (tmp) {
                                    if (SUCCEEDED(pConverter->lpVtbl->CopyPixels(pConverter, NULL, scaledStride, scaledSize, tmp))) {
                                        int xoff = (cx - (int)dstW) / 2;
                                        int yoff = (cy - (int)dstH) / 2;
                                        if (xoff < 0) xoff = 0;
                                        if (yoff < 0) yoff = 0;

                                        UINT copyWidth = dstW;
                                        UINT copyHeight = dstH;
                                        if ((UINT)xoff < (UINT)cx && (UINT)yoff < (UINT)cy) {
                                            if (copyWidth > (UINT)cx - (UINT)xoff) {
                                                copyWidth = (UINT)cx - (UINT)xoff;
                                            }
                                            if (copyHeight > (UINT)cy - (UINT)yoff) {
                                                copyHeight = (UINT)cy - (UINT)yoff;
                                            }
                                        } else {
                                            copyWidth = 0;
                                            copyHeight = 0;
                                        }

                                        UINT copyBytes = copyWidth * 4;
                                        for (UINT y = 0; copyBytes > 0 && y < copyHeight; ++y) {
                                            BYTE* dstRow = (BYTE*)pvBits + ((SIZE_T)(yoff + (int)y) * colorStride) + ((SIZE_T)xoff * 4u);
                                            const BYTE* srcRow = tmp + y * scaledStride;
                                            memcpy(dstRow, srcRow, copyBytes);
                                        }
                                        pixelsCopied = TRUE;
                                    }
                                    if (tmp != stackPixels) free(tmp);
                                }

                                if (pixelsCopied) {
                                    ICONINFO ii;
                                    ZeroMemory(&ii, sizeof(ii));
                                    ii.fIcon = TRUE;
                                    ii.hbmColor = hbmColor;
                                    ii.hbmMask = CreateAlphaIconMask((const BYTE*)pvBits, cx, cy);
                                    if (!ii.hbmMask) {
                                        ii.hbmMask = CreateOpaqueIconMask(cx, cy);
                                    }

                                    hIcon = CreateIconIndirect(&ii);
                                    if (ii.hbmMask) DeleteObject(ii.hbmMask);
                                }
                            }
                            if (hbmColor) DeleteObject(hbmColor);
                        }
                    }
                }
            }
        }
    }

    if (pConverter) pConverter->lpVtbl->Release(pConverter);
    if (pScaler) pScaler->lpVtbl->Release(pScaler);
    return hIcon;
}

/**
 * @brief Create icon from PBGRA buffer
 */
HICON CreateIconFromPBGRA(IWICImagingFactory* pFactory,
                          const BYTE* canvasPixels,
                          UINT canvasWidth, UINT canvasHeight,
                          int cx, int cy) {
    if (!pFactory || !canvasPixels || canvasWidth == 0 || canvasHeight == 0) {
        return NULL;
    }
    NormalizeDecodedIconSize(&cx, &cy);

    if (canvasWidth == (UINT)cx && canvasHeight == (UINT)cy) {
        return CreateIconFromExactPBGRA(canvasPixels, canvasWidth, canvasHeight);
    }

    HICON hIcon = NULL;
    IWICBitmap* pBitmap = NULL;

    UINT stride = 0;
    UINT size = 0;
    if (!CheckedImageBufferSize(canvasWidth, canvasHeight, &stride, &size)) {
        return NULL;
    }

    HRESULT hr = pFactory->lpVtbl->CreateBitmapFromMemory(pFactory,
        canvasWidth, canvasHeight, &GUID_WICPixelFormat32bppPBGRA,
        stride, size, (BYTE*)canvasPixels, &pBitmap);
        
    if (SUCCEEDED(hr) && pBitmap) {
        hIcon = CreateIconFromWICSource(pFactory, (IWICBitmapSource*)pBitmap, cx, cy);
        pBitmap->lpVtbl->Release(pBitmap);
    }

    return hIcon;
}

/**
 * @brief Decode animated GIF/WebP
 */
BOOL DecodeAnimatedImage(const char* utf8Path, DecodedAnimation* anim,
                         MemoryPool* pool, int iconWidth, int iconHeight) {
    return DecodeAnimatedImageWithCancel(utf8Path, anim, pool, iconWidth, iconHeight, NULL);
}

BOOL DecodeAnimatedImageWithCancel(const char* utf8Path, DecodedAnimation* anim,
                                   MemoryPool* pool, int iconWidth, int iconHeight,
                                   HANDLE cancelEvent) {
    if (!utf8Path || !anim) return FALSE;
    NormalizeDecodedIconSize(&iconWidth, &iconHeight);
    if (IsDecodeCancelRequested(cancelEvent)) return FALSE;

    wchar_t wPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH) <= 0) {
        return FALSE;
    }
    if (IsDecodeCancelRequested(cancelEvent)) return FALSE;

    HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* pFactory = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, 
                                   &IID_IWICImagingFactory, (void**)&pFactory);
    if (FAILED(hr) || !pFactory) {
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return FALSE;
    }

    IWICBitmapDecoder* pDecoder = NULL;
    hr = pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, wPath, NULL, 
                                                      GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr) || !pDecoder) {
        pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return FALSE;
    }

    /* Detect format */
    GUID containerFormat;
    BOOL isGif = FALSE;
    if (SUCCEEDED(pDecoder->lpVtbl->GetContainerFormat(pDecoder, &containerFormat))) {
        if (IsEqualGUID(&containerFormat, &GUID_ContainerFormatGif)) {
            isGif = TRUE;
        }
    }

    /* Get canvas size */
    UINT canvasWidth = 0, canvasHeight = 0;
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

    if (canvasWidth == 0 || canvasHeight == 0) {
        IWICBitmapFrameDecode* pFirstFrame = NULL;
        if (SUCCEEDED(pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFirstFrame)) && pFirstFrame) {
            pFirstFrame->lpVtbl->GetSize(pFirstFrame, &canvasWidth, &canvasHeight);
            pFirstFrame->lpVtbl->Release(pFirstFrame);
        }
    }

    if (canvasWidth == 0 || canvasHeight == 0) {
        pDecoder->lpVtbl->Release(pDecoder);
        pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return FALSE;
    }

    anim->canvasWidth = canvasWidth;
    anim->canvasHeight = canvasHeight;

    UINT canvasStride = 0;
    UINT canvasSize = 0;
    if (!CheckedImageBufferSize(canvasWidth, canvasHeight, &canvasStride, &canvasSize)) {
        pDecoder->lpVtbl->Release(pDecoder);
        pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return FALSE;
    }

    anim->canvas = (BYTE*)malloc(canvasSize);
    if (!anim->canvas) {
        pDecoder->lpVtbl->Release(pDecoder);
        pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return FALSE;
    }
    memset(anim->canvas, 0, canvasSize);

    /* Allocate frame arrays */
    UINT frameCount = 0;
    if (FAILED(pDecoder->lpVtbl->GetFrameCount(pDecoder, &frameCount)) || frameCount == 0) {
        DecodedAnimation_Free(anim);
        pDecoder->lpVtbl->Release(pDecoder);
        pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return FALSE;
    }
    if (frameCount > MAX_ANIMATION_FRAMES) {
        frameCount = MAX_ANIMATION_FRAMES;
    }

    anim->icons = (HICON*)calloc(frameCount, sizeof(HICON));
    anim->delays = (UINT*)calloc(frameCount, sizeof(UINT));
    if (!anim->icons || !anim->delays) {
        DecodedAnimation_Free(anim);
        pDecoder->lpVtbl->Release(pDecoder);
        pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return FALSE;
    }

    UINT prevDisposal = 0;
    UINT prevLeft = 0, prevTop = 0, prevWidth = 0, prevHeight = 0;
    BOOL canceled = FALSE;

    for (UINT i = 0; i < frameCount; ++i) {
        if (IsDecodeCancelRequested(cancelEvent)) {
            canceled = TRUE;
            break;
        }

        IWICBitmapFrameDecode* pFrame = NULL;
        if (FAILED(pDecoder->lpVtbl->GetFrame(pDecoder, i, &pFrame)) || !pFrame) continue;

        /* Handle disposal */
        if (isGif && i > 0) {
            if (prevDisposal == 2) {
                ClearCanvasRect(anim->canvas, canvasWidth, canvasHeight, 
                              prevLeft, prevTop, prevWidth, prevHeight, 0, 0, 0, 0);
            }
        } else if (!isGif) {
            memset(anim->canvas, 0, canvasSize);
        }

        UINT delayMs = 100;
        UINT disposal = 0;
        UINT frameLeft = 0, frameTop = 0, frameWidth = 0, frameHeight = 0;

        /* Read frame metadata */
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
            } else {
                PropVariantInit(&var);
                if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/webp/delay", &var))) {
                    if (var.vt == VT_UI4) delayMs = var.ulVal;
                }
                PropVariantClear(&var);
            }
            pMeta->lpVtbl->Release(pMeta);
        }

        pFrame->lpVtbl->GetSize(pFrame, &frameWidth, &frameHeight);

        /* Decode and blend frame */
        BOOL frameDecoded = FALSE;
        IWICFormatConverter* pConverter = NULL;
        if (SUCCEEDED(pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter)) && pConverter) {
            if (SUCCEEDED(pConverter->lpVtbl->Initialize(pConverter, (IWICBitmapSource*)pFrame,
                                                        &GUID_WICPixelFormat32bppPBGRA,
                                                        WICBitmapDitherTypeNone, NULL, 0.0,
                                                        WICBitmapPaletteTypeCustom))) {
                UINT frameStride = 0;
                UINT frameBufferSize = 0;
                if (!CheckedImageBufferSize(frameWidth, frameHeight, &frameStride, &frameBufferSize)) {
                    pConverter->lpVtbl->Release(pConverter);
                    pFrame->lpVtbl->Release(pFrame);
                    continue;
                }

                BYTE* frameBuffer = MemoryPool_Alloc(pool, frameBufferSize);
                
                if (frameBuffer) {
                    if (IsDecodeCancelRequested(cancelEvent)) {
                        canceled = TRUE;
                        MemoryPool_Free(pool, frameBuffer);
                        pConverter->lpVtbl->Release(pConverter);
                        pFrame->lpVtbl->Release(pFrame);
                        break;
                    }

                    if (SUCCEEDED(pConverter->lpVtbl->CopyPixels(pConverter, NULL, frameStride, frameBufferSize, frameBuffer))) {
                        if (isGif) {
                            if (frameLeft < canvasWidth && frameTop < canvasHeight) {
                                UINT copyWidth = frameWidth;
                                UINT copyHeight = frameHeight;
                                if (copyWidth > canvasWidth - frameLeft) {
                                    copyWidth = canvasWidth - frameLeft;
                                }
                                if (copyHeight > canvasHeight - frameTop) {
                                    copyHeight = canvasHeight - frameTop;
                                }

                                for (UINT y = 0; y < copyHeight; y++) {
                                    if (IsDecodeCancelRequested(cancelEvent)) {
                                        canceled = TRUE;
                                        break;
                                    }
                                    BYTE* dstPixel = anim->canvas + ((frameTop + y) * canvasStride) + (frameLeft * 4);
                                    BYTE* srcPixel = frameBuffer + (y * frameStride);
                                    for (UINT x = 0; x < copyWidth; x++) {
                                        BlendPixelInto(dstPixel, srcPixel[2], srcPixel[1], srcPixel[0], srcPixel[3]);
                                        dstPixel += 4;
                                        srcPixel += 4;
                                    }
                                }
                            }
                        } else {
                            if (frameWidth == canvasWidth && frameHeight == canvasHeight) {
                                memcpy(anim->canvas, frameBuffer, canvasSize);
                            } else {
                                UINT offsetX = (canvasWidth > frameWidth) ? (canvasWidth - frameWidth) / 2 : 0;
                                UINT offsetY = (canvasHeight > frameHeight) ? (canvasHeight - frameHeight) / 2 : 0;
                                UINT copyWidth = (offsetX < canvasWidth) ? canvasWidth - offsetX : 0;
                                if (copyWidth > frameWidth) {
                                    copyWidth = frameWidth;
                                }
                                UINT copyBytes = copyWidth * 4;

                                for (UINT y = 0; copyBytes > 0 && y < frameHeight && (offsetY + y) < canvasHeight; y++) {
                                    if (IsDecodeCancelRequested(cancelEvent)) {
                                        canceled = TRUE;
                                        break;
                                    }
                                    memcpy(anim->canvas + ((offsetY + y) * canvasStride) + (offsetX * 4),
                                         frameBuffer + (y * frameStride), copyBytes);
                                }
                            }
                        }
                        frameDecoded = !canceled;
                    }
                    MemoryPool_Free(pool, frameBuffer);
                }
            }
            pConverter->lpVtbl->Release(pConverter);
        }

        /* Create icon from composited canvas */
        if (frameDecoded && !IsDecodeCancelRequested(cancelEvent)) {
            HICON hIcon = CreateIconFromPBGRA(pFactory, anim->canvas, canvasWidth, canvasHeight,
                                             iconWidth, iconHeight);
            if (hIcon) {
                anim->icons[anim->count] = hIcon;
                anim->delays[anim->count] = ClampDecodedFrameDelayMs(delayMs);
                anim->count++;
            }
        }

        if (isGif && frameDecoded) {
            prevDisposal = disposal;
            prevLeft = frameLeft;
            prevTop = frameTop;
            prevWidth = frameWidth;
            prevHeight = frameHeight;
        }
        
        pFrame->lpVtbl->Release(pFrame);
        if (canceled) {
            break;
        }
    }

    pDecoder->lpVtbl->Release(pDecoder);
    pFactory->lpVtbl->Release(pFactory);
    if (SUCCEEDED(hrInit)) CoUninitialize();

    if (IsDecodeCancelRequested(cancelEvent)) {
        canceled = TRUE;
    }

    if (!canceled && anim->count > 0) {
        anim->isAnimated = TRUE;
        return TRUE;
    }

    DecodedAnimation_Free(anim);
    return FALSE;
}

/**
 * @brief Decode static image with an existing WIC factory
 */
HICON DecodeStaticImageWithFactory(IWICImagingFactory* pFactory, const wchar_t* wPath,
                                   int iconWidth, int iconHeight) {
    if (!wPath) return NULL;
    NormalizeDecodedIconSize(&iconWidth, &iconHeight);

    const wchar_t* ext = wcsrchr(wPath, L'.');
    if (ext && _wcsicmp(ext, L".ico") == 0) {
        return (HICON)LoadImageW(NULL, wPath, IMAGE_ICON, iconWidth, iconHeight, LR_LOADFROMFILE);
    }

    if (!pFactory) return NULL;

    HICON hIcon = NULL;
    IWICBitmapDecoder* pDecoder = NULL;
    if (SUCCEEDED(pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, wPath, NULL,
                                                               GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder)) && pDecoder) {
        IWICBitmapFrameDecode* pFrame = NULL;
        if (SUCCEEDED(pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame)) && pFrame) {
            hIcon = CreateIconFromWICSource(pFactory, (IWICBitmapSource*)pFrame, iconWidth, iconHeight);
            pFrame->lpVtbl->Release(pFrame);
        }
        pDecoder->lpVtbl->Release(pDecoder);
    }

    return hIcon;
}

/**
 * @brief Decode static image to single icon
 */
HICON DecodeStaticImage(const char* utf8Path, int iconWidth, int iconHeight) {
    if (!utf8Path) return NULL;
    NormalizeDecodedIconSize(&iconWidth, &iconHeight);

    wchar_t wPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH) <= 0) {
        return NULL;
    }

    const wchar_t* ext = wcsrchr(wPath, L'.');
    if (ext && _wcsicmp(ext, L".ico") == 0) {
        return DecodeStaticImageWithFactory(NULL, wPath, iconWidth, iconHeight);
    }

    HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* pFactory = NULL;
    HICON hIcon = NULL;

    if (SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_IWICImagingFactory, (void**)&pFactory)) && pFactory) {
        hIcon = DecodeStaticImageWithFactory(pFactory, wPath, iconWidth, iconHeight);
        pFactory->lpVtbl->Release(pFactory);
    }
    
    if (SUCCEEDED(hrInit)) CoUninitialize();
    return hIcon;
}

