#include "language_internal.h"
#include <stdlib.h>
#include <string.h>
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
#include "utils/compressed_resource.h"
#endif

BOOL Language_LoadResourceBuffer(UINT resourceId, char** outBuffer) {
    if (!outBuffer) return FALSE;
    *outBuffer = NULL;
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
    CompressedResourceGroup* group = NULL;
    if (!CompressedResource_LoadGroup(NULL,
                                      COMPRESSED_RESOURCE_GROUP_LANGUAGES,
                                      &group)) return FALSE;
    BOOL result = CompressedResource_CopyTextMember(group, resourceId,
                                                     outBuffer, NULL);
    CompressedResource_FreeGroup(group);
    return result;
#else
    HRSRC info = FindResourceW(NULL, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!info) return FALSE;
    DWORD size = SizeofResource(NULL, info);
    if (size == 0 || size == MAXDWORD) return FALSE;
    HGLOBAL resource = LoadResource(NULL, info);
    const char* data = resource ? (const char*)LockResource(resource) : NULL;
    if (!data) return FALSE;
    *outBuffer = (char*)malloc((size_t)size + 1);
    if (!*outBuffer) return FALSE;
    memcpy(*outBuffer, data, size);
    (*outBuffer)[size] = '\0';
    return TRUE;
#endif
}
