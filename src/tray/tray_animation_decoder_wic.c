/**
 * @file tray_animation_decoder_wic.c
 * @brief WIC decoder setup and animation canvas allocation.
 */

#include "tray_animation_decoder_internal.h"

static BOOL ConvertPath(const char* utf8Path, wchar_t path[MAX_PATH]) {
    return utf8Path && MultiByteToWideChar(
        CP_UTF8, 0, utf8Path, -1, path, MAX_PATH) > 0;
}

BOOL TrayDecoder_OpenContext(const char* utf8Path, HANDLE cancelEvent,
                             AnimationDecodeContext* context) {
    if (!context) return FALSE;
    ZeroMemory(context, sizeof(*context));
    context->coInit = E_FAIL;

    wchar_t path[MAX_PATH] = {0};
    if (!ConvertPath(utf8Path, path) ||
        TrayDecoder_IsCancelRequested(cancelEvent)) {
        return FALSE;
    }

    context->coInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT result = CoCreateInstance(
        &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory, (void**)&context->factory);
    if (FAILED(result) || !context->factory) {
        TrayDecoder_CloseContext(context);
        return FALSE;
    }

    result = context->factory->lpVtbl->CreateDecoderFromFilename(
        context->factory, path, NULL, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &context->decoder);
    if (FAILED(result) || !context->decoder) {
        TrayDecoder_CloseContext(context);
        return FALSE;
    }

    GUID format;
    if (SUCCEEDED(context->decoder->lpVtbl->GetContainerFormat(
            context->decoder, &format))) {
        context->isGif = IsEqualGUID(&format, &GUID_ContainerFormatGif);
    }
    return TRUE;
}

void TrayDecoder_CloseContext(AnimationDecodeContext* context) {
    if (!context) return;
    if (context->decoder) {
        context->decoder->lpVtbl->Release(context->decoder);
        context->decoder = NULL;
    }
    if (context->factory) {
        context->factory->lpVtbl->Release(context->factory);
        context->factory = NULL;
    }
    if (SUCCEEDED(context->coInit)) {
        CoUninitialize();
        context->coInit = E_FAIL;
    }
}

static UINT ReadGlobalGifDimension(IWICMetadataQueryReader* metadata,
                                   const wchar_t* query) {
    PROPVARIANT value;
    PropVariantInit(&value);
    UINT dimension = 0;
    if (SUCCEEDED(metadata->lpVtbl->GetMetadataByName(
            metadata, query, &value))) {
        if (value.vt == VT_UI2) dimension = value.uiVal;
        else if (value.vt == VT_I2) dimension = (UINT)value.iVal;
    }
    PropVariantClear(&value);
    return dimension;
}

static void ReadCanvasSize(AnimationDecodeContext* context) {
    if (context->isGif) {
        IWICMetadataQueryReader* metadata = NULL;
        if (SUCCEEDED(context->decoder->lpVtbl->GetMetadataQueryReader(
                context->decoder, &metadata)) && metadata) {
            context->canvasWidth = ReadGlobalGifDimension(
                metadata, L"/logscrdesc/Width");
            context->canvasHeight = ReadGlobalGifDimension(
                metadata, L"/logscrdesc/Height");
            metadata->lpVtbl->Release(metadata);
        }
    }

    if (!context->canvasWidth || !context->canvasHeight) {
        IWICBitmapFrameDecode* firstFrame = NULL;
        if (SUCCEEDED(context->decoder->lpVtbl->GetFrame(
                context->decoder, 0, &firstFrame)) && firstFrame) {
            firstFrame->lpVtbl->GetSize(
                firstFrame, &context->canvasWidth, &context->canvasHeight);
            firstFrame->lpVtbl->Release(firstFrame);
        }
    }
}

static BOOL AllocateAnimationStorage(AnimationDecodeContext* context,
                                     DecodedAnimation* anim,
                                     UINT frameCount) {
    anim->canvasWidth = context->canvasWidth;
    anim->canvasHeight = context->canvasHeight;
    anim->canvas = (BYTE*)calloc(1, context->canvasSize);
    anim->icons = (HICON*)calloc(frameCount, sizeof(*anim->icons));
    anim->delays = (UINT*)calloc(frameCount, sizeof(*anim->delays));
    return anim->canvas && anim->icons && anim->delays;
}

BOOL TrayDecoder_PrepareAnimation(AnimationDecodeContext* context,
                                  DecodedAnimation* anim,
                                  UINT* frameCount) {
    if (!context || !context->decoder || !anim || !frameCount) return FALSE;
    ReadCanvasSize(context);
    if (!TrayDecoder_CheckedBufferSize(
            context->canvasWidth, context->canvasHeight,
            &context->canvasStride, &context->canvasSize)) {
        return FALSE;
    }

    UINT count = 0;
    if (FAILED(context->decoder->lpVtbl->GetFrameCount(
            context->decoder, &count)) || count == 0) {
        return FALSE;
    }
    if (count > TRAY_DECODER_MAX_FRAMES) {
        count = TRAY_DECODER_MAX_FRAMES;
    }
    if (!AllocateAnimationStorage(context, anim, count)) return FALSE;
    *frameCount = count;
    return TRUE;
}
