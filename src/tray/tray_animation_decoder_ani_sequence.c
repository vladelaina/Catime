#include "tray_animation_decoder_ani_internal.h"
#include <stdlib.h>
#include <string.h>

BOOL DecodeAniCursorWithCancel(const char* utf8Path, DecodedAnimation* anim,
                               int iconWidth, int iconHeight,
                               HANDLE cancelEvent) {
    if (!utf8Path || !anim || IsAniDecodeCancelRequested(cancelEvent)) return FALSE;
    NormalizeAniIconSize(&iconWidth, &iconHeight);
    wchar_t wPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH) <= 0)
        return FALSE;
    DWORD fileSize = 0;
    BYTE* bytes = ReadAniFileBytes(wPath, &fileSize);
    if (!bytes) return FALSE;
    AniMetadata meta;
    AniFrameBlob frames[ANI_MAX_ANIMATION_FRAMES];
    UINT frameCount = 0;
    ZeroMemory(frames, sizeof(frames));
    BOOL parsed = ParseAniRiff(bytes, fileSize, &meta, frames, &frameCount);
    if (!parsed || frameCount == 0 || IsAniDecodeCancelRequested(cancelEvent)) {
        free(bytes);
        return FALSE;
    }
    HICON frameIcons[ANI_MAX_ANIMATION_FRAMES];
    ZeroMemory(frameIcons, sizeof(frameIcons));
    UINT decodedFrameCount = 0;
    for (UINT i = 0; i < frameCount; ++i) {
        if (IsAniDecodeCancelRequested(cancelEvent)) break;
        frameIcons[i] = CreateIconFromAniFrameBlob(frames[i].data,
                                                    frames[i].size,
                                                    iconWidth, iconHeight);
        if (frameIcons[i]) decodedFrameCount++;
    }
    if (decodedFrameCount == 0 || IsAniDecodeCancelRequested(cancelEvent)) {
        for (UINT i = 0; i < frameCount; ++i)
            if (frameIcons[i]) DestroyIcon(frameIcons[i]);
        free(bytes);
        return FALSE;
    }
    UINT stepCount = meta.cSteps ? meta.cSteps :
                     (meta.sequenceCount ? meta.sequenceCount : frameCount);
    if (stepCount == 0) stepCount = frameCount;
    if (stepCount > ANI_MAX_ANIMATION_FRAMES) stepCount = ANI_MAX_ANIMATION_FRAMES;
    anim->icons = (HICON*)calloc(stepCount, sizeof(HICON));
    anim->delays = (UINT*)calloc(stepCount, sizeof(UINT));
    if (!anim->icons || !anim->delays) {
        DecodedAnimation_Free(anim);
        for (UINT i = 0; i < frameCount; ++i)
            if (frameIcons[i]) DestroyIcon(frameIcons[i]);
        free(bytes);
        return FALSE;
    }
    DWORD fallbackJiffies = meta.jifRate ? meta.jifRate : ANI_DEFAULT_JIFFIES;
    BOOL canceled = FALSE;
    for (UINT step = 0; step < stepCount; ++step) {
        if (IsAniDecodeCancelRequested(cancelEvent)) {
            canceled = TRUE;
            break;
        }
        UINT frameIndex = step % frameCount;
        if (meta.sequence && step < meta.sequenceCount) {
            frameIndex = ReadLe32(meta.sequence + (size_t)step * 4u);
            if (frameIndex >= frameCount) frameIndex %= frameCount;
        }
        HICON sourceIcon = frameIcons[frameIndex];
        if (!sourceIcon) {
            for (UINT probe = 0; probe < frameCount; ++probe) {
                if (frameIcons[probe]) {
                    sourceIcon = frameIcons[probe];
                    break;
                }
            }
        }
        if (!sourceIcon) continue;
        HICON copiedIcon = CopyIcon(sourceIcon);
        if (!copiedIcon) continue;
        DWORD rateJiffies = (meta.rates && step < meta.rateCount)
            ? ReadLe32(meta.rates + (size_t)step * 4u) : 0;
        anim->icons[anim->count] = copiedIcon;
        anim->delays[anim->count] = AniJiffiesToMilliseconds(rateJiffies,
                                                              fallbackJiffies);
        anim->count++;
    }
    for (UINT i = 0; i < frameCount; ++i)
        if (frameIcons[i]) DestroyIcon(frameIcons[i]);
    free(bytes);
    if (canceled || anim->count == 0) {
        DecodedAnimation_Free(anim);
        return FALSE;
    }
    anim->isAnimated = (anim->count > 1);
    return TRUE;
}

BOOL DecodeAniCursor(const char* utf8Path, DecodedAnimation* anim,
                     int iconWidth, int iconHeight) {
    return DecodeAniCursorWithCancel(utf8Path, anim, iconWidth, iconHeight, NULL);
}
