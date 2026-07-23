#include "compressed_resource_internal.h"

#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES

#include <stdint.h>
#include <string.h>

WORD CompressedResource_ReadU16LE(const BYTE* data) {
    return (WORD)((WORD)data[0] | ((WORD)data[1] << 8));
}

DWORD CompressedResource_ReadU32LE(const BYTE* data) {
    return (DWORD)data[0] |
           ((DWORD)data[1] << 8) |
           ((DWORD)data[2] << 16) |
           ((DWORD)data[3] << 24);
}

static BOOL AddSizeChecked(size_t left, size_t right, size_t* result) {
    if (!result || left > SIZE_MAX - right) return FALSE;
    *result = left + right;
    return TRUE;
}

BOOL CompressedResource_GetGroupLimits(CompressedResourceGroupKind kind,
                                       const char** outMagic,
                                       size_t* outGroupLimit,
                                       size_t* outMemberLimit) {
    if (!outMagic || !outGroupLimit || !outMemberLimit) return FALSE;
    switch (kind) {
        case COMPRESSED_RESOURCE_GROUP_LANGUAGES:
            *outMagic = "CTLG";
            *outGroupLimit = CTAR_MAX_LANGUAGE_GROUP_SIZE;
            *outMemberLimit = CTAR_MAX_LANGUAGE_MEMBER_SIZE;
            return TRUE;
        case COMPRESSED_RESOURCE_GROUP_FONTS:
            *outMagic = "CTFT";
            *outGroupLimit = CTAR_MAX_FONT_GROUP_SIZE;
            *outMemberLimit = CTAR_MAX_FONT_MEMBER_SIZE;
            return TRUE;
        default:
            return FALSE;
    }
}

static BOOL HasDuplicateResourceId(const CompressedResourceGroup* group,
                                   WORD index, WORD resourceId) {
    for (WORD previous = 0; previous < index; previous++) {
        const BYTE* entry = group->data + CTAR_GROUP_HEADER_SIZE +
                            (size_t)previous * CTAR_GROUP_ENTRY_SIZE;
        if (CompressedResource_ReadU16LE(entry) == resourceId) return TRUE;
    }
    return FALSE;
}

BOOL CompressedResource_ValidateGroup(CompressedResourceGroup* group) {
    const char* expectedMagic = NULL;
    size_t groupLimit = 0;
    size_t memberLimit = 0;
    if (!group ||
        !CompressedResource_GetGroupLimits(
            group->kind, &expectedMagic, &groupLimit, &memberLimit) ||
        group->rawSize < CTAR_GROUP_HEADER_SIZE ||
        group->rawSize > groupLimit ||
        memcmp(group->data, expectedMagic, 4) != 0 ||
        CompressedResource_ReadU16LE(group->data + 4) != CTAR_GROUP_VERSION) {
        return FALSE;
    }

    WORD memberCount = CompressedResource_ReadU16LE(group->data + 6);
    if (memberCount == 0 || memberCount > CTAR_MAX_GROUP_MEMBERS) return FALSE;

    size_t tableSize = (size_t)memberCount * CTAR_GROUP_ENTRY_SIZE;
    size_t payloadOffset = 0;
    if (!AddSizeChecked(CTAR_GROUP_HEADER_SIZE, tableSize, &payloadOffset) ||
        payloadOffset > group->rawSize) {
        return FALSE;
    }

    size_t payloadLength = 0;
    for (WORD i = 0; i < memberCount; i++) {
        const BYTE* entry = group->data + CTAR_GROUP_HEADER_SIZE +
                            (size_t)i * CTAR_GROUP_ENTRY_SIZE;
        WORD resourceId = CompressedResource_ReadU16LE(entry);
        WORD flags = CompressedResource_ReadU16LE(entry + 2);
        size_t length = (size_t)CompressedResource_ReadU32LE(entry + 4);
        if (resourceId == 0 || flags != CTAR_GROUP_SUPPORTED_FLAGS ||
            length == 0 || length > memberLimit ||
            !AddSizeChecked(payloadLength, length, &payloadLength) ||
            HasDuplicateResourceId(group, i, resourceId)) {
            return FALSE;
        }
    }
    if (payloadLength != group->rawSize - payloadOffset) return FALSE;

    group->memberCount = memberCount;
    group->payloadOffset = payloadOffset;
    return TRUE;
}

#endif
