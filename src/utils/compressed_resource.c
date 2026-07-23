/**
 * @file compressed_resource.c
 * @brief Strict CTAR v1 compressed embedded asset loader
 */

#include "compressed_resource_internal.h"

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

static BOOL AddSizeChecked(size_t left, size_t right, size_t* outValue) {
    if (!outValue || left > SIZE_MAX - right) {
        return FALSE;
    }
    *outValue = left + right;
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
    if (!CompressedResource_GetGroupLimits(
            kind, &expectedMagic, &rawLimit, &memberLimit)) {
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
        CompressedResource_ReadU16LE(container + CTAR_OFFSET_VERSION) !=
            CTAR_VERSION ||
        CompressedResource_ReadU16LE(container + CTAR_OFFSET_HEADER_SIZE) !=
            CTAR_HEADER_SIZE ||
        CompressedResource_ReadU32LE(container + CTAR_OFFSET_CONTAINER_SIZE) !=
            resourceSizeValue ||
        CompressedResource_ReadU32LE(container + CTAR_OFFSET_FLAGS) !=
            CTAR_SUPPORTED_FLAGS) {
        return FALSE;
    }

    size_t languageCompressedSize =
        (size_t)CompressedResource_ReadU32LE(
            container + CTAR_OFFSET_LANGUAGE_COMPRESSED_SIZE);
    size_t languageRawSize =
        (size_t)CompressedResource_ReadU32LE(
            container + CTAR_OFFSET_LANGUAGE_RAW_SIZE);
    size_t fontCompressedSize =
        (size_t)CompressedResource_ReadU32LE(
            container + CTAR_OFFSET_FONT_COMPRESSED_SIZE);
    size_t fontRawSize =
        (size_t)CompressedResource_ReadU32LE(
            container + CTAR_OFFSET_FONT_RAW_SIZE);

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
        !CompressedResource_ValidateGroup(group)) {
        free(group);
        return FALSE;
    }

    *outGroup = group;
    return TRUE;
}

#endif /* CATIME_COMPRESSED_EMBEDDED_RESOURCES */
