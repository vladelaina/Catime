#include "shortcut_policy.h"

#include <stddef.h>
#include <string.h>

static bool IsSeparator(char c) {
    return c == '\\' || c == '/';
}

static const char* FindPreviousSeparator(const char* begin,
                                         const char* before) {
    if (!begin || !before || before <= begin) return NULL;
    const char* cursor = before;
    while (cursor > begin) {
        --cursor;
        if (IsSeparator(*cursor)) return cursor;
    }
    return NULL;
}

static bool ComponentEquals(const char* begin, const char* end,
                            const char* expected) {
    if (!begin || !end || !expected || end < begin) return false;
    size_t length = (size_t)(end - begin);
    return strlen(expected) == length &&
           _strnicmp(begin, expected, length) == 0;
}

bool ShortcutPolicy_IsLegacyPackagedTarget(const char* targetPath,
                                            const char* packageName,
                                            const char* executableName) {
    if (!targetPath || !*targetPath || !packageName || !*packageName ||
        !executableName || !*executableName) {
        return false;
    }

    const char* end = targetPath + strlen(targetPath);
    while (end > targetPath && IsSeparator(end[-1])) --end;

    const char* fileSeparator = FindPreviousSeparator(targetPath, end);
    if (!fileSeparator ||
        !ComponentEquals(fileSeparator + 1, end, executableName)) {
        return false;
    }

    const char* packageSeparator =
        FindPreviousSeparator(targetPath, fileSeparator);
    if (!packageSeparator) return false;

    const char* packageBegin = packageSeparator + 1;
    size_t packageNameLength = strlen(packageName);
    size_t packageComponentLength =
        (size_t)(fileSeparator - packageBegin);
    if (packageComponentLength <= packageNameLength ||
        _strnicmp(packageBegin, packageName, packageNameLength) != 0 ||
        packageBegin[packageNameLength] != '_') {
        return false;
    }

    const char* windowsAppsSeparator =
        FindPreviousSeparator(targetPath, packageSeparator);
    const char* windowsAppsBegin = windowsAppsSeparator
                                       ? windowsAppsSeparator + 1
                                       : targetPath;
    return ComponentEquals(windowsAppsBegin, packageSeparator,
                           "WindowsApps");
}
