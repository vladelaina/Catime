#ifndef OLE_DROP_SCAN_H
#define OLE_DROP_SCAN_H

#include <windows.h>
#include <shellapi.h>

typedef struct {
    int fontCount;
    int animCount;
    wchar_t fontPath[MAX_PATH];
    wchar_t animPath[MAX_PATH];
    DWORD scannedEntries;
    BOOL truncated;
} ResourceScanResult;

BOOL OleDrop_IsFontFile(const wchar_t* filename);
BOOL OleDrop_IsAnimationFile(const wchar_t* filename);
BOOL OleDrop_QueryFilePathExactW(HDROP drop, UINT index, wchar_t* path,
                                 UINT pathCount);
BOOL OleDrop_IsResourceScanResolved(const ResourceScanResult* scan);
void OleDrop_ScanPathForResources(const wchar_t* filePath,
                                  ResourceScanResult* scan);

#endif
