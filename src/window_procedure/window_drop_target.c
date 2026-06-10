/**
 * @file window_drop_target.c
 * @brief Handles drag and drop operations for resource import
 */

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <shlwapi.h>
#include "window_procedure/window_drop_target.h"
#include "config.h"
#include "log.h"
#include "font.h"
#include "tray/tray_animation_core.h"
#include "window_procedure/window_procedure.h"

#ifdef _MSC_VER
#pragma comment(lib, "shlwapi.lib")
#endif

extern char FONT_FILE_NAME[MAX_PATH];
extern char CLOCK_STARTUP_MODE[20];

#define DROP_IMPORT_SCAN_ENTRY_BUDGET 4096u
#define DROP_IMPORT_SCAN_DEPTH_LIMIT 16u
#define DROP_IMPORT_MAX_FONT_BYTES (64ull * 1024ull * 1024ull)
#define DROP_IMPORT_MAX_ANIMATION_BYTES (128ull * 1024ull * 1024ull)

typedef struct {
    wchar_t lastFontPath[MAX_PATH];
    wchar_t lastAnimPath[MAX_PATH];
    wchar_t fontTargetRoot[MAX_PATH];
    wchar_t animTargetRoot[MAX_PATH];
    int fontCount;
    int animCount;
    int importedCount;
    DWORD scannedEntries;
    BOOL truncated;
} DropImportState;

/**
 * @brief Check file extension
 */
static BOOL HasExtension(const wchar_t* filename, const wchar_t* ext) {
    const wchar_t* dot = wcsrchr(filename, L'.');
    if (!dot) return FALSE;
    return _wcsicmp(dot, ext) == 0;
}

/**
 * @brief Determine resource type from file extension
 */
static ResourceType GetResourceType(const wchar_t* filename) {
    if (HasExtension(filename, L".ttf") ||
        HasExtension(filename, L".otf") ||
        HasExtension(filename, L".ttc")) {
        return RESOURCE_TYPE_FONT;
    }

    if (HasExtension(filename, L".gif") ||
        HasExtension(filename, L".webp") ||
        HasExtension(filename, L".png") ||
        HasExtension(filename, L".jpg") ||
        HasExtension(filename, L".jpeg") ||
        HasExtension(filename, L".bmp") ||
        HasExtension(filename, L".ico") ||
        HasExtension(filename, L".tif") ||
        HasExtension(filename, L".tiff")) {
        return RESOURCE_TYPE_ANIMATION;
    }

    return RESOURCE_TYPE_UNKNOWN;
}

