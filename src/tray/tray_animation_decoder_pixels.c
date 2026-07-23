/**
 * @file tray_animation_decoder_pixels.c
 * @brief Checked pixel buffers, icon masks, and canvas compositing.
 */

#include "tray_animation_decoder_internal.h"

BOOL TrayDecoder_IsCancelRequested(HANDLE cancelEvent) {
    return cancelEvent &&
           WaitForSingleObject(cancelEvent, 0) == WAIT_OBJECT_0;
}

static int ClampIconDimension(int value) {
    if (value <= 0) return TRAY_DECODER_FALLBACK_ICON_SIZE;
    if (value > TRAY_DECODER_MAX_ICON_SIZE) {
        return TRAY_DECODER_MAX_ICON_SIZE;
    }
    return value;
}

void TrayDecoder_NormalizeIconSize(int* iconWidth, int* iconHeight) {
    if (iconWidth) *iconWidth = ClampIconDimension(*iconWidth);
    if (iconHeight) *iconHeight = ClampIconDimension(*iconHeight);
}

UINT TrayDecoder_ClampFrameDelay(UINT delayMs) {
    if (delayMs < TRAY_DECODER_MIN_DELAY_MS) {
        return TRAY_DECODER_MIN_DELAY_MS;
    }
    if (delayMs > TRAY_DECODER_MAX_DELAY_MS) {
        return TRAY_DECODER_MAX_DELAY_MS;
    }
    return delayMs;
}

BOOL TrayDecoder_CheckedBufferSize(UINT width, UINT height,
                                   UINT* stride, UINT* size) {
    if (!stride || !size || width == 0 || height == 0) return FALSE;
    if (width > TRAY_DECODER_MAX_PIXELS / height) return FALSE;
    if (width > UINT32_MAX / 4u) return FALSE;

    UINT checkedStride = width * 4u;
    if (height > UINT32_MAX / checkedStride) return FALSE;
    *stride = checkedStride;
    *size = height * checkedStride;
    return TRUE;
}

static HBITMAP CreateIconMask(const BYTE* pixels, int width, int height,
                              BOOL useAlpha) {
    if (width <= 0 || height <= 0 || (useAlpha && !pixels)) return NULL;

    SIZE_T stride = (SIZE_T)(((width + 15) / 16) * 2);
    SIZE_T size = stride * (SIZE_T)height;
    BYTE stackBits[TRAY_DECODER_ICON_MASK_STACK_BYTES];
    BYTE* bits = size <= sizeof(stackBits) ? stackBits : (BYTE*)malloc(size);
    if (!bits) return NULL;
    memset(bits, 0, size);

    if (useAlpha) {
        for (int y = 0; y < height; ++y) {
            const BYTE* row = pixels + (SIZE_T)y * (SIZE_T)width * 4u;
            for (int x = 0; x < width; ++x) {
                if (row[(SIZE_T)x * 4u + 3u] == 0) {
                    bits[(SIZE_T)y * stride + (SIZE_T)x / 8u] |=
                        (BYTE)(0x80u >> (x & 7));
                }
            }
        }
    }

    HBITMAP mask = CreateBitmap(width, height, 1, 1, bits);
    if (bits != stackBits) free(bits);
    return mask;
}

HBITMAP TrayDecoder_CreateAlphaMask(const BYTE* pixels,
                                    int width, int height) {
    return CreateIconMask(pixels, width, height, TRUE);
}

HBITMAP TrayDecoder_CreateOpaqueMask(int width, int height) {
    return CreateIconMask(NULL, width, height, FALSE);
}

