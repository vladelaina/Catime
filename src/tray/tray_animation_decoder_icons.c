/**
 * @file tray_animation_decoder_icons.c
 * @brief WIC scaling and conversion to tray icons.
 */

#include "tray_animation_decoder_internal.h"

static BOOL CopyCenteredPixels(IWICFormatConverter* converter,
                               BYTE* destination, UINT destinationStride,
                               int destinationWidth, int destinationHeight,
                               UINT sourceWidth, UINT sourceHeight) {
    UINT sourceStride = 0;
    UINT sourceSize = 0;
    if (!TrayDecoder_CheckedBufferSize(sourceWidth, sourceHeight,
                                       &sourceStride, &sourceSize)) {
        return FALSE;
    }

    BYTE stackPixels[TRAY_DECODER_ICON_PIXEL_STACK_BYTES];
    BYTE* source = sourceSize <= sizeof(stackPixels)
        ? stackPixels : (BYTE*)malloc(sourceSize);
    if (!source) return FALSE;

    BOOL copied = FALSE;
    if (SUCCEEDED(converter->lpVtbl->CopyPixels(
            converter, NULL, sourceStride, sourceSize, source))) {
        int offsetX = (destinationWidth - (int)sourceWidth) / 2;
        int offsetY = (destinationHeight - (int)sourceHeight) / 2;
        if (offsetX < 0) offsetX = 0;
        if (offsetY < 0) offsetY = 0;

        UINT copyWidth = sourceWidth;
        UINT copyHeight = sourceHeight;
        if ((UINT)offsetX < (UINT)destinationWidth &&
            (UINT)offsetY < (UINT)destinationHeight) {
            if (copyWidth > (UINT)destinationWidth - (UINT)offsetX) {
                copyWidth = (UINT)destinationWidth - (UINT)offsetX;
            }
            if (copyHeight > (UINT)destinationHeight - (UINT)offsetY) {
                copyHeight = (UINT)destinationHeight - (UINT)offsetY;
            }
        } else {
            copyWidth = 0;
            copyHeight = 0;
        }

        UINT copyBytes = copyWidth * 4u;
        for (UINT y = 0; copyBytes && y < copyHeight; ++y) {
            memcpy(destination + (SIZE_T)(offsetY + (int)y) *
                       destinationStride + (SIZE_T)offsetX * 4u,
                   source + (SIZE_T)y * sourceStride, copyBytes);
        }
        copied = TRUE;
    }

    if (source != stackPixels) free(source);
    return copied;
}

static HICON CreateIconFromScaledConverter(IWICFormatConverter* converter,
                                           UINT scaledWidth,
                                           UINT scaledHeight,
                                           int width, int height) {
    UINT stride = 0;
    UINT size = 0;
    if (!TrayDecoder_CheckedBufferSize((UINT)width, (UINT)height,
                                       &stride, &size)) {
        return NULL;
    }

    BITMAPINFO bitmapInfo;
    ZeroMemory(&bitmapInfo, sizeof(bitmapInfo));
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* pixels = NULL;
    HBITMAP color = CreateDIBSection(NULL, &bitmapInfo, DIB_RGB_COLORS,
                                     &pixels, NULL, 0);
    if (!color || !pixels) {
        if (color) DeleteObject(color);
        return NULL;
    }
    ZeroMemory(pixels, size);

    HICON icon = NULL;
    if (CopyCenteredPixels(converter, (BYTE*)pixels, stride, width, height,
                           scaledWidth, scaledHeight)) {
        ICONINFO iconInfo;
        ZeroMemory(&iconInfo, sizeof(iconInfo));
        iconInfo.fIcon = TRUE;
        iconInfo.hbmColor = color;
        iconInfo.hbmMask = TrayDecoder_CreateAlphaMask(
            (const BYTE*)pixels, width, height);
        if (!iconInfo.hbmMask) {
            iconInfo.hbmMask = TrayDecoder_CreateOpaqueMask(width, height);
        }
        icon = CreateIconIndirect(&iconInfo);
        if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
    }
    DeleteObject(color);
    return icon;
}

