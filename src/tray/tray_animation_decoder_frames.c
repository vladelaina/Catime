/**
 * @file tray_animation_decoder_frames.c
 * @brief Frame metadata reading and compositing into the animation canvas.
 */

#include "tray_animation_decoder_internal.h"

static UINT ReadMetadataUnsigned(IWICMetadataQueryReader* metadata,
                                 const wchar_t* query, VARTYPE expectedType) {
    PROPVARIANT value;
    PropVariantInit(&value);
    UINT result = 0;
    if (SUCCEEDED(metadata->lpVtbl->GetMetadataByName(
            metadata, query, &value))) {
        if (expectedType == VT_UI1 && value.vt == VT_UI1) {
            result = value.bVal;
        } else if (expectedType == VT_UI2 && value.vt == VT_UI2) {
            result = value.uiVal;
        } else if (expectedType == VT_UI4 && value.vt == VT_UI4) {
            result = value.ulVal;
        }
    }
    PropVariantClear(&value);
    return result;
}

static UINT ReadGifDelay(IWICMetadataQueryReader* metadata) {
    PROPVARIANT value;
    PropVariantInit(&value);
    UINT delayMs = 100;
    if (SUCCEEDED(metadata->lpVtbl->GetMetadataByName(
            metadata, L"/grctlext/Delay", &value)) &&
        (value.vt == VT_UI2 || value.vt == VT_I2)) {
        USHORT centiseconds = value.vt == VT_UI2
            ? value.uiVal : (USHORT)value.iVal;
        if (centiseconds == 0) centiseconds = 10;
        delayMs = (UINT)centiseconds * 10u;
    }
    PropVariantClear(&value);
    return delayMs;
}

static void ReadWebpDelay(IWICMetadataQueryReader* metadata,
                          AnimationFrameInfo* info) {
    PROPVARIANT value;
    PropVariantInit(&value);
    if (SUCCEEDED(metadata->lpVtbl->GetMetadataByName(
            metadata, L"/webp/delay", &value)) && value.vt == VT_UI4) {
        info->delayMs = value.ulVal;
    }
    PropVariantClear(&value);
}

void TrayDecoder_ReadFrameInfo(IWICBitmapFrameDecode* frame, BOOL isGif,
                               AnimationFrameInfo* info) {
    if (!info) return;
    ZeroMemory(info, sizeof(*info));
    info->delayMs = 100;
    if (!frame) return;

    IWICMetadataQueryReader* metadata = NULL;
    if (SUCCEEDED(frame->lpVtbl->GetMetadataQueryReader(
            frame, &metadata)) && metadata) {
        if (isGif) {
            info->delayMs = ReadGifDelay(metadata);
            info->disposal = ReadMetadataUnsigned(
                metadata, L"/grctlext/Disposal", VT_UI1);
            info->left = ReadMetadataUnsigned(
                metadata, L"/imgdesc/Left", VT_UI2);
            info->top = ReadMetadataUnsigned(
                metadata, L"/imgdesc/Top", VT_UI2);
            info->width = ReadMetadataUnsigned(
                metadata, L"/imgdesc/Width", VT_UI2);
            info->height = ReadMetadataUnsigned(
                metadata, L"/imgdesc/Height", VT_UI2);
        } else {
            ReadWebpDelay(metadata, info);
        }
        metadata->lpVtbl->Release(metadata);
    }
    frame->lpVtbl->GetSize(frame, &info->width, &info->height);
}

static BOOL BlendGifFrame(const AnimationDecodeContext* context,
                          const AnimationFrameInfo* info, BYTE* canvas,
                          const BYTE* framePixels, UINT frameStride,
                          HANDLE cancelEvent, BOOL* canceled) {
    if (info->left >= context->canvasWidth ||
        info->top >= context->canvasHeight) {
        return TRUE;
    }

    UINT copyWidth = info->width;
    UINT copyHeight = info->height;
    if (copyWidth > context->canvasWidth - info->left) {
        copyWidth = context->canvasWidth - info->left;
    }
    if (copyHeight > context->canvasHeight - info->top) {
        copyHeight = context->canvasHeight - info->top;
    }

    for (UINT y = 0; y < copyHeight; ++y) {
        if (TrayDecoder_IsCancelRequested(cancelEvent)) {
            *canceled = TRUE;
            return FALSE;
        }
        BYTE* destination = canvas +
            (SIZE_T)(info->top + y) * context->canvasStride +
            (SIZE_T)info->left * 4u;
        const BYTE* source = framePixels + (SIZE_T)y * frameStride;
        for (UINT x = 0; x < copyWidth; ++x) {
            TrayDecoder_BlendPixelInto(destination, source[2], source[1],
                                       source[0], source[3]);
            destination += 4;
            source += 4;
        }
    }
    return TRUE;
}

