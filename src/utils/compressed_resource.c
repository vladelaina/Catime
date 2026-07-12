/**
 * @file compressed_resource.c
 * @brief Strict CTAR v1 compressed embedded asset loader
 */

#include "utils/compressed_resource.h"

#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES

#include "../../resource/resource.h"
#include "miniz_tinfl.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CTAR_HEADER_SIZE 32u
#define CTAR_VERSION 1u
#define CTAR_SUPPORTED_FLAGS 0u

#define CTAR_OFFSET_VERSION 4u
#define CTAR_OFFSET_HEADER_SIZE 6u
#define CTAR_OFFSET_CONTAINER_SIZE 8u
#define CTAR_OFFSET_LANGUAGE_COMPRESSED_SIZE 12u
#define CTAR_OFFSET_LANGUAGE_RAW_SIZE 16u
#define CTAR_OFFSET_FONT_COMPRESSED_SIZE 20u
#define CTAR_OFFSET_FONT_RAW_SIZE 24u
#define CTAR_OFFSET_FLAGS 28u

#define CTAR_GROUP_HEADER_SIZE 8u
#define CTAR_GROUP_ENTRY_SIZE 8u
#define CTAR_GROUP_VERSION 1u
#define CTAR_GROUP_SUPPORTED_FLAGS 0u

#define CTAR_MAX_CONTAINER_SIZE (128u * 1024u * 1024u)
#define CTAR_MAX_COMPRESSED_GROUP_SIZE (64u * 1024u * 1024u)
#define CTAR_MAX_LANGUAGE_GROUP_SIZE (8u * 1024u * 1024u)
#define CTAR_MAX_FONT_GROUP_SIZE (64u * 1024u * 1024u)
#define CTAR_MAX_LANGUAGE_MEMBER_SIZE (2u * 1024u * 1024u)
#define CTAR_MAX_FONT_MEMBER_SIZE (32u * 1024u * 1024u)
#define CTAR_MAX_GROUP_MEMBERS 1024u

struct CompressedResourceGroup {
    size_t rawSize;
    size_t payloadOffset;
    WORD memberCount;
    CompressedResourceGroupKind kind;
    BYTE data[];
};

static WORD ReadU16LE(const BYTE* data) {
    return (WORD)((WORD)data[0] | ((WORD)data[1] << 8));
}

static DWORD ReadU32LE(const BYTE* data) {
    return (DWORD)data[0] |
           ((DWORD)data[1] << 8) |
           ((DWORD)data[2] << 16) |
           ((DWORD)data[3] << 24);
}

static BOOL AddSizeChecked(size_t left, size_t right, size_t* outValue) {
    if (!outValue || left > SIZE_MAX - right) {
        return FALSE;
    }
    *outValue = left + right;
    return TRUE;
}

static BOOL GetGroupLimits(CompressedResourceGroupKind kind,
                           const char** outMagic,
                           size_t* outGroupLimit,
                           size_t* outMemberLimit) {
    if (!outMagic || !outGroupLimit || !outMemberLimit) {
        return FALSE;
    }

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

static BOOL ValidateGroup(CompressedResourceGroup* group) {
    const char* expectedMagic = NULL;
    size_t groupLimit = 0;
    size_t memberLimit = 0;
    if (!group ||
        !GetGroupLimits(group->kind, &expectedMagic, &groupLimit, &memberLimit) ||
        group->rawSize < CTAR_GROUP_HEADER_SIZE ||
        group->rawSize > groupLimit ||
        memcmp(group->data, expectedMagic, 4) != 0 ||
        ReadU16LE(group->data + 4) != CTAR_GROUP_VERSION) {
        return FALSE;
    }

    WORD memberCount = ReadU16LE(group->data + 6);
    if (memberCount == 0 || memberCount > CTAR_MAX_GROUP_MEMBERS) {
        return FALSE;
    }

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
        WORD resourceId = ReadU16LE(entry);
        WORD flags = ReadU16LE(entry + 2);
        size_t length = (size_t)ReadU32LE(entry + 4);

        if (resourceId == 0 ||
            flags != CTAR_GROUP_SUPPORTED_FLAGS ||
            length == 0 ||
            length > memberLimit ||
            !AddSizeChecked(payloadLength, length, &payloadLength)) {
            return FALSE;
        }

        for (WORD previous = 0; previous < i; previous++) {
            const BYTE* previousEntry = group->data + CTAR_GROUP_HEADER_SIZE +
                                        (size_t)previous * CTAR_GROUP_ENTRY_SIZE;
            if (ReadU16LE(previousEntry) == resourceId) {
                return FALSE;
            }
        }
    }

    if (payloadLength != group->rawSize - payloadOffset) {
        return FALSE;
    }

    group->memberCount = memberCount;
    group->payloadOffset = payloadOffset;
    return TRUE;
}

