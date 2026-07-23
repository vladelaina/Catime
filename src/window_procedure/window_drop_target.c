/**
 * @file window_drop_target.c
 * @brief Dropped-resource result application and command dispatch.
 */

#include "window_drop_target_internal.h"

static BOOL WidePathToUtf8(const wchar_t* path, char* outPath, size_t outSize) {
    if (!path || !outPath || outSize == 0 || outSize > INT_MAX) return FALSE;
    int written = WideCharToMultiByte(CP_UTF8, 0, path, -1, outPath, (int)outSize, NULL, NULL);
    return written > 0;
}
static BOOL GetImportedResourceRelativePath(ResourceType type, const wchar_t* importedPath, char* outPath, size_t outSize) {
    if (!importedPath || !outPath || outSize == 0) return FALSE;
    wchar_t resourceRoot[MAX_PATH];
    if (DropImport_GetTargetFolderPath(type, resourceRoot, MAX_PATH)) {
        size_t rootLen = wcslen(resourceRoot);
        if (_wcsnicmp(importedPath, resourceRoot, rootLen) == 0 && (importedPath[rootLen] == L'\\' || importedPath[rootLen] == L'\0')) {
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
DropImportResult HandleDropFiles(HWND hwnd, HDROP hDrop) {
    DropImportResult result = {
        0
    };
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
    if (fileCount == 0) {
        return result;
    }
    DropImportState state = {
        0
    };
    DropImport_InitializeTargetRoots(&state);
    for (UINT i = 0; i < fileCount && !state.truncated; i++) {
        if (state.scannedEntries >= DROP_IMPORT_SCAN_ENTRY_BUDGET) {
            state.truncated = TRUE;
            break;
        }
        state.scannedEntries++;
        wchar_t filePath[MAX_PATH];
        if (!DropImport_QueryFilePathExactW(hDrop, i, filePath, MAX_PATH)) {
            state.truncated = TRUE;
            break;
        }
        DWORD attrs = GetFileAttributesW(filePath);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            DropImport_ProcessDirectoryRecursive(filePath, filePath, &state, 0);
        } else {
            ResourceType type = DropImport_GetResourceType(filePath);
            if (type != RESOURCE_TYPE_UNKNOWN) {
                wchar_t newPath[MAX_PATH];
                if (DropImport_ImportResourceFile(filePath, type, &state, NULL, newPath, MAX_PATH)) {
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
        LOG_WARNING("Dropped resource import truncated after %lu entries; skipped auto-apply", state.scannedEntries);
        result.truncated = TRUE;
        result.movedCount = state.importedCount;
        return result;
    }
    result.movedCount = state.importedCount;
    if (state.fontCount == 1) {
        char relPathA[MAX_PATH] = {
            0
        };
        if (!GetImportedResourceRelativePath(RESOURCE_TYPE_FONT, state.lastFontPath, relPathA, sizeof(relPathA))) {
            LOG_WARNING("Dropped font path conversion failed");
            return result;
        }
        if (SwitchFont(GetModuleHandle(NULL), relPathA)) {
            result.fontApplied = TRUE;
            RefreshCustomTextDisplayDialogFont();
            InvalidateRect(hwnd, NULL, TRUE);
        }
    }
    if (state.animCount == 1) {
        char relPathA[MAX_PATH] = {
            0
        };
        if (!GetImportedResourceRelativePath(RESOURCE_TYPE_ANIMATION, state.lastAnimPath, relPathA, sizeof(relPathA))) {
            LOG_WARNING("Dropped animation path conversion failed");
            return result;
        }
        result.animationApplied = SetCurrentAnimationName(relPathA);
    }
    return result;
}
