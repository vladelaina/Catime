/**
 * @file window_drop_import.c
 * @brief Validation and atomic import of dropped resource files.
 */

#include "window_drop_target_internal.h"

static BOOL HasExtension(const wchar_t* filename, const wchar_t* ext) {
    const wchar_t* dot = wcsrchr(filename, L'.');
    if (!dot) return FALSE;
    return _wcsicmp(dot, ext) == 0;
}
ResourceType DropImport_GetResourceType(const wchar_t* filename) {
    if (HasExtension(filename, L".ttf") || HasExtension(filename, L".otf") || HasExtension(filename, L".ttc")) {
        return RESOURCE_TYPE_FONT;
    }
    if (HasExtension(filename, L".gif") || HasExtension(filename, L".webp") || HasExtension(filename, L".ani") || HasExtension(filename, L".png") || HasExtension(filename, L".jpg") || HasExtension(filename, L".jpeg") || HasExtension(filename, L".bmp") || HasExtension(filename, L".ico") || HasExtension(filename, L".tif") || HasExtension(filename, L".tiff")) {
        return RESOURCE_TYPE_ANIMATION;
    }
    return RESOURCE_TYPE_UNKNOWN;
}
BOOL DropImport_QueryFilePathExactW(HDROP hDrop, UINT index,
                                    wchar_t* outPath, UINT outPathCount) {
    if (!hDrop || !outPath || outPathCount == 0) return FALSE;
    outPath[0] = L'\0';
    UINT pathLen = DragQueryFileW(hDrop, index, NULL, 0);
    if (pathLen == 0 || pathLen >= outPathCount) {
        return FALSE;
    }
    UINT copied = DragQueryFileW(hDrop, index, outPath, outPathCount);
    if (copied != pathLen) {
        outPath[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}
static ULONGLONG GetDropImportMaxBytes(ResourceType type) {
    if (type == RESOURCE_TYPE_FONT) {
        return DROP_IMPORT_MAX_FONT_BYTES;
    }
    if (type == RESOURCE_TYPE_ANIMATION) {
        return DROP_IMPORT_MAX_ANIMATION_BYTES;
    }
    return 0;
}
static BOOL IsDropImportFileSizeAllowed(const wchar_t* srcPath, ResourceType type) {
    ULONGLONG maxBytes = GetDropImportMaxBytes(type);
    if (!srcPath || maxBytes == 0) return FALSE;
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(srcPath, GetFileExInfoStandard, &data)) {
        LOG_WARNING("Failed to query dropped resource size: %ls (error=%lu)", srcPath, GetLastError());
        return FALSE;
    }
    ULONGLONG fileSize = ((ULONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    if (fileSize > maxBytes) {
        LOG_WARNING("Dropped resource too large: %ls (%llu bytes, limit %llu bytes)", srcPath, fileSize, maxBytes);
        return FALSE;
    }
    return TRUE;
}
BOOL DropImport_GetTargetFolderPath(ResourceType type, wchar_t* outPath, size_t size) {
    if (!outPath || size == 0) return FALSE;
    outPath[0] = L'\0';
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    if (configPath[0] == '\0') return FALSE;
    wchar_t wconfigPath[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH) == 0) {
        return FALSE;
    }
    const wchar_t* lastSep = wcsrchr(wconfigPath, L'\\');
    if (!lastSep) return FALSE;
    size_t dirLen = (size_t)(lastSep - wconfigPath);
    if (type == RESOURCE_TYPE_FONT) {
        return _snwprintf_s(outPath, size, _TRUNCATE, L"%.*ls\\resources\\fonts", (int)dirLen, wconfigPath) >= 0;
    } else if (type == RESOURCE_TYPE_ANIMATION) {
        return _snwprintf_s(outPath, size, _TRUNCATE, L"%.*ls\\resources\\animations", (int)dirLen, wconfigPath) >= 0;
    }
    return FALSE;
}
static size_t TrimTrailingPathSeparatorsLength(const wchar_t* path) {
    if (!path) return 0;
    size_t len = wcslen(path);
    while (len > 0 && (path[len - 1] == L'\\' || path[len - 1] == L'/')) {
        len--;
    }
    return len;
}
static BOOL IsPathSameOrUnderDirectory(const wchar_t* path, const wchar_t* directory) {
    if (!path || !directory || !*path || !*directory) return FALSE;
    size_t dirLen = TrimTrailingPathSeparatorsLength(directory);
    if (dirLen == 0) return FALSE;
    if (_wcsnicmp(path, directory, dirLen) != 0) {
        return FALSE;
    }
    wchar_t next = path[dirLen];
    return next == L'\0' || next == L'\\' || next == L'/';
}
void DropImport_InitializeTargetRoots(DropImportState* state) {
    if (!state) return;
    DropImport_GetTargetFolderPath(RESOURCE_TYPE_FONT, state->fontTargetRoot, MAX_PATH);
    DropImport_GetTargetFolderPath(RESOURCE_TYPE_ANIMATION, state->animTargetRoot, MAX_PATH);
}
BOOL DropImport_IsTargetSubtree(const wchar_t* dirPath, const wchar_t* rootDropPath, const DropImportState* state) {
    if (!dirPath || !rootDropPath || !state) return FALSE;
    return (IsPathSameOrUnderDirectory(state->fontTargetRoot, rootDropPath) && IsPathSameOrUnderDirectory(dirPath, state->fontTargetRoot)) ||
    (IsPathSameOrUnderDirectory(state->animTargetRoot, rootDropPath) && IsPathSameOrUnderDirectory(dirPath, state->animTargetRoot));
}
static const wchar_t* GetCachedTargetRoot(ResourceType type, const DropImportState* state) {
    if (!state) return NULL;
    if (type == RESOURCE_TYPE_FONT && state->fontTargetRoot[0] != L'\0') {
        return state->fontTargetRoot;
    }
    if (type == RESOURCE_TYPE_ANIMATION && state->animTargetRoot[0] != L'\0') {
        return state->animTargetRoot;
    }
    return NULL;
}
static BOOL CopyResourceFileAtomicW(const wchar_t* srcPath, const wchar_t* targetDir, const wchar_t* destPath) {
    if (!srcPath || !targetDir || !destPath) return FALSE;
    wchar_t tempPath[MAX_PATH] = {
        0
    };
    if (GetTempFileNameW(targetDir, L"ctd", 0, tempPath) == 0) {
        LOG_ERROR("Failed to create temporary dropped resource file in: %ls (error=%lu)", targetDir, GetLastError());
        return FALSE;
    }
    BOOL success = CopyFileW(srcPath, tempPath, FALSE) &&
    MoveFileExW(tempPath, destPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!success) {
        DWORD error = GetLastError();
        DeleteFileW(tempPath);
        SetLastError(error);
    }
    return success;
}
BOOL DropImport_ImportResourceFile(const wchar_t* srcPath, ResourceType type, const DropImportState* state, const wchar_t* relativeDir, wchar_t* outNewPath, size_t size) {
    wchar_t baseDir[MAX_PATH];
    wchar_t targetDir[MAX_PATH];
    const wchar_t* cachedBaseDir = GetCachedTargetRoot(type, state);
    if (cachedBaseDir) {
        if (wcscpy_s(baseDir, MAX_PATH, cachedBaseDir) != 0) {
            return FALSE;
        }
    } else if (!DropImport_GetTargetFolderPath(type, baseDir, MAX_PATH)) {
        return FALSE;
    }
    int targetDirLen = (relativeDir && *relativeDir)
    ? _snwprintf_s(targetDir, MAX_PATH, _TRUNCATE, L"%s\\%s", baseDir, relativeDir)
    : _snwprintf_s(targetDir, MAX_PATH, _TRUNCATE, L"%s", baseDir);
    if (targetDirLen < 0) {
        LOG_WARNING("Dropped resource target directory path too long");
        return FALSE;
    }
    int createResult = SHCreateDirectoryExW(NULL, targetDir, NULL);
    if (createResult != ERROR_SUCCESS && createResult != ERROR_FILE_EXISTS && createResult != ERROR_ALREADY_EXISTS) {
        LOG_ERROR("Failed to create dropped resource directory: %ls (error=%d)", targetDir, createResult);
        return FALSE;
    }
    DWORD targetAttrs = GetFileAttributesW(targetDir);
    if (targetAttrs == INVALID_FILE_ATTRIBUTES || !(targetAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
        LOG_ERROR("Dropped resource target is not a directory: %ls", targetDir);
        return FALSE;
    }
    const wchar_t* fileName = wcsrchr(srcPath, L'\\');
    if (fileName) fileName++;
    else fileName = srcPath;
    if (_snwprintf_s(outNewPath, size, _TRUNCATE, L"%s\\%s", targetDir, fileName) < 0) {
        LOG_WARNING("Dropped resource destination path too long");
        return FALSE;
    }
    if (_wcsicmp(srcPath, outNewPath) == 0) {
        return TRUE;
        /* Already in place */
    }
    if (!IsDropImportFileSizeAllowed(srcPath, type)) {
        return FALSE;
    }
    if (CopyResourceFileAtomicW(srcPath, targetDir, outNewPath)) {
        return TRUE;
    }
    LOG_ERROR("Failed to import dropped resource: %ls -> %ls (error=%lu)", srcPath, outNewPath, GetLastError());
    return FALSE;
}