static BOOL QueryDropFilePathExactW(HDROP hDrop, UINT index,
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
        LOG_WARNING("Failed to query dropped resource size: %ls (error=%lu)",
                    srcPath, GetLastError());
        return FALSE;
    }

    ULONGLONG fileSize = ((ULONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    if (fileSize > maxBytes) {
        LOG_WARNING("Dropped resource too large: %ls (%llu bytes, limit %llu bytes)",
                    srcPath, fileSize, maxBytes);
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Get target folder path based on resource type
 */
static BOOL GetTargetFolderPath(ResourceType type, wchar_t* outPath, size_t size) {
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
        return _snwprintf_s(outPath, size, _TRUNCATE,
                            L"%.*ls\\resources\\fonts", (int)dirLen, wconfigPath) >= 0;
    } else if (type == RESOURCE_TYPE_ANIMATION) {
        return _snwprintf_s(outPath, size, _TRUNCATE,
                            L"%.*ls\\resources\\animations", (int)dirLen, wconfigPath) >= 0;
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

static void InitializeDropImportTargetRoots(DropImportState* state) {
    if (!state) return;

    GetTargetFolderPath(RESOURCE_TYPE_FONT, state->fontTargetRoot, MAX_PATH);
    GetTargetFolderPath(RESOURCE_TYPE_ANIMATION, state->animTargetRoot, MAX_PATH);
}

static BOOL IsDropImportTargetSubtree(const wchar_t* dirPath,
                                      const wchar_t* rootDropPath,
                                      const DropImportState* state) {
    if (!dirPath || !rootDropPath || !state) return FALSE;

    return (IsPathSameOrUnderDirectory(state->fontTargetRoot, rootDropPath) &&
            IsPathSameOrUnderDirectory(dirPath, state->fontTargetRoot)) ||
           (IsPathSameOrUnderDirectory(state->animTargetRoot, rootDropPath) &&
            IsPathSameOrUnderDirectory(dirPath, state->animTargetRoot));
}

/**
 * @brief Copy file to target directory and return new filename
 * @param relativeDir Optional relative directory structure to preserve (can be NULL)
 */
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

static BOOL CopyResourceFileAtomicW(const wchar_t* srcPath,
                                    const wchar_t* targetDir,
                                    const wchar_t* destPath) {
    if (!srcPath || !targetDir || !destPath) return FALSE;

    wchar_t tempPath[MAX_PATH] = {0};
    if (GetTempFileNameW(targetDir, L"ctd", 0, tempPath) == 0) {
        LOG_ERROR("Failed to create temporary dropped resource file in: %ls (error=%lu)",
                  targetDir, GetLastError());
        return FALSE;
    }

    BOOL success = CopyFileW(srcPath, tempPath, FALSE) &&
                   MoveFileExW(tempPath, destPath,
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!success) {
        DWORD error = GetLastError();
        DeleteFileW(tempPath);
        SetLastError(error);
    }

    return success;
}

static BOOL ImportResourceFile(const wchar_t* srcPath,
                               ResourceType type,
                               const DropImportState* state,
                               const wchar_t* relativeDir,
                               wchar_t* outNewPath,
                               size_t size) {
    wchar_t baseDir[MAX_PATH];
    wchar_t targetDir[MAX_PATH];
    const wchar_t* cachedBaseDir = GetCachedTargetRoot(type, state);
    if (cachedBaseDir) {
        if (wcscpy_s(baseDir, MAX_PATH, cachedBaseDir) != 0) {
            return FALSE;
        }
    } else if (!GetTargetFolderPath(type, baseDir, MAX_PATH)) {
        return FALSE;
    }

    int targetDirLen = (relativeDir && *relativeDir)
        ? _snwprintf_s(targetDir, MAX_PATH, _TRUNCATE, L"%s\\%s", baseDir, relativeDir)
        : _snwprintf_s(targetDir, MAX_PATH, _TRUNCATE, L"%s", baseDir);
    if (targetDirLen < 0) {
        LOG_WARNING("Dropped resource target directory path too long");
        return FALSE;
    }

    /* Ensure directory exists */
    int createResult = SHCreateDirectoryExW(NULL, targetDir, NULL);
    if (createResult != ERROR_SUCCESS &&
        createResult != ERROR_FILE_EXISTS &&
        createResult != ERROR_ALREADY_EXISTS) {
        LOG_ERROR("Failed to create dropped resource directory: %ls (error=%d)",
                  targetDir, createResult);
        return FALSE;
    }

    DWORD targetAttrs = GetFileAttributesW(targetDir);
    if (targetAttrs == INVALID_FILE_ATTRIBUTES ||
        !(targetAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
        LOG_ERROR("Dropped resource target is not a directory: %ls", targetDir);
        return FALSE;
    }

    /* Extract filename */
    const wchar_t* fileName = wcsrchr(srcPath, L'\\');
    if (fileName) fileName++;
    else fileName = srcPath;

    /* Build destination path */
    if (_snwprintf_s(outNewPath, size, _TRUNCATE, L"%s\\%s", targetDir, fileName) < 0) {
        LOG_WARNING("Dropped resource destination path too long");
        return FALSE;
    }

    /* Check if source and dest are the same */
    if (_wcsicmp(srcPath, outNewPath) == 0) {
        return TRUE; /* Already in place */
    }

    if (!IsDropImportFileSizeAllowed(srcPath, type)) {
        return FALSE;
    }

    /* Copy through a same-directory temp file so a failed import cannot corrupt an existing resource. */
    if (CopyResourceFileAtomicW(srcPath, targetDir, outNewPath)) {
        return TRUE;
    }

    LOG_ERROR("Failed to import dropped resource: %ls -> %ls (error=%lu)",
              srcPath, outNewPath, GetLastError());
    return FALSE;
}

static void ProcessDirectoryRecursive(const wchar_t* dirPath,
                                      const wchar_t* rootDropPath,
                                      DropImportState* state,
                                      unsigned depth) {
    if (!dirPath || !rootDropPath || !state || state->truncated) return;
    if (IsDropImportTargetSubtree(dirPath, rootDropPath, state)) return;
    if (depth >= DROP_IMPORT_SCAN_DEPTH_LIMIT) {
        state->truncated = TRUE;
        return;
    }

    WIN32_FIND_DATAW findData;
    wchar_t searchPath[MAX_PATH];

    if (_snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", dirPath) < 0) {
        state->truncated = TRUE;
        return;
    }

    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        if (state->scannedEntries >= DROP_IMPORT_SCAN_ENTRY_BUDGET) {
            state->truncated = TRUE;
            break;
        }
        state->scannedEntries++;

        wchar_t fullPath[MAX_PATH];
        if (_snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", dirPath, findData.cFileName) < 0) {
            state->truncated = TRUE;
            break;
        }

        BOOL isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDirectory) {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                ProcessDirectoryRecursive(fullPath, rootDropPath, state, depth + 1);
            }
        } else {
            ResourceType type = GetResourceType(fullPath);
            if (type != RESOURCE_TYPE_UNKNOWN) {
                /* Calculate relative path from root drop path to maintain structure */
                wchar_t relativeDir[MAX_PATH] = {0};

                /* Get parent directory of the dropped root to calculate relative path including the dropped folder name */
                size_t rootLen = wcslen(rootDropPath);

                /* Check if fullPath starts with rootDropPath */
                if (_wcsnicmp(fullPath, rootDropPath, rootLen) == 0) {
                    /* Extract folder name from rootDropPath */
                    const wchar_t* folderName = wcsrchr(rootDropPath, L'\\');
                    folderName = folderName ? folderName + 1 : rootDropPath;

                    wchar_t subPath[MAX_PATH] = {0};
                    if (fullPath[rootLen] == L'\\') {
                        if (wcscpy_s(subPath, MAX_PATH, fullPath + rootLen + 1) != 0) {
                            state->truncated = TRUE;
                            break;
                        }
                    }

                    /* Remove filename from subPath to get directory */
                    wchar_t* lastSlash = wcsrchr(subPath, L'\\');
                    if (lastSlash) *lastSlash = L'\0';
                    else subPath[0] = L'\0'; // File is directly in dropped folder

                    /* Construct final relative dir: "FolderName\SubPath" */
                    if (_snwprintf_s(relativeDir, MAX_PATH, _TRUNCATE, L"%s", folderName) < 0) {
                        state->truncated = TRUE;
                        break;
                    }
                    if (subPath[0]) {
                        if (wcscat_s(relativeDir, MAX_PATH, L"\\") != 0 ||
                            wcscat_s(relativeDir, MAX_PATH, subPath) != 0) {
                            state->truncated = TRUE;
                            break;
                        }
                    }
                }

                wchar_t newPath[MAX_PATH];
                if (ImportResourceFile(fullPath, type, state, relativeDir, newPath, MAX_PATH)) {
                    state->importedCount++;
                    if (type == RESOURCE_TYPE_FONT) {
                        wcscpy_s(state->lastFontPath, MAX_PATH, newPath);
                        state->fontCount++;
                    } else if (type == RESOURCE_TYPE_ANIMATION) {
                        wcscpy_s(state->lastAnimPath, MAX_PATH, newPath);
                        state->animCount++;
                    }
                }
            }
        }
    } while (!state->truncated && FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

static BOOL WidePathToUtf8(const wchar_t* path, char* outPath, size_t outSize) {
    if (!path || !outPath || outSize == 0 || outSize > INT_MAX) return FALSE;

    int written = WideCharToMultiByte(CP_UTF8, 0, path, -1,
                                      outPath, (int)outSize, NULL, NULL);
    return written > 0;
}

static BOOL GetImportedResourceRelativePath(ResourceType type,
                                            const wchar_t* importedPath,
                                            char* outPath,
                                            size_t outSize) {
    if (!importedPath || !outPath || outSize == 0) return FALSE;

    wchar_t resourceRoot[MAX_PATH];
    if (GetTargetFolderPath(type, resourceRoot, MAX_PATH)) {
        size_t rootLen = wcslen(resourceRoot);
        if (_wcsnicmp(importedPath, resourceRoot, rootLen) == 0 &&
            (importedPath[rootLen] == L'\\' || importedPath[rootLen] == L'\0')) {
            const wchar_t* relPathW = importedPath + rootLen;
            if (*relPathW == L'\\') relPathW++;
            if (*relPathW && WidePathToUtf8(relPathW, outPath, outSize)) {
                return TRUE;
            }
        }
    }

    const wchar_t* fileNameW = wcsrchr(importedPath, L'\\');
    fileNameW = fileNameW ? fileNameW + 1 : importedPath;
    return WidePathToUtf8(fileNameW, outPath, outSize);
}

/**
 * @brief Handle dropped files
 * Supports:
 * - Single files (Font/Anim)
 * - Multiple files
 * - Directories (Recursive scan)
 * - Mixed content (Files + Dirs)
 * - Preserves directory structure for dropped folders
 */
DropImportResult HandleDropFiles(HWND hwnd, HDROP hDrop) {
    DropImportResult result = {0};
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);

    if (fileCount == 0) {
        return result;
    }

    DropImportState state = {0};
    InitializeDropImportTargetRoots(&state);

    for (UINT i = 0; i < fileCount && !state.truncated; i++) {
        if (state.scannedEntries >= DROP_IMPORT_SCAN_ENTRY_BUDGET) {
            state.truncated = TRUE;
            break;
        }
        state.scannedEntries++;

        wchar_t filePath[MAX_PATH];
        if (!QueryDropFilePathExactW(hDrop, i, filePath, MAX_PATH)) {
            state.truncated = TRUE;
            break;
        }

        DWORD attrs = GetFileAttributesW(filePath);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            /* It's a directory - process recursively to find all resources */
            ProcessDirectoryRecursive(filePath, filePath, &state, 0);
        } else {
            /* It's a file */
            ResourceType type = GetResourceType(filePath);
            if (type != RESOURCE_TYPE_UNKNOWN) {
                wchar_t newPath[MAX_PATH];
                /* NULL relativeDir means put in root of resources/type */
                if (ImportResourceFile(filePath, type, &state, NULL, newPath, MAX_PATH)) {
                    state.importedCount++;
                    if (type == RESOURCE_TYPE_FONT) {
                        wcscpy_s(state.lastFontPath, MAX_PATH, newPath);
                        state.fontCount++;
                    } else if (type == RESOURCE_TYPE_ANIMATION) {
                        wcscpy_s(state.lastAnimPath, MAX_PATH, newPath);
                        state.animCount++;
                    }
                }
            }
        }
    }

    if (state.truncated) {
        LOG_WARNING("Dropped resource import truncated after %lu entries; skipped auto-apply",
                    state.scannedEntries);
        result.truncated = TRUE;
        result.movedCount = state.importedCount;
        return result;
    }

    result.movedCount = state.importedCount;

    /* Smart Auto-Apply Logic */
    if (state.fontCount == 1) {
        char relPathA[MAX_PATH] = {0};

        if (!GetImportedResourceRelativePath(RESOURCE_TYPE_FONT,
                                             state.lastFontPath,
                                             relPathA,
                                             sizeof(relPathA))) {
            LOG_WARNING("Dropped font path conversion failed");
            return result;
        }

        if (SwitchFont(GetModuleHandle(NULL), relPathA)) {
            result.fontApplied = TRUE;
            InvalidateRect(hwnd, NULL, TRUE);
        }
    }

    if (state.animCount == 1) {
        char relPathA[MAX_PATH] = {0};

        if (!GetImportedResourceRelativePath(RESOURCE_TYPE_ANIMATION,
                                             state.lastAnimPath,
                                                 relPathA,
                                                 sizeof(relPathA))) {
            LOG_WARNING("Dropped animation path conversion failed");
            return result;
        }

        result.animationApplied = SetCurrentAnimationName(relPathA);
    }

    return result;
}
