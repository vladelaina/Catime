#include "compressed_resource_internal.h"

#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

BOOL CompressedResource_GetMember(const CompressedResourceGroup* group,
                                  UINT resourceId,
                                  const BYTE** outData,
                                  size_t* outLength,
                                  WORD* outFlags) {
    if (outData) *outData = NULL;
    if (outLength) *outLength = 0;
    if (outFlags) *outFlags = 0;
    if (!group || !outData || !outLength ||
        resourceId == 0 || resourceId > 0xFFFFu) {
        return FALSE;
    }

    size_t memberOffset = group->payloadOffset;
    for (WORD i = 0; i < group->memberCount; i++) {
        const BYTE* entry = group->data + CTAR_GROUP_HEADER_SIZE +
                            (size_t)i * CTAR_GROUP_ENTRY_SIZE;
        WORD entryResourceId = CompressedResource_ReadU16LE(entry);
        WORD entryFlags = CompressedResource_ReadU16LE(entry + 2);
        size_t entryLength =
            (size_t)CompressedResource_ReadU32LE(entry + 4);
        if (memberOffset > group->rawSize ||
            entryLength > group->rawSize - memberOffset) {
            return FALSE;
        }
        if (entryResourceId == (WORD)resourceId) {
            *outData = group->data + memberOffset;
            *outLength = entryLength;
            if (outFlags) *outFlags = entryFlags;
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
    if (!outBuffer) return FALSE;
    *outBuffer = NULL;
    if (outLength) *outLength = 0;

    const BYTE* memberData = NULL;
    size_t memberLength = 0;
    if (!CompressedResource_GetMember(group, resourceId, &memberData,
                                      &memberLength, NULL) ||
        memberLength == SIZE_MAX ||
        memchr(memberData, '\0', memberLength) != NULL) {
        return FALSE;
    }

    char* copy = (char*)malloc(memberLength + 1);
    if (!copy) return FALSE;
    memcpy(copy, memberData, memberLength);
    copy[memberLength] = '\0';
    *outBuffer = copy;
    if (outLength) *outLength = memberLength;
    return TRUE;
}

void CompressedResource_FreeGroup(CompressedResourceGroup* group) {
    free(group);
}

#endif
