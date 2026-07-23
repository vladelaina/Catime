/**
 * @file window_drop_target_internal.h
 * @brief Shared implementation details for dropped-resource importing.
 */

#ifndef CATIME_WINDOW_DROP_TARGET_INTERNAL_H
#define CATIME_WINDOW_DROP_TARGET_INTERNAL_H

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdio.h>

#include "window_procedure/window_drop_target.h"
#include "config.h"
#include "log.h"
#include "font.h"
#include "tray/tray_animation_core.h"
#include "window_procedure/window_commands.h"
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

ResourceType DropImport_GetResourceType(const wchar_t* filename);
BOOL DropImport_QueryFilePathExactW(HDROP hDrop, UINT index,
                                    wchar_t* outPath, UINT outPathCount);
BOOL DropImport_GetTargetFolderPath(ResourceType type, wchar_t* outPath,
                                    size_t size);
void DropImport_InitializeTargetRoots(DropImportState* state);
BOOL DropImport_IsTargetSubtree(const wchar_t* dirPath,
                                const wchar_t* rootDropPath,
                                const DropImportState* state);
BOOL DropImport_ImportResourceFile(const wchar_t* srcPath, ResourceType type,
                                   const DropImportState* state,
                                   const wchar_t* relativeDir,
                                   wchar_t* outNewPath, size_t size);
void DropImport_ProcessDirectoryRecursive(const wchar_t* dirPath,
                                          const wchar_t* rootDropPath,
                                          DropImportState* state,
                                          unsigned depth);

#endif /* CATIME_WINDOW_DROP_TARGET_INTERNAL_H */