static BOOL CopyCenteredFrame(const AnimationDecodeContext* context,
                              const AnimationFrameInfo* info, BYTE* canvas,
                              const BYTE* framePixels, UINT frameStride,
                              HANDLE cancelEvent, BOOL* canceled) {
    if (info->width == context->canvasWidth &&
        info->height == context->canvasHeight) {
        memcpy(canvas, framePixels, context->canvasSize);
        return TRUE;
    }

    UINT offsetX = context->canvasWidth > info->width
        ? (context->canvasWidth - info->width) / 2u : 0;
    UINT offsetY = context->canvasHeight > info->height
        ? (context->canvasHeight - info->height) / 2u : 0;
    UINT copyWidth = offsetX < context->canvasWidth
        ? context->canvasWidth - offsetX : 0;
    if (copyWidth > info->width) copyWidth = info->width;
    UINT copyBytes = copyWidth * 4u;

    for (UINT y = 0; copyBytes && y < info->height &&
         offsetY + y < context->canvasHeight; ++y) {
        if (TrayDecoder_IsCancelRequested(cancelEvent)) {
            *canceled = TRUE;
            return FALSE;
        }
        memcpy(canvas + (SIZE_T)(offsetY + y) * context->canvasStride +
                   (SIZE_T)offsetX * 4u,
               framePixels + (SIZE_T)y * frameStride, copyBytes);
    }
    return TRUE;
}

static BOOL ConvertFramePixels(const AnimationDecodeContext* context,
                               const AnimationFrameInfo* info, BYTE* canvas,
                               IWICFormatConverter* converter,
                               MemoryPool* pool, HANDLE cancelEvent,
                               BOOL* canceled) {
    UINT frameStride = 0;
    UINT frameSize = 0;
    if (!TrayDecoder_CheckedBufferSize(info->width, info->height,
                                       &frameStride, &frameSize)) {
        return FALSE;
    }

    BYTE* framePixels = (BYTE*)MemoryPool_Alloc(pool, frameSize);
    if (!framePixels) return FALSE;
    BOOL decoded = FALSE;
    if (TrayDecoder_IsCancelRequested(cancelEvent)) {
        *canceled = TRUE;
    } else if (SUCCEEDED(converter->lpVtbl->CopyPixels(
                   converter, NULL, frameStride, frameSize, framePixels))) {
        decoded = context->isGif
            ? BlendGifFrame(context, info, canvas, framePixels, frameStride,
                            cancelEvent, canceled)
            : CopyCenteredFrame(context, info, canvas, framePixels,
                                frameStride, cancelEvent, canceled);
    }
    MemoryPool_Free(pool, framePixels);
    return decoded && !*canceled;
}

BOOL TrayDecoder_DecodeFrameToCanvas(
    const AnimationDecodeContext* context, IWICBitmapFrameDecode* frame,
    const AnimationFrameInfo* info, BYTE* canvas, MemoryPool* pool,
    HANDLE cancelEvent, BOOL* canceled) {
    if (!context || !frame || !info || !canvas || !canceled) return FALSE;

    IWICFormatConverter* converter = NULL;
    HRESULT result = context->factory->lpVtbl->CreateFormatConverter(
        context->factory, &converter);
    if (FAILED(result) || !converter) return FALSE;

    result = converter->lpVtbl->Initialize(
        converter, (IWICBitmapSource*)frame,
        &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
        NULL, 0.0, WICBitmapPaletteTypeCustom);
    BOOL decoded = FALSE;
    if (SUCCEEDED(result)) {
        decoded = ConvertFramePixels(context, info, canvas,
                                     converter, pool, cancelEvent, canceled);
    }
    converter->lpVtbl->Release(converter);
    return decoded;
}
