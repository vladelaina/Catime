#ifndef STARTUP_SHORTCUT_H
#define STARTUP_SHORTCUT_H

#include <windows.h>

/** Write a complete startup shell link to the requested path. */
BOOL StartupShortcut_Write(const wchar_t* shortcutPath,
                           const wchar_t* executablePath,
                           const wchar_t* arguments);

/** Validate target, arguments, and working directory from a shell link. */
BOOL StartupShortcut_IsCurrent(const wchar_t* shortcutPath,
                               const wchar_t* executablePath,
                               const wchar_t* arguments);

/** Replace a shell link transactionally, preserving the previous valid file. */
BOOL StartupShortcut_ReplaceAtomically(const wchar_t* shortcutPath,
                                       const wchar_t* executablePath,
                                       const wchar_t* arguments);

#endif /* STARTUP_SHORTCUT_H */
