/**
 * @file tray_animation_decoder.c
 * @brief Image decoding implementation using WIC
 */

#include "tray/tray_animation_decoder.h"
#include <propvarutil.h>
#include <objbase.h>
#include <string.h>
#include <stdlib.h>

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
void BlendPixel(BYTE* canvas, UINT canvasStride, UINT x, UINT y,
                BYTE r, BYTE g, BYTE b, BYTE a) {
    if (a == 0) return;
    
    BYTE* pixel = canvas + (y * canvasStride) + (x * 4);
    
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

/**
 * @brief Clear canvas rectangle (GIF disposal mode 2)
 */
void ClearCanvasRect(BYTE* canvas, UINT canvasWidth, UINT canvasHeight,
                     UINT left, UINT top, UINT width, UINT height,
                     BYTE bgR, BYTE bgG, BYTE bgB, BYTE bgA) {
    UINT canvasStride = canvasWidth * 4;
    UINT right = left + width;
    UINT bottom = top + height;
    
    if (left >= canvasWidth || top >= canvasHeight) return;
    if (right > canvasWidth) right = canvasWidth;
    if (bottom > canvasHeight) bottom = canvasHeight;
    
    for (UINT y = top; y < bottom; y++) {
        for (UINT x = left; x < right; x++) {
            BYTE* pixel = canvas + (y * canvasStride) + (x * 4);
            pixel[0] = bgB;
            pixel[1] = bgG;
            pixel[2] = bgR;
            pixel[3] = bgA;
        }
    }
}

/**
 * @brief Create icon from WIC bitmap source
 */
HICON CreateIconFromWICSource(IWICImagingFactory* pFactory,
                               IWICBitmapSource* source,
                               int cx, int cy) {
    if (!pFactory || !source || cx <= 0 || cy <= 0) return NULL;

    HICON hIcon = NULL;
    IWICBitmapScaler* pScaler = NULL;
    
    HRESULT hr = pFactory->lpVtbl->CreateBitmapScaler(pFactory, &pScaler);
    if (SUCCEEDED(hr) && pScaler) {
        UINT srcW = 0, srcH = 0;
        if (FAILED(source->lpVtbl->GetSize(source, &srcW, &srcH)) || srcW == 0 || srcH == 0) {
            srcW = (UINT)cx;
            srcH = (UINT)cy;
        }

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
            IWICFormatConverter* pConverter = NULL;
            hr = pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter);
            if (SUCCEEDED(hr) && pConverter) {
                hr = pConverter->lpVtbl->Initialize(pConverter,
                                                    (IWICBitmapSource*)pScaler,
                                                    &GUID_WICPixelFormat32bppPBGRA,
                                                    WICBitmapDitherTypeNone,
                                                    NULL, 0.0,
                                                    WICBitmapPaletteTypeCustom);
                if (SUCCEEDED(hr)) {
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
                        ZeroMemory(pvBits, (SIZE_T)(cy * (cx * 4)));

                        UINT scaledStride = dstW * 4;
                        UINT scaledSize = dstH * scaledStride;
                        BYTE* tmp = (BYTE*)malloc(scaledSize);
                        if (tmp) {
                            if (SUCCEEDED(pConverter->lpVtbl->CopyPixels(pConverter, NULL, scaledStride, scaledSize, tmp))) {
                                int xoff = (cx - (int)dstW) / 2;
                                int yoff = (cy - (int)dstH) / 2;
                                if (xoff < 0) xoff = 0;
                                if (yoff < 0) yoff = 0;
                                
                                for (UINT y = 0; y < dstH; ++y) {
                                    BYTE* dstRow = (BYTE*)pvBits + ((yoff + (int)y) * cx + xoff) * 4;
                                    BYTE* srcRow = tmp + y * scaledStride;
                                    memcpy(dstRow, srcRow, scaledStride);
                                }
                            }
                            free(tmp);
                        }

                        ICONINFO ii;
                        ZeroMemory(&ii, sizeof(ii));
                        ii.fIcon = TRUE;
                        ii.hbmColor = hbmColor;
                        ii.hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);
                        
                        if (ii.hbmMask) {
                            HDC hdcMem = GetDC(NULL);
                            if (hdcMem) {
                                HDC hdcColor = CreateCompatibleDC(hdcMem);
                                HDC hdcMask = CreateCompatibleDC(hdcMem);
                                if (hdcColor && hdcMask) {
                                    SelectObject(hdcColor, hbmColor);
                                    SelectObject(hdcMask, ii.hbmMask);

                                    BitBlt(hdcMask, 0, 0, cx, cy, NULL, 0, 0, BLACKNESS);
                                    SetBkColor(hdcColor, RGB(0, 0, 0));
                                    BitBlt(hdcMask, 0, 0, cx, cy, hdcColor, 0, 0, SRCCOPY);
                                    BitBlt(hdcMask, 0, 0, cx, cy, NULL, 0, 0, DSTINVERT);
                                }
                                if (hdcColor) DeleteDC(hdcColor);
                                if (hdcMask) DeleteDC(hdcMask);
                                ReleaseDC(NULL, hdcMem);
                            }
                        }

                        hIcon = CreateIconIndirect(&ii);
                        if (ii.hbmMask) DeleteObject(ii.hbmMask);
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

/**
 * @brief Create icon from PBGRA buffer
 */
HICON CreateIconFromPBGRA(IWICImagingFactory* pFactory,
                          const BYTE* canvasPixels,
                          UINT canvasWidth, UINT canvasHeight,
                          int cx, int cy) {
    if (!pFactory || !canvasPixels || canvasWidth == 0 || canvasHeight == 0 || cx <= 0 || cy <= 0) {
        return NULL;
    }

    HICON hIcon = NULL;
    IWICBitmap* pBitmap = NULL;

    const UINT stride = canvasWidth * 4;
    const UINT size = canvasHeight * stride;

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
    if (!utf8Path || !anim) return FALSE;

    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);

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

    UINT canvasStride = canvasWidth * 4;
    UINT canvasSize = canvasHeight * canvasStride;
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
    pDecoder->lpVtbl->GetFrameCount(pDecoder, &frameCount);
    if (frameCount == 0 || frameCount > 2048) frameCount = 2048;
    
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

    for (UINT i = 0; i < frameCount && anim->count < 2048; ++i) {
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
        IWICFormatConverter* pConverter = NULL;
        if (SUCCEEDED(pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter)) && pConverter) {
            if (SUCCEEDED(pConverter->lpVtbl->Initialize(pConverter, (IWICBitmapSource*)pFrame, 
                                                        &GUID_WICPixelFormat32bppPBGRA, 
                                                        WICBitmapDitherTypeNone, NULL, 0.0, 
                                                        WICBitmapPaletteTypeCustom))) {
                UINT frameStride = frameWidth * 4;
                UINT frameBufferSize = frameHeight * frameStride;
                BYTE* frameBuffer = MemoryPool_Alloc(pool, frameBufferSize);
                
                if (frameBuffer) {
                    if (SUCCEEDED(pConverter->lpVtbl->CopyPixels(pConverter, NULL, frameStride, frameBufferSize, frameBuffer))) {
                        if (isGif) {
                            for (UINT y = 0; y < frameHeight; y++) {
                                for (UINT x = 0; x < frameWidth; x++) {
                                    if (frameLeft + x < canvasWidth && frameTop + y < canvasHeight) {
                                        BYTE* srcPixel = frameBuffer + (y * frameStride) + (x * 4);
                                        BlendPixel(anim->canvas, canvasStride, frameLeft + x, frameTop + y,
                                                 srcPixel[2], srcPixel[1], srcPixel[0], srcPixel[3]);
                                    }
                                }
                            }
                        } else {
                            if (frameWidth == canvasWidth && frameHeight == canvasHeight) {
                                memcpy(anim->canvas, frameBuffer, canvasSize);
                            } else {
                                UINT offsetX = (canvasWidth > frameWidth) ? (canvasWidth - frameWidth) / 2 : 0;
                                UINT offsetY = (canvasHeight > frameHeight) ? (canvasHeight - frameHeight) / 2 : 0;
                                for (UINT y = 0; y < frameHeight && (offsetY + y) < canvasHeight; y++) {
                                    memcpy(anim->canvas + ((offsetY + y) * canvasStride) + (offsetX * 4),
                                         frameBuffer + (y * frameStride), frameStride);
                                }
                            }
                        }
                    }
                    MemoryPool_Free(pool, frameBuffer);
                }
            }
            pConverter->lpVtbl->Release(pConverter);
        }

        /* Create icon from composited canvas */
        HICON hIcon = CreateIconFromPBGRA(pFactory, anim->canvas, canvasWidth, canvasHeight, 
                                         iconWidth, iconHeight);
        if (hIcon) {
            anim->icons[anim->count] = hIcon;
            anim->delays[anim->count] = delayMs;
            anim->count++;
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

    pDecoder->lpVtbl->Release(pDecoder);
    pFactory->lpVtbl->Release(pFactory);
    if (SUCCEEDED(hrInit)) CoUninitialize();

    if (anim->count > 0) {
        anim->isAnimated = TRUE;
    }

    return anim->count > 0;
}

/**
 * @brief Decode static image to single icon
 */
HICON DecodeStaticImage(const char* utf8Path, int iconWidth, int iconHeight) {
    if (!utf8Path) return NULL;

    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);

    wchar_t* ext = wcsrchr(wPath, L'.');
    if (ext && _wcsicmp(ext, L".ico") == 0) {
        return (HICON)LoadImageW(NULL, wPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }

    HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* pFactory = NULL;
    HICON hIcon = NULL;
    
    if (SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, 
                                   &IID_IWICImagingFactory, (void**)&pFactory)) && pFactory) {
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
        pFactory->lpVtbl->Release(pFactory);
    }
    
    if (SUCCEEDED(hrInit)) CoUninitialize();
    return hIcon;
}