HICON TrayDecoder_CreateExactIcon(const BYTE* pixels,
                                  UINT width, UINT height) {
    UINT stride = 0;
    UINT size = 0;
    if (!pixels ||
        !TrayDecoder_CheckedBufferSize(width, height, &stride, &size)) {
        return NULL;
    }

    BITMAPINFO bitmapInfo;
    ZeroMemory(&bitmapInfo, sizeof(bitmapInfo));
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = (LONG)width;
    bitmapInfo.bmiHeader.biHeight = -(LONG)height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* dibPixels = NULL;
    HBITMAP color = CreateDIBSection(NULL, &bitmapInfo, DIB_RGB_COLORS,
                                     &dibPixels, NULL, 0);
    if (!color || !dibPixels) {
        if (color) DeleteObject(color);
        return NULL;
    }
    memcpy(dibPixels, pixels, size);

    ICONINFO iconInfo;
    ZeroMemory(&iconInfo, sizeof(iconInfo));
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = color;
    iconInfo.hbmMask = TrayDecoder_CreateAlphaMask(
        (const BYTE*)dibPixels, (int)width, (int)height);
    if (!iconInfo.hbmMask) {
        iconInfo.hbmMask = TrayDecoder_CreateOpaqueMask((int)width,
                                                        (int)height);
    }

    HICON icon = CreateIconIndirect(&iconInfo);
    if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
    DeleteObject(color);
    return icon;
}

void TrayDecoder_BlendPixelInto(BYTE* pixel,
                                BYTE r, BYTE g, BYTE b, BYTE a) {
    if (!pixel || a == 0) return;
    if (a == 255 || pixel[3] == 0) {
        pixel[0] = b;
        pixel[1] = g;
        pixel[2] = r;
        pixel[3] = a;
        return;
    }

    UINT sourceAlpha = a;
    UINT destinationAlpha = pixel[3];
    UINT inverseSourceAlpha = 255u - sourceAlpha;
    UINT resultAlpha = sourceAlpha +
        (destinationAlpha * inverseSourceAlpha) / 255u;
    if (resultAlpha == 0) return;

    pixel[0] = (BYTE)((b * sourceAlpha +
        pixel[0] * destinationAlpha * inverseSourceAlpha / 255u) /
        resultAlpha);
    pixel[1] = (BYTE)((g * sourceAlpha +
        pixel[1] * destinationAlpha * inverseSourceAlpha / 255u) /
        resultAlpha);
    pixel[2] = (BYTE)((r * sourceAlpha +
        pixel[2] * destinationAlpha * inverseSourceAlpha / 255u) /
        resultAlpha);
    pixel[3] = (BYTE)resultAlpha;
}

void BlendPixel(BYTE* canvas, UINT canvasStride, UINT x, UINT y,
                BYTE r, BYTE g, BYTE b, BYTE a) {
    if (!canvas) return;
    TrayDecoder_BlendPixelInto(
        canvas + (SIZE_T)y * canvasStride + (SIZE_T)x * 4u,
        r, g, b, a);
}

void ClearCanvasRect(BYTE* canvas, UINT canvasWidth, UINT canvasHeight,
                     UINT left, UINT top, UINT width, UINT height,
                     BYTE bgR, BYTE bgG, BYTE bgB, BYTE bgA) {
    if (!canvas || !canvasWidth || !canvasHeight || !width || !height ||
        left >= canvasWidth || top >= canvasHeight) return;

    UINT clearWidth = width > canvasWidth - left ? canvasWidth - left : width;
    UINT clearHeight = height > canvasHeight - top ? canvasHeight - top : height;
    SIZE_T canvasStride = (SIZE_T)canvasWidth * 4u;
    BYTE* firstRow = canvas + (SIZE_T)top * canvasStride + (SIZE_T)left * 4u;

    if (bgR == 0 && bgG == 0 && bgB == 0 && bgA == 0) {
        SIZE_T clearBytes = (SIZE_T)clearWidth * 4u;
        for (UINT y = 0; y < clearHeight; ++y) {
            memset(firstRow + (SIZE_T)y * canvasStride, 0, clearBytes);
        }
        return;
    }

    DWORD value = (DWORD)bgB | ((DWORD)bgG << 8) |
                  ((DWORD)bgR << 16) | ((DWORD)bgA << 24);
    for (UINT y = 0; y < clearHeight; ++y) {
        DWORD* row = (DWORD*)(firstRow + (SIZE_T)y * canvasStride);
        for (UINT x = 0; x < clearWidth; ++x) row[x] = value;
    }
}
