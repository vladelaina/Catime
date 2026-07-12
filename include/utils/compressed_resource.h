/**
 * @file compressed_resource.h
 * @brief Strict loader for Catime's compressed embedded asset container
 */

#ifndef COMPRESSED_RESOURCE_H
#define COMPRESSED_RESOURCE_H

#include <windows.h>
#include <stddef.h>

typedef enum {
    COMPRESSED_RESOURCE_GROUP_LANGUAGES = 1,
    COMPRESSED_RESOURCE_GROUP_FONTS = 2
} CompressedResourceGroupKind;

typedef struct CompressedResourceGroup CompressedResourceGroup;

/**
 * Load and validate one compressed group from IDR_COMPRESSED_ASSETS.
 * The returned group owns its decompressed storage and must be freed with
 * CompressedResource_FreeGroup().
 */
BOOL CompressedResource_LoadGroup(HINSTANCE hInstance,
                                  CompressedResourceGroupKind kind,
                                  CompressedResourceGroup** outGroup);

/**
 * Return a borrowed view of one member. The view remains valid until the group
 * is freed.
 */
BOOL CompressedResource_GetMember(const CompressedResourceGroup* group,
                                  UINT resourceId,
                                  const BYTE** outData,
                                  size_t* outLength,
                                  WORD* outFlags);

/**
 * Copy one member to an independently owned, writable, NUL-terminated buffer.
 * Embedded NUL bytes are rejected. The caller must free the returned buffer.
 */
BOOL CompressedResource_CopyTextMember(const CompressedResourceGroup* group,
                                       UINT resourceId,
                                       char** outBuffer,
                                       size_t* outLength);

/** Release a group and all borrowed member views. */
void CompressedResource_FreeGroup(CompressedResourceGroup* group);

#endif /* COMPRESSED_RESOURCE_H */
