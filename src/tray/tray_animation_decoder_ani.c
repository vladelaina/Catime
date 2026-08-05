#include "tray/tray_animation_decoder.h"
#include "tray_animation_decoder_ani_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    int width;
    int height;
    int bitCount;
    WORD hotspotX;
    WORD hotspotY;
    DWORD bytesInResource;
    DWORD imageOffset;
} AniIconDirectoryEntry;
BOOL IsAniDecodeCancelRequested(HANDLE cancelEvent) {
    return cancelEvent && WaitForSingleObject(cancelEvent, 0) == WAIT_OBJECT_0;
}
static int ClampAniIconDimension(int value) {
    if (value <= 0) return ANI_ICON_FALLBACK_SIZE;
    if (value > ANI_ICON_MAX_SIZE) return ANI_ICON_MAX_SIZE;
    return value;
}
void NormalizeAniIconSize(int* iconWidth, int* iconHeight) {
    if (iconWidth) {
        *iconWidth = ClampAniIconDimension(*iconWidth);
    }
    if (iconHeight) {
        *iconHeight = ClampAniIconDimension(*iconHeight);
    }
}
static UINT ClampAniFrameDelayMs(UINT delayMs) {
    if (delayMs < ANI_MIN_FRAME_DELAY_MS) return ANI_MIN_FRAME_DELAY_MS;
    if (delayMs > ANI_MAX_FRAME_DELAY_MS) return ANI_MAX_FRAME_DELAY_MS;
    return delayMs;
}
static WORD ReadLe16(const BYTE* p) {
    return (WORD)((WORD)p[0] | ((WORD)p[1] << 8));
}
DWORD ReadLe32(const BYTE* p) {
    return (DWORD)p[0] |
           ((DWORD)p[1] << 8) |
           ((DWORD)p[2] << 16) |
           ((DWORD)p[3] << 24);
}
UINT AniJiffiesToMilliseconds(DWORD jiffies, DWORD fallbackJiffies) {
    DWORD effectiveJiffies = jiffies ? jiffies : fallbackJiffies;
    if (!effectiveJiffies) {
        effectiveJiffies = ANI_DEFAULT_JIFFIES;
    }
    unsigned long long ms =
        ((unsigned long long)effectiveJiffies * 1000ull + 30ull) / 60ull;
    if (ms > ANI_MAX_FRAME_DELAY_MS) {
        ms = ANI_MAX_FRAME_DELAY_MS;
    }
    return ClampAniFrameDelayMs((UINT)ms);
}
BYTE* ReadAniFileBytes(const wchar_t* path, DWORD* outSize) {
    if (!path || !outSize) return NULL;
    *outSize = 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart <= 0 || (unsigned long long)fileSize.QuadPart > ANI_MAX_FILE_BYTES || fileSize.QuadPart > (LONGLONG)MAXDWORD) {
        CloseHandle(file);
        return NULL;
    }
    DWORD size = (DWORD)fileSize.QuadPart;
    BYTE* bytes = (BYTE*)malloc(size);
    if (!bytes) {
        CloseHandle(file);
        return NULL;
    }
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(file, bytes, size, &bytesRead, NULL);
    CloseHandle(file);
    if (!ok || bytesRead != size) {
        free(bytes);
        return NULL;
    }
    *outSize = size;
    return bytes;
}
static BOOL AppendAniFrameBlob(AniFrameBlob* frames, UINT* frameCount, const BYTE* data, DWORD size) {
    if (!frames || !frameCount || !data || size == 0) return FALSE;
    if (*frameCount >= ANI_MAX_ANIMATION_FRAMES) {
        return TRUE;
    }
    frames[*frameCount].data = data;
    frames[*frameCount].size = size;
    (*frameCount)++;
    return TRUE;
}
static void ParseAniFrameList(const BYTE* data, size_t size, AniFrameBlob* frames, UINT* frameCount, UINT depth) {
    if (!data || !frames || !frameCount) return;
    if (depth > ANI_MAX_LIST_DEPTH) return;
    size_t offset = 0;
    while (offset + 8 <= size) {
        DWORD id = ReadLe32(data + offset);
        DWORD chunkSize = ReadLe32(data + offset + 4);
        size_t payloadOffset = offset + 8;
        size_t payloadEnd = payloadOffset + (size_t)chunkSize;
        if (payloadEnd > size || payloadEnd < payloadOffset) {
            break;
        }
        const BYTE* payload = data + payloadOffset;
        if (id == ANI_FOURCC('i', 'c', 'o', 'n')) {
            AppendAniFrameBlob(frames, frameCount, payload, chunkSize);
        } else if (id == ANI_FOURCC('L', 'I', 'S', 'T') && chunkSize >= 4) {
            DWORD listType = ReadLe32(payload);
            if (listType == ANI_FOURCC('f', 'r', 'a', 'm')) {
                ParseAniFrameList(payload + 4, (size_t)chunkSize - 4u, frames, frameCount, depth + 1u);
            }
        }
        offset = payloadEnd + (chunkSize & 1u);
    }
}
static void ParseAniChunkList(const BYTE* data, size_t size, AniMetadata* meta, AniFrameBlob* frames, UINT* frameCount) {
    if (!data || !meta || !frames || !frameCount) return;
    size_t offset = 0;
    while (offset + 8 <= size) {
        DWORD id = ReadLe32(data + offset);
        DWORD chunkSize = ReadLe32(data + offset + 4);
        size_t payloadOffset = offset + 8;
        size_t payloadEnd = payloadOffset + (size_t)chunkSize;
        if (payloadEnd > size || payloadEnd < payloadOffset) {
            break;
        }
        const BYTE* payload = data + payloadOffset;
        if (id == ANI_FOURCC('a', 'n', 'i', 'h') && chunkSize >= ANI_HEADER_MIN_SIZE) {
            meta->cSteps = ReadLe32(payload + 8);
            meta->jifRate = ReadLe32(payload + 28);
        } else if (id == ANI_FOURCC('r', 'a', 't', 'e')) {
            meta->rates = payload;
            meta->rateCount = chunkSize / 4u;
        } else if (id == ANI_FOURCC('s', 'e', 'q', ' ')) {
            meta->sequence = payload;
            meta->sequenceCount = chunkSize / 4u;
        } else if (id == ANI_FOURCC('L', 'I', 'S', 'T') && chunkSize >= 4) {
            DWORD listType = ReadLe32(payload);
            if (listType == ANI_FOURCC('f', 'r', 'a', 'm')) {
                ParseAniFrameList(payload + 4, (size_t)chunkSize - 4u, frames, frameCount, 0);
            }
        } else if (id == ANI_FOURCC('i', 'c', 'o', 'n')) {
            AppendAniFrameBlob(frames, frameCount, payload, chunkSize);
        }
        offset = payloadEnd + (chunkSize & 1u);
    }
}
BOOL ParseAniRiff(const BYTE* bytes, DWORD size, AniMetadata* meta, AniFrameBlob* frames, UINT* frameCount) {
    if (!bytes || size < 12 || !meta || !frames || !frameCount) return FALSE;
    if (ReadLe32(bytes) != ANI_FOURCC('R', 'I', 'F', 'F') || ReadLe32(bytes + 8) != ANI_FOURCC('A', 'C', 'O', 'N')) {
        return FALSE;
    }
    DWORD riffSize = ReadLe32(bytes + 4);
    size_t declaredEnd = (size_t)riffSize + 8u;
    size_t parseEnd = declaredEnd < (size_t)size ? declaredEnd : (size_t)size;
    if (parseEnd <= 12) return FALSE;
    ZeroMemory(meta, sizeof(*meta));
    *frameCount = 0;
    ParseAniChunkList(bytes + 12, parseEnd - 12u, meta, frames, frameCount);
    return *frameCount > 0;
}
static int ScoreIconDirectoryEntry(int frameWidth, int frameHeight, int targetWidth, int targetHeight, int bitCount) {
    int score = abs(frameWidth - targetWidth) + abs(frameHeight - targetHeight);
    if (frameWidth < targetWidth) score += targetWidth - frameWidth;
    if (frameHeight < targetHeight) score += targetHeight - frameHeight;
    if (bitCount > 0 && bitCount < 32) score += 8;
    return score;
}
static HICON ConvertCursorHandleToIcon(HCURSOR cursor) {
    if (!cursor) return NULL;
    ICONINFO info;
    ZeroMemory(&info, sizeof(info));
    if (!GetIconInfo(cursor, &info)) {
        return NULL;
    }
    info.fIcon = TRUE;
    HICON icon = CreateIconIndirect(&info);
    if (info.hbmMask) DeleteObject(info.hbmMask);
    if (info.hbmColor) DeleteObject(info.hbmColor);
    return icon;
}
static HCURSOR CreateCursorFromAniDirectoryEntry(const BYTE* data, DWORD size, const AniIconDirectoryEntry* entry, int iconWidth, int iconHeight) {
    if (!data || !entry || entry->bytesInResource == 0 || entry->imageOffset >= size || entry->bytesInResource > size - entry->imageOffset || entry->bytesInResource > MAXDWORD - 4u) {
        return NULL;
    }
    DWORD cursorResourceSize = entry->bytesInResource + 4u;
    BYTE* cursorResource = (BYTE*)malloc(cursorResourceSize);
    if (!cursorResource) {
        return NULL;
    }
    cursorResource[0] = (BYTE)(entry->hotspotX & 0xFFu);
    cursorResource[1] = (BYTE)(entry->hotspotX >> 8);
    cursorResource[2] = (BYTE)(entry->hotspotY & 0xFFu);
    cursorResource[3] = (BYTE)(entry->hotspotY >> 8);
    memcpy(cursorResource + 4, data + entry->imageOffset, entry->bytesInResource);
    HCURSOR cursor = (HCURSOR)CreateIconFromResourceEx(cursorResource, cursorResourceSize, FALSE, 0x00030000, iconWidth, iconHeight, LR_DEFAULTCOLOR);
    free(cursorResource);
    return cursor;
}
HICON CreateIconFromAniFrameBlob(const BYTE* data, DWORD size, int iconWidth, int iconHeight) {
    if (!data || size < 6) return NULL;
    WORD reserved = ReadLe16(data);
    WORD resourceType = ReadLe16(data + 2);
    WORD imageCount = ReadLe16(data + 4);
    if (reserved != 0 || (resourceType != 1 && resourceType != 2) || imageCount == 0) {
        HICON icon = (HICON)CreateIconFromResourceEx((PBYTE)data, size, TRUE, 0x00030000, iconWidth, iconHeight, LR_DEFAULTCOLOR);
        if (icon) {
            return icon;
        }
        HCURSOR cursor = (HCURSOR)CreateIconFromResourceEx((PBYTE)data, size, FALSE, 0x00030000, iconWidth, iconHeight, LR_DEFAULTCOLOR);
        if (!cursor) {
            return NULL;
        }
        icon = ConvertCursorHandleToIcon(cursor);
        DestroyCursor(cursor);
        return icon;
    }
    const DWORD directoryHeaderSize = 6u;
    const DWORD entrySize = 16u;
    if (imageCount > ANI_MAX_ANIMATION_FRAMES || size < directoryHeaderSize + (DWORD)imageCount * entrySize) {
        return NULL;
    }
    int bestIndex = -1;
    int bestScore = INT_MAX;
    AniIconDirectoryEntry bestEntry;
    ZeroMemory(&bestEntry, sizeof(bestEntry));
    for (WORD i = 0; i < imageCount; ++i) {
        const BYTE* entry = data + directoryHeaderSize + (DWORD)i * entrySize;
        AniIconDirectoryEntry candidate;
        ZeroMemory(&candidate, sizeof(candidate));
        candidate.width = entry[0] ? entry[0] : 256;
        candidate.height = entry[1] ? entry[1] : 256;
        candidate.hotspotX = (resourceType == 2) ? ReadLe16(entry + 4) : 0;
        candidate.hotspotY = (resourceType == 2) ? ReadLe16(entry + 6) : 0;
        candidate.bitCount = (resourceType == 1) ? (int)ReadLe16(entry + 6) : 0;
        candidate.bytesInResource = ReadLe32(entry + 8);
        candidate.imageOffset = ReadLe32(entry + 12);
        if (candidate.bytesInResource == 0 || candidate.imageOffset >= size || candidate.bytesInResource > size - candidate.imageOffset) {
            continue;
        }
        int score = ScoreIconDirectoryEntry(candidate.width, candidate.height, iconWidth, iconHeight, candidate.bitCount);
        if (score < bestScore) {
            bestScore = score;
            bestIndex = (int)i;
            bestEntry = candidate;
        }
    }
    if (bestIndex < 0) {
        return NULL;
    }
    if (resourceType == 1) {
        return (HICON)CreateIconFromResourceEx((PBYTE)(data + bestEntry.imageOffset), bestEntry.bytesInResource, TRUE, 0x00030000, iconWidth, iconHeight, LR_DEFAULTCOLOR);
    }
    HCURSOR cursor = CreateCursorFromAniDirectoryEntry(data, size, &bestEntry, iconWidth, iconHeight);
    if (!cursor) {
        return NULL;
    }
    HICON icon = ConvertCursorHandleToIcon(cursor);
    DestroyCursor(cursor);
    return icon;
}
