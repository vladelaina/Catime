/**
 * @file window_menus.h
 * @brief Menu construction and preview dispatch system
 */

#ifndef WINDOW_MENUS_H
#define WINDOW_MENUS_H

#include <windows.h>

/* ============================================================================
 * File System Scanning
 * ============================================================================ */

typedef BOOL (*FileFilterFunc)(const wchar_t* filename);
typedef BOOL (*FileActionFunc)(const char* relPath, void* userData);

/**
 * @brief Check if filename is animation file
 */
BOOL IsAnimationFile(const wchar_t* filename);

/**
 * @brief Check if filename is font file
 */
BOOL IsFontFile(const wchar_t* filename);

/**
 * @brief Recursively map menu ID to file path
 */
BOOL RecursiveFindFile(const wchar_t* rootPathW, const char* relPathUtf8,
                       FileFilterFunc filter, UINT targetId, UINT* currentId,
                       FileActionFunc action, void* userData);

/**
 * @brief Map font menu ID back to file path
 */
BOOL FindFontByIdRecursiveW(const wchar_t* folderPathW, int targetId, int* currentId,
                            wchar_t* foundRelativePathW, const wchar_t* fontsFolderRootW);

/**
 * @brief Map animation menu ID to file and start preview
 */
BOOL FindAnimationByIdRecursive(const wchar_t* folderPathW, const char* relPathUtf8, 
                                UINT* nextIdPtr, UINT targetId);

/* ============================================================================
 * Preview System
 * ============================================================================ */

/**
 * @brief Dispatch menu hover preview
 * @return TRUE if preview started, FALSE otherwise
 */
BOOL DispatchMenuPreview(HWND hwnd, UINT menuId);

#endif /* WINDOW_MENUS_H */

