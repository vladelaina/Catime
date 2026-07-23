/**
 * @file tray_animation_decoder_internal.h
 * @brief Shared WIC animation decoder implementation details.
 */

#ifndef CATIME_TRAY_ANIMATION_DECODER_INTERNAL_H
#define CATIME_TRAY_ANIMATION_DECODER_INTERNAL_H

#include "tray/tray_animation_decoder.h"

#include <objbase.h>
#include <propvarutil.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TRAY_DECODER_MAX_PIXELS (4096u * 4096u)
#define TRAY_DECODER_MAX_FRAMES 512u
#define TRAY_DECODER_MIN_DELAY_MS 20u
#define TRAY_DECODER_MAX_DELAY_MS 60000u
#define TRAY_DECODER_ICON_MASK_STACK_BYTES 2048u
#define TRAY_DECODER_ICON_PIXEL_STACK_BYTES 4096u
#define TRAY_DECODER_FALLBACK_ICON_SIZE 16
#define TRAY_DECODER_MAX_ICON_SIZE 256

typedef struct {
    HRESULT coInit;
    IWICImagingFactory* factory;
    IWICBitmapDecoder* decoder;
    BOOL isGif;
    UINT canvasWidth;
    UINT canvasHeight;
    UINT canvasStride;
    UINT canvasSize;
} AnimationDecodeContext;

typedef struct {
    UINT delayMs;
    UINT disposal;
    UINT left;
    UINT top;
    UINT width;
    UINT height;
} AnimationFrameInfo;

BOOL TrayDecoder_IsCancelRequested(HANDLE cancelEvent);
void TrayDecoder_NormalizeIconSize(int* iconWidth, int* iconHeight);
UINT TrayDecoder_ClampFrameDelay(UINT delayMs);
BOOL TrayDecoder_CheckedBufferSize(UINT width, UINT height,
                                   UINT* stride, UINT* size);
HBITMAP TrayDecoder_CreateAlphaMask(const BYTE* pixels, int width, int height);
HBITMAP TrayDecoder_CreateOpaqueMask(int width, int height);
HICON TrayDecoder_CreateExactIcon(const BYTE* pixels, UINT width, UINT height);
void TrayDecoder_BlendPixelInto(BYTE* pixel, BYTE r, BYTE g, BYTE b, BYTE a);

BOOL TrayDecoder_OpenContext(const char* utf8Path, HANDLE cancelEvent,
                             AnimationDecodeContext* context);
void TrayDecoder_CloseContext(AnimationDecodeContext* context);
BOOL TrayDecoder_PrepareAnimation(AnimationDecodeContext* context,
                                  DecodedAnimation* anim, UINT* frameCount);

void TrayDecoder_ReadFrameInfo(IWICBitmapFrameDecode* frame, BOOL isGif,
                               AnimationFrameInfo* info);
BOOL TrayDecoder_DecodeFrameToCanvas(
    const AnimationDecodeContext* context, IWICBitmapFrameDecode* frame,
    const AnimationFrameInfo* info, BYTE* canvas, MemoryPool* pool,
    HANDLE cancelEvent, BOOL* canceled);

#endif /* CATIME_TRAY_ANIMATION_DECODER_INTERNAL_H */
