#ifndef TRAY_ANIMATION_DECODER_ANI_INTERNAL_H
#define TRAY_ANIMATION_DECODER_ANI_INTERNAL_H

#include "tray/tray_animation_decoder.h"

#define ANI_MAX_FILE_BYTES (128ull * 1024ull * 1024ull)
#define ANI_MAX_ANIMATION_FRAMES 512u
#define ANI_MIN_FRAME_DELAY_MS 20u
#define ANI_MAX_FRAME_DELAY_MS 60000u
#define ANI_ICON_FALLBACK_SIZE 16
#define ANI_ICON_MAX_SIZE 256
#define ANI_DEFAULT_JIFFIES 6u
#define ANI_HEADER_MIN_SIZE 36u
#define ANI_MAX_LIST_DEPTH 16u
#define ANI_FOURCC(a, b, c, d) \
    ((DWORD)(BYTE)(a) | ((DWORD)(BYTE)(b) << 8) | \
     ((DWORD)(BYTE)(c) << 16) | ((DWORD)(BYTE)(d) << 24))

typedef struct {
    const BYTE* data;
    DWORD size;
} AniFrameBlob;
typedef struct {
    DWORD cSteps;
    DWORD jifRate;
    const BYTE* rates;
    UINT rateCount;
    const BYTE* sequence;
    UINT sequenceCount;
} AniMetadata;

BOOL IsAniDecodeCancelRequested(HANDLE cancelEvent);
void NormalizeAniIconSize(int* iconWidth, int* iconHeight);
DWORD ReadLe32(const BYTE* p);
UINT AniJiffiesToMilliseconds(DWORD jiffies, DWORD fallbackJiffies);
BYTE* ReadAniFileBytes(const wchar_t* path, DWORD* outSize);
BOOL ParseAniRiff(const BYTE* bytes, DWORD size, AniMetadata* meta,
                  AniFrameBlob* frames, UINT* frameCount);
HICON CreateIconFromAniFrameBlob(const BYTE* data, DWORD size,
                                 int iconWidth, int iconHeight);

#endif
