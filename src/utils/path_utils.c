/**
 * @file path_utils.c
 * @brief Path manipulation implementation
 */

#include "utils/path_utils.h"
#include <string.h>
#include <wchar.h>

/* ============================================================================
 * Filename extraction
 * ============================================================================ */

const char* GetFileNameU8(const char* path) {
    if (!path) return NULL;
    
    const char* lastBackslash = strrchr(path, '\\');
    const char* lastForwardSlash = strrchr(path, '/');
    const char* separator = (lastBackslash > lastForwardSlash) ? lastBackslash : lastForwardSlash;
    
    return separator ? (separator + 1) : path;
}

const wchar_t* GetFileNameW(const wchar_t* path) {
    if (!path) return NULL;
    
    const wchar_t* lastBackslash = wcsrchr(path, L'\\');
    const wchar_t* lastForwardSlash = wcsrchr(path, L'/');
    const wchar_t* separator = (lastBackslash > lastForwardSlash) ? lastBackslash : lastForwardSlash;
    
    return separator ? (separator + 1) : path;
}

BOOL ExtractFileNameU8(const char* path, char* name, size_t nameSize) {
    if (!path || !name || nameSize == 0) return FALSE;
    
    const char* filename = GetFileNameU8(path);
    if (!filename) return FALSE;
    
    strncpy(name, filename, nameSize - 1);
    name[nameSize - 1] = '\0';
    return TRUE;
}

BOOL ExtractFileNameW(const wchar_t* path, wchar_t* name, size_t nameSize) {
    if (!path || !name || nameSize == 0) return FALSE;
    
    const wchar_t* filename = GetFileNameW(path);
    if (!filename) return FALSE;
    
    wcsncpy(name, filename, nameSize - 1);
    name[nameSize - 1] = L'\0';
    return TRUE;
}

/* ============================================================================
 * Directory path extraction
 * ============================================================================ */

BOOL ExtractDirectoryU8(const char* path, char* dir, size_t dirSize) {
    if (!path || !dir || dirSize == 0) return FALSE;
    
    strncpy(dir, path, dirSize - 1);
    dir[dirSize - 1] = '\0';
    
    char* lastBackslash = strrchr(dir, '\\');
    char* lastForwardSlash = strrchr(dir, '/');
    char* separator = (lastBackslash > lastForwardSlash) ? lastBackslash : lastForwardSlash;
    
    if (separator) {
        *separator = '\0';
    } else {
        dir[0] = '.';
        dir[1] = '\0';
    }
    
    return TRUE;
}

BOOL ExtractDirectoryW(const wchar_t* path, wchar_t* dir, size_t dirSize) {
    if (!path || !dir || dirSize == 0) return FALSE;
    
    wcsncpy(dir, path, dirSize - 1);
    dir[dirSize - 1] = L'\0';
    
    wchar_t* lastBackslash = wcsrchr(dir, L'\\');
    wchar_t* lastForwardSlash = wcsrchr(dir, L'/');
    wchar_t* separator = (lastBackslash > lastForwardSlash) ? lastBackslash : lastForwardSlash;
    
    if (separator) {
        *separator = L'\0';
    } else {
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
    
    /* Add separator if needed */
    if (len > 0 && base[len - 1] != '\\' && base[len - 1] != '/') {
        if (len + 1 >= baseSize) return FALSE;
        base[len] = '\\';
        base[len + 1] = '\0';
        len++;
    }
    
    /* Append component */
    if (len + strlen(component) >= baseSize) return FALSE;
    strcat_s(base, baseSize, component);

    return TRUE;
}

BOOL PathJoinW(wchar_t* base, size_t baseSize, const wchar_t* component) {
    if (!base || !component || baseSize == 0) return FALSE;
    
    size_t len = wcslen(base);
    
    /* Add separator if needed */
    if (len > 0 && base[len - 1] != L'\\' && base[len - 1] != L'/') {
        if (len + 1 >= baseSize) return FALSE;
        base[len] = L'\\';
        base[len + 1] = L'\0';
        len++;
    }
    
    /* Append component */
    if (len + wcslen(component) >= baseSize) return FALSE;
    wcscat_s(base, baseSize, component);

    return TRUE;
}

/* ============================================================================
 * Relative path calculation
 * ============================================================================ */

BOOL GetRelativePathU8(const char* root, const char* target, 
                       char* relative, size_t relativeSize) {
    if (!root || !target || !relative || relativeSize == 0) return FALSE;
    
    size_t rootLen = strlen(root);
    
    /* Check if target starts with root (case-insensitive) */
    if (_strnicmp(target, root, rootLen) != 0) return FALSE;
    
    /* Skip root prefix and leading separators */
    const char* rel = target + rootLen;
    while (*rel == '\\' || *rel == '/') rel++;
    
    strncpy(relative, rel, relativeSize - 1);
    relative[relativeSize - 1] = '\0';
    
    return TRUE;
}

BOOL GetRelativePathW(const wchar_t* root, const wchar_t* target,
                      wchar_t* relative, size_t relativeSize) {
    if (!root || !target || !relative || relativeSize == 0) return FALSE;
    
    size_t rootLen = wcslen(root);
    
    /* Check if target starts with root (case-insensitive) */
    if (_wcsnicmp(target, root, rootLen) != 0) return FALSE;
    
    /* Skip root prefix and leading separators */
    const wchar_t* rel = target + rootLen;
    while (*rel == L'\\' || *rel == L'/') rel++;
    
    wcsncpy(relative, rel, relativeSize - 1);
    relative[relativeSize - 1] = L'\0';
    
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

