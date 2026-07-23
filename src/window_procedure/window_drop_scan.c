/**
 * @file window_drop_scan.c
 * @brief Bounded recursive scan of dropped directories.
 */

#include "window_drop_target_internal.h"

void DropImport_ProcessDirectoryRecursive(const wchar_t* dirPath, const wchar_t* rootDropPath, DropImportState* state, unsigned depth) {
    if (!dirPath || !rootDropPath || !state || state->truncated) return;
    if (DropImport_IsTargetSubtree(dirPath, rootDropPath, state)) return;
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
                DropImport_ProcessDirectoryRecursive(fullPath, rootDropPath, state, depth + 1);
            }
        } else {
            ResourceType type = DropImport_GetResourceType(fullPath);
            if (type != RESOURCE_TYPE_UNKNOWN) {
                wchar_t relativeDir[MAX_PATH] = {
                    0
                };
                size_t rootLen = wcslen(rootDropPath);
                if (_wcsnicmp(fullPath, rootDropPath, rootLen) == 0) {
                    const wchar_t* folderName = wcsrchr(rootDropPath, L'\\');
                    folderName = folderName ? folderName + 1 : rootDropPath;
                    wchar_t subPath[MAX_PATH] = {
                        0
                    };
                    if (fullPath[rootLen] == L'\\') {
                        if (wcscpy_s(subPath, MAX_PATH, fullPath + rootLen + 1) != 0) {
                            state->truncated = TRUE;
                            break;
                        }
                    }
                    wchar_t* lastSlash = wcsrchr(subPath, L'\\');
                    if (lastSlash) *lastSlash = L'\0';
                    else subPath[0] = L'\0';
                    // File is directly in dropped folder
                    if (_snwprintf_s(relativeDir, MAX_PATH, _TRUNCATE, L"%s", folderName) < 0) {
                        state->truncated = TRUE;
                        break;
                    }
                    if (subPath[0]) {
                        if (wcscat_s(relativeDir, MAX_PATH, L"\\") != 0 || wcscat_s(relativeDir, MAX_PATH, subPath) != 0) {
                            state->truncated = TRUE;
                            break;
                        }
                    }
                }
                wchar_t newPath[MAX_PATH];
                if (DropImport_ImportResourceFile(fullPath, type, state, relativeDir, newPath, MAX_PATH)) {
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