BOOL CompressedResource_LoadGroup(HINSTANCE hInstance,
                                  CompressedResourceGroupKind kind,
                                  CompressedResourceGroup** outGroup) {
    if (!outGroup) {
        return FALSE;
    }
    *outGroup = NULL;

    const char* expectedMagic = NULL;
    size_t rawLimit = 0;
    size_t memberLimit = 0;
    if (!GetGroupLimits(kind, &expectedMagic, &rawLimit, &memberLimit)) {
        return FALSE;
    }
    (void)expectedMagic;
    (void)memberLimit;

    HRSRC resourceInfo = FindResourceW(hInstance,
                                       MAKEINTRESOURCEW(IDR_COMPRESSED_ASSETS),
                                       RT_RCDATA);
    if (!resourceInfo) {
        return FALSE;
    }

    DWORD resourceSizeValue = SizeofResource(hInstance, resourceInfo);
    size_t resourceSize = (size_t)resourceSizeValue;
    if (resourceSize < CTAR_HEADER_SIZE ||
        resourceSize > CTAR_MAX_CONTAINER_SIZE) {
        return FALSE;
    }

    HGLOBAL resourceHandle = LoadResource(hInstance, resourceInfo);
    if (!resourceHandle) {
        return FALSE;
    }

    const BYTE* container = (const BYTE*)LockResource(resourceHandle);
    if (!container ||
        memcmp(container, "CTAR", 4) != 0 ||
        ReadU16LE(container + CTAR_OFFSET_VERSION) != CTAR_VERSION ||
        ReadU16LE(container + CTAR_OFFSET_HEADER_SIZE) != CTAR_HEADER_SIZE ||
        ReadU32LE(container + CTAR_OFFSET_CONTAINER_SIZE) != resourceSizeValue ||
        ReadU32LE(container + CTAR_OFFSET_FLAGS) != CTAR_SUPPORTED_FLAGS) {
        return FALSE;
    }

    size_t languageCompressedSize =
        (size_t)ReadU32LE(container + CTAR_OFFSET_LANGUAGE_COMPRESSED_SIZE);
    size_t languageRawSize =
        (size_t)ReadU32LE(container + CTAR_OFFSET_LANGUAGE_RAW_SIZE);
    size_t fontCompressedSize =
        (size_t)ReadU32LE(container + CTAR_OFFSET_FONT_COMPRESSED_SIZE);
    size_t fontRawSize =
        (size_t)ReadU32LE(container + CTAR_OFFSET_FONT_RAW_SIZE);

    if (languageCompressedSize == 0 ||
        languageCompressedSize > CTAR_MAX_COMPRESSED_GROUP_SIZE ||
        languageRawSize < CTAR_GROUP_HEADER_SIZE ||
        languageRawSize > CTAR_MAX_LANGUAGE_GROUP_SIZE ||
        fontCompressedSize == 0 ||
        fontCompressedSize > CTAR_MAX_COMPRESSED_GROUP_SIZE ||
        fontRawSize < CTAR_GROUP_HEADER_SIZE ||
        fontRawSize > CTAR_MAX_FONT_GROUP_SIZE) {
        return FALSE;
    }

    size_t compressedPayloadSize = 0;
    size_t expectedContainerSize = 0;
    if (!AddSizeChecked(languageCompressedSize, fontCompressedSize,
                        &compressedPayloadSize) ||
        !AddSizeChecked(CTAR_HEADER_SIZE, compressedPayloadSize,
                        &expectedContainerSize) ||
        expectedContainerSize != resourceSize) {
        return FALSE;
    }

    const BYTE* compressedData = container + CTAR_HEADER_SIZE;
    size_t compressedSize = languageCompressedSize;
    size_t rawSize = languageRawSize;
    if (kind == COMPRESSED_RESOURCE_GROUP_FONTS) {
        compressedData += languageCompressedSize;
        compressedSize = fontCompressedSize;
        rawSize = fontRawSize;
    }

    if (rawSize > rawLimit || rawSize > SIZE_MAX - sizeof(CompressedResourceGroup)) {
        return FALSE;
    }

    CompressedResourceGroup* group =
        (CompressedResourceGroup*)malloc(sizeof(*group) + rawSize);
    if (!group) {
        return FALSE;
    }

    group->rawSize = rawSize;
    group->payloadOffset = 0;
    group->memberCount = 0;
    group->kind = kind;

    tinfl_decompressor decompressor;
    tinfl_init(&decompressor);
    size_t consumed = compressedSize;
    size_t produced = rawSize;
    tinfl_status status = tinfl_decompress(
        &decompressor,
        compressedData,
        &consumed,
        group->data,
        group->data,
        &produced,
        TINFL_FLAG_PARSE_ZLIB_HEADER |
            TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);

    if (status != TINFL_STATUS_DONE ||
        consumed != compressedSize ||
        produced != rawSize ||
        !ValidateGroup(group)) {
        free(group);
        return FALSE;
    }

    *outGroup = group;
    return TRUE;
}

