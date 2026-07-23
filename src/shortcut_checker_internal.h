#ifndef SHORTCUT_CHECKER_INTERNAL_H
#define SHORTCUT_CHECKER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SHORTCUT_NOT_FOUND = 0,
    SHORTCUT_POINTS_TO_CURRENT = 1,
    SHORTCUT_POINTS_TO_OTHER = 2
} ShortcutStatus;

ShortcutStatus ShortcutShell_CheckStatus(const char* exePath,
                                         char* shortcutPath, size_t shortcutSize,
                                         char* targetPath, size_t targetSize);
bool ShortcutShell_CreateOrUpdate(const char* exePath,
                                  const char* existingShortcutPath);

#endif
