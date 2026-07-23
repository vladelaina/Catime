/**
 * @file tray_animation_decoder.c
 * @brief Public animation decoding lifecycle and orchestration.
 */

#include "tray_animation_decoder_internal.h"

void DecodedAnimation_Init(DecodedAnimation* anim) {
    if (!anim) return;
    ZeroMemory(anim, sizeof(*anim));
}

void DecodedAnimation_Free(DecodedAnimation* anim) {
    if (!anim) return;

    if (anim->icons) {
        for (int i = 0; i < anim->count; ++i) {
            if (anim->icons[i]) DestroyIcon(anim->icons[i]);
        }
        free(anim->icons);
    }
    free(anim->delays);
    free(anim->canvas);
    ZeroMemory(anim, sizeof(*anim));
}

BOOL DecodeAnimatedImage(const char* utf8Path, DecodedAnimation* anim,
                         MemoryPool* pool, int iconWidth, int iconHeight) {
    return DecodeAnimatedImageWithCancel(utf8Path, anim, pool,
                                         iconWidth, iconHeight, NULL);
}

static void ApplyPreviousDisposal(const AnimationDecodeContext* context,
                                  DecodedAnimation* anim, UINT frameIndex,
                                  const AnimationFrameInfo* previous) {
    if (!context || !anim || !previous) return;
    if (context->isGif && frameIndex > 0 && previous->disposal == 2) {
        ClearCanvasRect(anim->canvas, context->canvasWidth,
                        context->canvasHeight, previous->left, previous->top,
                        previous->width, previous->height, 0, 0, 0, 0);
    } else if (!context->isGif) {
        memset(anim->canvas, 0, context->canvasSize);
    }
}

static BOOL StoreDecodedFrame(const AnimationDecodeContext* context,
                              DecodedAnimation* anim,
                              const AnimationFrameInfo* frame,
                              int iconWidth, int iconHeight,
                              HANDLE cancelEvent) {
    if (TrayDecoder_IsCancelRequested(cancelEvent)) return FALSE;

    HICON icon = CreateIconFromPBGRA(context->factory, anim->canvas,
                                    context->canvasWidth,
                                    context->canvasHeight,
                                    iconWidth, iconHeight);
    if (!icon) return FALSE;

    anim->icons[anim->count] = icon;
    anim->delays[anim->count] = TrayDecoder_ClampFrameDelay(frame->delayMs);
    ++anim->count;
    return TRUE;
}

static BOOL DecodeFrames(AnimationDecodeContext* context,
                         DecodedAnimation* anim, MemoryPool* pool,
                         UINT frameCount, int iconWidth, int iconHeight,
                         HANDLE cancelEvent, BOOL* canceled) {
    AnimationFrameInfo previous;
    ZeroMemory(&previous, sizeof(previous));

    for (UINT i = 0; i < frameCount; ++i) {
        if (TrayDecoder_IsCancelRequested(cancelEvent)) {
            *canceled = TRUE;
            break;
        }

        IWICBitmapFrameDecode* frameSource = NULL;
        if (FAILED(context->decoder->lpVtbl->GetFrame(
                context->decoder, i, &frameSource)) || !frameSource) {
            continue;
        }

        ApplyPreviousDisposal(context, anim, i, &previous);

        AnimationFrameInfo frame;
        TrayDecoder_ReadFrameInfo(frameSource, context->isGif, &frame);
        BOOL frameDecoded = TrayDecoder_DecodeFrameToCanvas(
            context, frameSource, &frame, anim->canvas, pool,
            cancelEvent, canceled);

        if (frameDecoded) {
            StoreDecodedFrame(context, anim, &frame, iconWidth, iconHeight,
                              cancelEvent);
            if (context->isGif) previous = frame;
        }

        frameSource->lpVtbl->Release(frameSource);
        if (*canceled) break;
    }
    return anim->count > 0;
}

BOOL DecodeAnimatedImageWithCancel(const char* utf8Path, DecodedAnimation* anim,
                                   MemoryPool* pool, int iconWidth,
                                   int iconHeight, HANDLE cancelEvent) {
    if (!utf8Path || !anim) return FALSE;
    TrayDecoder_NormalizeIconSize(&iconWidth, &iconHeight);
    if (TrayDecoder_IsCancelRequested(cancelEvent)) return FALSE;

    AnimationDecodeContext context;
    if (!TrayDecoder_OpenContext(utf8Path, cancelEvent, &context)) {
        return FALSE;
    }

    UINT frameCount = 0;
    BOOL prepared = TrayDecoder_PrepareAnimation(&context, anim, &frameCount);
    BOOL canceled = FALSE;
    BOOL decoded = prepared && DecodeFrames(
        &context, anim, pool, frameCount, iconWidth, iconHeight,
        cancelEvent, &canceled);

    TrayDecoder_CloseContext(&context);
    if (TrayDecoder_IsCancelRequested(cancelEvent)) canceled = TRUE;

    if (decoded && !canceled) {
        anim->isAnimated = TRUE;
        return TRUE;
    }

    DecodedAnimation_Free(anim);
    return FALSE;
}
