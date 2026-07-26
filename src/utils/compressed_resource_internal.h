#ifndef COMPRESSED_RESOURCE_INTERNAL_H
#define COMPRESSED_RESOURCE_INTERNAL_H

#include "utils/compressed_resource.h"

#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES

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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
struct CompressedResourceGroup {
    size_t rawSize;
    size_t payloadOffset;
    WORD memberCount;
    CompressedResourceGroupKind kind;
    BYTE data[];
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

WORD CompressedResource_ReadU16LE(const BYTE* data);
DWORD CompressedResource_ReadU32LE(const BYTE* data);
BOOL CompressedResource_GetGroupLimits(CompressedResourceGroupKind kind,
                                       const char** outMagic,
                                       size_t* outGroupLimit,
                                       size_t* outMemberLimit);
BOOL CompressedResource_ValidateGroup(CompressedResourceGroup* group);

#endif
#endif