HICON CreateIconFromWICSource(IWICImagingFactory* pFactory,
                              IWICBitmapSource* source,
                              int cx, int cy) {
    if (!pFactory || !source) return NULL;
    TrayDecoder_NormalizeIconSize(&cx, &cy);

    UINT sourceWidth = 0;
    UINT sourceHeight = 0;
    if (FAILED(source->lpVtbl->GetSize(
            source, &sourceWidth, &sourceHeight)) ||
        sourceWidth == 0 || sourceHeight == 0) {
        sourceWidth = (UINT)cx;
        sourceHeight = (UINT)cy;
    } else {
        UINT checkedStride = 0;
        UINT checkedSize = 0;
        if (!TrayDecoder_CheckedBufferSize(sourceWidth, sourceHeight,
                                           &checkedStride, &checkedSize)) {
            return NULL;
        }
    }

    double scaleX = (double)cx / sourceWidth;
    double scaleY = (double)cy / sourceHeight;
    double scale = scaleX < scaleY ? scaleX : scaleY;
    if (scale <= 0.0) scale = 1.0;
    UINT scaledWidth = (UINT)((double)sourceWidth * scale + 0.5);
    UINT scaledHeight = (UINT)((double)sourceHeight * scale + 0.5);
    if (!scaledWidth) scaledWidth = 1;
    if (!scaledHeight) scaledHeight = 1;

    IWICBitmapScaler* scaler = NULL;
    IWICFormatConverter* converter = NULL;
    HICON icon = NULL;
    HRESULT result = pFactory->lpVtbl->CreateBitmapScaler(pFactory, &scaler);
    if (SUCCEEDED(result) && scaler) {
        result = scaler->lpVtbl->Initialize(
            scaler, source, scaledWidth, scaledHeight,
            WICBitmapInterpolationModeFant);
    }
    if (SUCCEEDED(result)) {
        result = pFactory->lpVtbl->CreateFormatConverter(pFactory, &converter);
    }
    if (SUCCEEDED(result) && converter) {
        result = converter->lpVtbl->Initialize(
            converter, (IWICBitmapSource*)scaler,
            &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
            NULL, 0.0, WICBitmapPaletteTypeCustom);
    }
    if (SUCCEEDED(result)) {
        icon = CreateIconFromScaledConverter(converter, scaledWidth,
                                             scaledHeight, cx, cy);
    }

    if (converter) converter->lpVtbl->Release(converter);
    if (scaler) scaler->lpVtbl->Release(scaler);
    return icon;
}

HICON CreateIconFromPBGRA(IWICImagingFactory* pFactory,
                          const BYTE* canvasPixels,
                          UINT canvasWidth, UINT canvasHeight,
                          int cx, int cy) {
    if (!pFactory || !canvasPixels || !canvasWidth || !canvasHeight) {
        return NULL;
    }
    TrayDecoder_NormalizeIconSize(&cx, &cy);
    if (canvasWidth == (UINT)cx && canvasHeight == (UINT)cy) {
        return TrayDecoder_CreateExactIcon(canvasPixels,
                                           canvasWidth, canvasHeight);
    }

    UINT stride = 0;
    UINT size = 0;
    if (!TrayDecoder_CheckedBufferSize(canvasWidth, canvasHeight,
                                       &stride, &size)) {
        return NULL;
    }

    IWICBitmap* bitmap = NULL;
    HRESULT result = pFactory->lpVtbl->CreateBitmapFromMemory(
        pFactory, canvasWidth, canvasHeight, &GUID_WICPixelFormat32bppPBGRA,
        stride, size, (BYTE*)canvasPixels, &bitmap);
    if (FAILED(result) || !bitmap) return NULL;

    HICON icon = CreateIconFromWICSource(
        pFactory, (IWICBitmapSource*)bitmap, cx, cy);
    bitmap->lpVtbl->Release(bitmap);
    return icon;
}

HICON DecodeStaticImageWithFactory(IWICImagingFactory* pFactory,
                                   const wchar_t* wPath,
                                   int iconWidth, int iconHeight) {
    if (!wPath) return NULL;
    TrayDecoder_NormalizeIconSize(&iconWidth, &iconHeight);

    const wchar_t* extension = wcsrchr(wPath, L'.');
    if (extension && _wcsicmp(extension, L".ico") == 0) {
        return (HICON)LoadImageW(NULL, wPath, IMAGE_ICON, iconWidth, iconHeight,
                                 LR_LOADFROMFILE);
    }
    if (!pFactory) return NULL;

    IWICBitmapDecoder* decoder = NULL;
    HRESULT result = pFactory->lpVtbl->CreateDecoderFromFilename(
        pFactory, wPath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(result) || !decoder) return NULL;

    HICON icon = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    if (SUCCEEDED(decoder->lpVtbl->GetFrame(decoder, 0, &frame)) && frame) {
        icon = CreateIconFromWICSource(pFactory, (IWICBitmapSource*)frame,
                                       iconWidth, iconHeight);
        frame->lpVtbl->Release(frame);
    }
    decoder->lpVtbl->Release(decoder);
    return icon;
}

HICON DecodeStaticImage(const char* utf8Path, int iconWidth, int iconHeight) {
    if (!utf8Path) return NULL;
    TrayDecoder_NormalizeIconSize(&iconWidth, &iconHeight);

    wchar_t path[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1,
                            path, MAX_PATH) <= 0) {
        return NULL;
    }

    const wchar_t* extension = wcsrchr(path, L'.');
    if (extension && _wcsicmp(extension, L".ico") == 0) {
        return DecodeStaticImageWithFactory(NULL, path, iconWidth, iconHeight);
    }

    HRESULT coInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* factory = NULL;
    HICON icon = NULL;
    if (SUCCEEDED(CoCreateInstance(
            &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory, (void**)&factory)) && factory) {
        icon = DecodeStaticImageWithFactory(
            factory, path, iconWidth, iconHeight);
        factory->lpVtbl->Release(factory);
    }
    if (SUCCEEDED(coInit)) CoUninitialize();
    return icon;
}
