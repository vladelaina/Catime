#ifndef SHORTCUT_CHECKER_INTERNAL_H
#define SHORTCUT_CHECKER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SHORTCUT_CHECK_ERROR = -1,
    SHORTCUT_NOT_FOUND = 0,
    SHORTCUT_POINTS_TO_CURRENT = 1,
    SHORTCUT_POINTS_TO_OTHER = 2
} ShortcutStatus;

ShortcutStatus ShortcutShell_CheckStatus(const char* exePath,
                                          char* shortcutPath, size_t shortcutSize,
                                          char* targetPath, size_t targetSize);
ShortcutStatus ShortcutShell_CheckPath(const char* exePath,
                                       const char* shortcutPath,
                                       char* targetPath, size_t targetSize);
bool ShortcutShell_CreateOrUpdate(const char* exePath,
                                  const char* existingShortcutPath);

/* Packaged apps must be activated through the AppsFolder namespace rather
 * than by linking directly to their protected WindowsApps executable. */
ShortcutStatus ShortcutShell_CheckPackagedStatus(
    const wchar_t* appUserModelId,
    char* shortcutPath, size_t shortcutSize,
    char* targetPath, size_t targetSize);
ShortcutStatus ShortcutShell_CheckPackagedPath(
    const wchar_t* appUserModelId,
    const char* shortcutPath,
    char* targetPath, size_t targetSize);
bool ShortcutShell_CreateOrUpdatePackaged(
    const wchar_t* appUserModelId,
    const char* existingShortcutPath);

#endif
