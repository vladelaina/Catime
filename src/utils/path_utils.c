/**
 * @file path_utils.c
 * @brief Path manipulation implementation
 */

#include "utils/path_utils.h"
#include "utils/string_safe.h"
#include <string.h>
#include <wchar.h>

static BOOL IsPathSeparatorU8(char c) {
    return c == '\\' || c == '/';
}

static BOOL IsPathSeparatorW(wchar_t c) {
    return c == L'\\' || c == L'/';
}

static BOOL IsPathBoundaryU8(char c) {
    return c == '\0' || IsPathSeparatorU8(c);
}

static BOOL IsPathBoundaryW(wchar_t c) {
    return c == L'\0' || IsPathSeparatorW(c);
}

/* ============================================================================
 * Filename extraction
 * ============================================================================ */

const char* GetFileNameU8(const char* path) {
    if (!path) return NULL;
    
    const char* lastBackslash = strrchr(path, '\\');
    const char* lastForwardSlash = strrchr(path, '/');
    const char* separator = lastBackslash;
    if (!separator || (lastForwardSlash && lastForwardSlash > separator)) {
        separator = lastForwardSlash;
    }
    
    return separator ? (separator + 1) : path;
}

const wchar_t* GetFileNameW(const wchar_t* path) {
    if (!path) return NULL;
    
    const wchar_t* lastBackslash = wcsrchr(path, L'\\');
    const wchar_t* lastForwardSlash = wcsrchr(path, L'/');
    const wchar_t* separator = lastBackslash;
    if (!separator || (lastForwardSlash && lastForwardSlash > separator)) {
        separator = lastForwardSlash;
    }
    
    return separator ? (separator + 1) : path;
}

BOOL ExtractFileNameU8(const char* path, char* name, size_t nameSize) {
    if (!path || !name || nameSize == 0) return FALSE;
    name[0] = '\0';
    
    const char* filename = GetFileNameU8(path);
    if (!filename) return FALSE;
    if (strlen(filename) >= nameSize) return FALSE;
    
    safe_strncpy(name, filename, nameSize);
    return TRUE;
}

BOOL ExtractFileNameW(const wchar_t* path, wchar_t* name, size_t nameSize) {
    if (!path || !name || nameSize == 0) return FALSE;
    name[0] = L'\0';
    
    const wchar_t* filename = GetFileNameW(path);
    if (!filename) return FALSE;
    if (wcslen(filename) >= nameSize) return FALSE;
    
    safe_wcsncpy(name, filename, nameSize);
    return TRUE;
}

/* ============================================================================
 * Directory path extraction
 * ============================================================================ */

BOOL ExtractDirectoryU8(const char* path, char* dir, size_t dirSize) {
    if (!path || !dir || dirSize == 0) return FALSE;
    dir[0] = '\0';

    size_t pathLen = strlen(path);
    if (pathLen >= dirSize) return FALSE;
    
    safe_strncpy(dir, path, dirSize);
    
    char* lastBackslash = strrchr(dir, '\\');
    char* lastForwardSlash = strrchr(dir, '/');
    char* separator = lastBackslash;
    if (!separator || (lastForwardSlash && lastForwardSlash > separator)) {
        separator = lastForwardSlash;
    }
    
    if (separator) {
        *separator = '\0';
    } else {
        if (dirSize < 2) return FALSE;
        dir[0] = '.';
        dir[1] = '\0';
    }
    
    return TRUE;
}

BOOL ExtractDirectoryW(const wchar_t* path, wchar_t* dir, size_t dirSize) {
    if (!path || !dir || dirSize == 0) return FALSE;
    dir[0] = L'\0';

    size_t pathLen = wcslen(path);
    if (pathLen >= dirSize) return FALSE;
    
    safe_wcsncpy(dir, path, dirSize);
    
    wchar_t* lastBackslash = wcsrchr(dir, L'\\');
    wchar_t* lastForwardSlash = wcsrchr(dir, L'/');
    wchar_t* separator = lastBackslash;
    if (!separator || (lastForwardSlash && lastForwardSlash > separator)) {
        separator = lastForwardSlash;
    }
    
    if (separator) {
        *separator = L'\0';
    } else {
        if (dirSize < 2) return FALSE;
        wcscpy_s(dir, dirSize, L".");
    }
    
    return TRUE;
}

/* ============================================================================
 * Path joining
 * ============================================================================ */

