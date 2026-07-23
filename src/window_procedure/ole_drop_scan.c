#include "ole_drop_scan.h"
#include <wchar.h>

#define DROP_PREVIEW_SCAN_ENTRY_BUDGET 512u
#define DROP_PREVIEW_SCAN_DEPTH_LIMIT 8u

BOOL OleDrop_IsFontFile(const wchar_t* filename) {
    const wchar_t* ext = filename ? wcsrchr(filename, L'.') : NULL;
    return ext && (_wcsicmp(ext, L".ttf") == 0 ||
                   _wcsicmp(ext, L".otf") == 0 ||
                   _wcsicmp(ext, L".ttc") == 0);
}

BOOL OleDrop_IsAnimationFile(const wchar_t* filename) {
    const wchar_t* ext = filename ? wcsrchr(filename, L'.') : NULL;
    return ext && (_wcsicmp(ext, L".gif") == 0 ||
                   _wcsicmp(ext, L".webp") == 0 ||
                   _wcsicmp(ext, L".ani") == 0 ||
                   _wcsicmp(ext, L".ico") == 0 ||
                   _wcsicmp(ext, L".png") == 0 ||
                   _wcsicmp(ext, L".jpg") == 0 ||
                   _wcsicmp(ext, L".jpeg") == 0 ||
                   _wcsicmp(ext, L".bmp") == 0 ||
                   _wcsicmp(ext, L".tif") == 0 ||
                   _wcsicmp(ext, L".tiff") == 0);
}

BOOL OleDrop_QueryFilePathExactW(HDROP drop, UINT index, wchar_t* path,
                                 UINT pathCount) {
    if (!drop || !path || pathCount == 0) return FALSE;
    path[0] = L'\0';
    UINT pathLen = DragQueryFileW(drop, index, NULL, 0);
    if (pathLen == 0 || pathLen >= pathCount) return FALSE;
    UINT copied = DragQueryFileW(drop, index, path, pathCount);
    if (copied != pathLen) {
        path[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

BOOL OleDrop_IsResourceScanResolved(const ResourceScanResult* scan) {
    return scan && (scan->truncated ||
                    (scan->fontCount > 1 && scan->animCount > 1));
}

static void RecordScannedResource(ResourceScanResult* scan,
                                  const wchar_t* filePath) {
    if (OleDrop_IsFontFile(filePath)) {
        if (scan->fontCount == 0) wcscpy_s(scan->fontPath, MAX_PATH, filePath);
        if (scan->fontCount < 2) scan->fontCount++;
    } else if (OleDrop_IsAnimationFile(filePath)) {
        if (scan->animCount == 0) wcscpy_s(scan->animPath, MAX_PATH, filePath);
        if (scan->animCount < 2) scan->animCount++;
    }
}

static void ScanDirectoryLimited(const wchar_t* dirPath,
                                 ResourceScanResult* scan, unsigned depth) {
    if (!dirPath || !scan || OleDrop_IsResourceScanResolved(scan)) return;
    if (depth >= DROP_PREVIEW_SCAN_DEPTH_LIMIT) {
        scan->truncated = TRUE;
        return;
    }
    WIN32_FIND_DATAW findData;
    wchar_t searchPath[MAX_PATH];
    if (_snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", dirPath) < 0) {
        scan->truncated = TRUE;
        return;
    }
    HANDLE find = FindFirstFileW(searchPath, &findData);
    if (find == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0) continue;
        if (scan->scannedEntries >= DROP_PREVIEW_SCAN_ENTRY_BUDGET) {
            scan->truncated = TRUE;
            break;
        }
        scan->scannedEntries++;
        wchar_t fullPath[MAX_PATH];
        if (_snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", dirPath,
                         findData.cFileName) < 0) {
            scan->truncated = TRUE;
            break;
        }
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                ScanDirectoryLimited(fullPath, scan, depth + 1);
        } else {
            RecordScannedResource(scan, fullPath);
        }
    } while (!OleDrop_IsResourceScanResolved(scan) &&
             FindNextFileW(find, &findData));
    FindClose(find);
}

void OleDrop_ScanPathForResources(const wchar_t* filePath,
                                  ResourceScanResult* scan) {
    if (!filePath || !scan || OleDrop_IsResourceScanResolved(scan)) return;
    DWORD attrs = GetFileAttributesW(filePath);
    if (attrs != INVALID_FILE_ATTRIBUTES &&
        (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        ScanDirectoryLimited(filePath, scan, 0);
    } else {
        RecordScannedResource(scan, filePath);
    }
}