BOOL CompressedResource_GetMember(const CompressedResourceGroup* group,
                                  UINT resourceId,
                                  const BYTE** outData,
                                  size_t* outLength,
                                  WORD* outFlags) {
    if (outData) {
        *outData = NULL;
    }
    if (outLength) {
        *outLength = 0;
    }
    if (outFlags) {
        *outFlags = 0;
    }

    if (!group || !outData || !outLength || resourceId == 0 || resourceId > 0xFFFFu) {
        return FALSE;
    }

    size_t memberOffset = group->payloadOffset;
    for (WORD i = 0; i < group->memberCount; i++) {
        const BYTE* entry = group->data + CTAR_GROUP_HEADER_SIZE +
                            (size_t)i * CTAR_GROUP_ENTRY_SIZE;
        WORD entryResourceId = ReadU16LE(entry);
        WORD entryFlags = ReadU16LE(entry + 2);
        size_t entryLength = (size_t)ReadU32LE(entry + 4);

        if (memberOffset > group->rawSize ||
            entryLength > group->rawSize - memberOffset) {
            return FALSE;
        }

        if (entryResourceId == (WORD)resourceId) {
            *outData = group->data + memberOffset;
            *outLength = entryLength;
            if (outFlags) {
                *outFlags = entryFlags;
            }
            return TRUE;
        }

        memberOffset += entryLength;
    }

    return FALSE;
}

BOOL CompressedResource_CopyTextMember(const CompressedResourceGroup* group,
                                       UINT resourceId,
                                       char** outBuffer,
                                       size_t* outLength) {
    if (!outBuffer) {
        return FALSE;
    }
    *outBuffer = NULL;
    if (outLength) {
        *outLength = 0;
    }

    const BYTE* memberData = NULL;
    size_t memberLength = 0;
    if (!CompressedResource_GetMember(group, resourceId, &memberData,
                                      &memberLength, NULL) ||
        memberLength == SIZE_MAX ||
        memchr(memberData, '\0', memberLength) != NULL) {
        return FALSE;
    }

    char* copy = (char*)malloc(memberLength + 1);
    if (!copy) {
        return FALSE;
    }

    memcpy(copy, memberData, memberLength);
    copy[memberLength] = '\0';
    *outBuffer = copy;
    if (outLength) {
        *outLength = memberLength;
    }
    return TRUE;
}

void CompressedResource_FreeGroup(CompressedResourceGroup* group) {
    free(group);
}

#endif /* CATIME_COMPRESSED_EMBEDDED_RESOURCES */