BOOL PathJoinU8(char* base, size_t baseSize, const char* component) {
    if (!base || !component || baseSize == 0) return FALSE;
    
    size_t len = strlen(base);
    if (len >= baseSize) return FALSE;

    size_t componentLen = strlen(component);
    size_t separatorLen = 0;
    
    if (len > 0 && !IsPathSeparatorU8(base[len - 1])) {
        separatorLen = 1;
    }

    if (componentLen >= baseSize - len - separatorLen) return FALSE;

    if (separatorLen) {
        base[len] = '\\';
        base[len + 1] = '\0';
    }
    strcat_s(base, baseSize, component);

    return TRUE;
}

BOOL PathJoinW(wchar_t* base, size_t baseSize, const wchar_t* component) {
    if (!base || !component || baseSize == 0) return FALSE;
    
    size_t len = wcslen(base);
    if (len >= baseSize) return FALSE;

    size_t componentLen = wcslen(component);
    size_t separatorLen = 0;
    
    if (len > 0 && !IsPathSeparatorW(base[len - 1])) {
        separatorLen = 1;
    }

    if (componentLen >= baseSize - len - separatorLen) return FALSE;

    if (separatorLen) {
        base[len] = L'\\';
        base[len + 1] = L'\0';
    }
    wcscat_s(base, baseSize, component);

    return TRUE;
}

/* ============================================================================
 * Relative path calculation
 * ============================================================================ */

BOOL GetRelativePathU8(const char* root, const char* target, 
                       char* relative, size_t relativeSize) {
    if (!root || !target || !relative || relativeSize == 0) return FALSE;
    relative[0] = '\0';
    
    size_t rootLen = strlen(root);
    
    /* Check if target starts with root (case-insensitive) */
    if (_strnicmp(target, root, rootLen) != 0) return FALSE;
    if (rootLen > 0 && !IsPathSeparatorU8(root[rootLen - 1]) &&
        !IsPathBoundaryU8(target[rootLen])) {
        return FALSE;
    }
    
    /* Skip root prefix and leading separators */
    const char* rel = target + rootLen;
    while (IsPathSeparatorU8(*rel)) rel++;
    if (strlen(rel) >= relativeSize) return FALSE;
    
    safe_strncpy(relative, rel, relativeSize);
    
    return TRUE;
}

BOOL GetRelativePathW(const wchar_t* root, const wchar_t* target,
                      wchar_t* relative, size_t relativeSize) {
    if (!root || !target || !relative || relativeSize == 0) return FALSE;
    relative[0] = L'\0';
    
    size_t rootLen = wcslen(root);
    
    /* Check if target starts with root (case-insensitive) */
    if (_wcsnicmp(target, root, rootLen) != 0) return FALSE;
    if (rootLen > 0 && !IsPathSeparatorW(root[rootLen - 1]) &&
        !IsPathBoundaryW(target[rootLen])) {
        return FALSE;
    }
    
    /* Skip root prefix and leading separators */
    const wchar_t* rel = target + rootLen;
    while (IsPathSeparatorW(*rel)) rel++;
    if (wcslen(rel) >= relativeSize) return FALSE;
    
    safe_wcsncpy(relative, rel, relativeSize);
    
    return TRUE;
}

/* ============================================================================
 * Path normalization
 * ============================================================================ */

void NormalizePathSeparatorsU8(char* path) {
    if (!path) return;
    
    for (char* p = path; *p; p++) {
        if (*p == '/') *p = '\\';
    }
}

void NormalizePathSeparatorsW(wchar_t* path) {
    if (!path) return;
    
    for (wchar_t* p = path; *p; p++) {
        if (*p == L'/') *p = L'\\';
    }
}

void RemoveTrailingSeparatorU8(char* path) {
    if (!path) return;
    
    size_t len = strlen(path);
    if (len > 0 && (path[len - 1] == '\\' || path[len - 1] == '/')) {
        path[len - 1] = '\0';
    }
}

void RemoveTrailingSeparatorW(wchar_t* path) {
    if (!path) return;
    
    size_t len = wcslen(path);
    if (len > 0 && (path[len - 1] == L'\\' || path[len - 1] == L'/')) {
        path[len - 1] = L'\0';
    }
}

/* ============================================================================
 * Path validation
 * ============================================================================ */

BOOL PathStartsWith(const char* path, const char* prefix) {
    if (!path || !prefix) return FALSE;
    
    size_t prefixLen = strlen(prefix);
    return _strnicmp(path, prefix, prefixLen) == 0;
}
