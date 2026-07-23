/**
 * @file tray_animation_loader_sources.c
 * @brief Builtin registry, source detection, paths, and source validation.
 */

#include "tray_animation_loader_internal.h"

static int GetCpuValue(void) {
    float value = 0.0f;
    return SystemMonitor_GetCpuUsage(&value) ? (int)(value + 0.5f) : 0;
}

static int GetMemoryValue(void) {
    float value = 0.0f;
    return SystemMonitor_GetMemoryUsage(&value) ? (int)(value + 0.5f) : 0;
}

static int GetBatteryValue(void) {
    int value = 0;
    return SystemMonitor_GetBatteryPercent(&value) ? value : 0;
}

static const BuiltinAnimDef s_builtinAnimations[] = {
    {"__logo__", CLOCK_IDM_ANIMATIONS_USE_LOGO, L"Use Logo",
     ANIM_SOURCE_LOGO, NULL},
    {"__cpu__", CLOCK_IDM_ANIMATIONS_USE_CPU, L"CPU Percent",
     ANIM_SOURCE_PERCENT, GetCpuValue},
    {"__mem__", CLOCK_IDM_ANIMATIONS_USE_MEM, L"Memory Percent",
     ANIM_SOURCE_PERCENT, GetMemoryValue},
    {"__battery__", CLOCK_IDM_ANIMATIONS_USE_BATTERY, L"Battery Percent",
     ANIM_SOURCE_PERCENT, GetBatteryValue},
    {"__capslock__", CLOCK_IDM_ANIMATIONS_USE_CAPSLOCK, L"Caps Lock",
     ANIM_SOURCE_CAPSLOCK, NULL},
    {"__none__", CLOCK_IDM_ANIMATIONS_USE_NONE, L"None",
     ANIM_SOURCE_UNKNOWN, NULL}
};

static const int s_builtinAnimationCount =
    (int)(sizeof(s_builtinAnimations) / sizeof(s_builtinAnimations[0]));

static BOOL EndsWithIgnoreCase(const char* value, const char* suffix) {
    if (!value || !suffix) return FALSE;
    size_t valueLength = strlen(value);
    size_t suffixLength = strlen(suffix);
    return suffixLength <= valueLength &&
           _stricmp(value + valueLength - suffixLength, suffix) == 0;
}

const BuiltinAnimDef* GetBuiltinAnimDef(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < s_builtinAnimationCount; ++i) {
        if (_stricmp(name, s_builtinAnimations[i].name) == 0) {
            return &s_builtinAnimations[i];
        }
    }
    return NULL;
}

const BuiltinAnimDef* GetBuiltinAnimDefById(UINT id) {
    for (int i = 0; i < s_builtinAnimationCount; ++i) {
        if (s_builtinAnimations[i].menuId == id) return &s_builtinAnimations[i];
    }
    return NULL;
}

const BuiltinAnimDef* GetBuiltinAnims(int* count) {
    if (count) *count = s_builtinAnimationCount;
    return s_builtinAnimations;
}

BOOL IsBuiltinAnimationName(const char* name) {
    return GetBuiltinAnimDef(name) != NULL;
}

AnimationSourceType DetectAnimationSourceType(const char* name) {
    if (!name || !*name) return ANIM_SOURCE_UNKNOWN;
    const BuiltinAnimDef* builtin = GetBuiltinAnimDef(name);
    if (builtin) return builtin->type;
    if (EndsWithIgnoreCase(name, ".gif")) return ANIM_SOURCE_GIF;
    if (EndsWithIgnoreCase(name, ".webp")) return ANIM_SOURCE_WEBP;
    if (EndsWithIgnoreCase(name, ".ani")) return ANIM_SOURCE_ANI;
    if (EndsWithIgnoreCase(name, ".ico") ||
        EndsWithIgnoreCase(name, ".png") ||
        EndsWithIgnoreCase(name, ".bmp") ||
        EndsWithIgnoreCase(name, ".jpg") ||
        EndsWithIgnoreCase(name, ".jpeg") ||
        EndsWithIgnoreCase(name, ".tif") ||
        EndsWithIgnoreCase(name, ".tiff")) {
        return ANIM_SOURCE_STATIC;
    }
    return ANIM_SOURCE_FOLDER;
}

BOOL IsSafeAnimationRelativePath(const char* path) {
    if (!path || !*path || path[0] == '\\' || path[0] == '/') return FALSE;
    const char* segment = path;
    while (*segment) {
        const char* end = segment;
        while (*end && *end != '\\' && *end != '/') ++end;
        size_t length = (size_t)(end - segment);
        if (!length || (length == 1 && segment[0] == '.') ||
            (length == 2 && segment[0] == '.' && segment[1] == '.')) {
            return FALSE;
        }
        segment = *end ? end + 1 : end;
    }
    return TRUE;
}

BOOL AnimationLoader_BuildPath(const char* name, char* path, size_t size) {
    if (!name || !path || !size || !IsSafeAnimationRelativePath(name)) {
        return FALSE;
    }
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    if (!base[0]) {
        path[0] = '\0';
        return FALSE;
    }
    size_t length = strlen(base);
    int written = snprintf(path, size,
        length && (base[length - 1] == '/' || base[length - 1] == '\\')
            ? "%s%s" : "%s\\%s", base, name);
    if (written < 0 || (size_t)written >= size) {
        path[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

BOOL AnimationLoader_IsFileSizeAllowed(const char* path) {
    if (!path || !*path) return FALSE;
    wchar_t widePath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1,
                            widePath, MAX_PATH) <= 0) return FALSE;
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(widePath, GetFileExInfoStandard, &data)) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Failed to query animation file size: %s (error=%lu)",
                 path, GetLastError());
        return FALSE;
    }
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return TRUE;
    ULONGLONG size = ((ULONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    if (size > ANIMATION_LOADER_MAX_FILE_BYTES) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Animation file too large: %s (%llu bytes, limit %llu bytes)",
                 path, size, (ULONGLONG)ANIMATION_LOADER_MAX_FILE_BYTES);
        return FALSE;
    }
    return TRUE;
}

BOOL AnimationLoader_IsFindDataSizeAllowed(const WIN32_FIND_DATAW* data) {
    if (!data || (data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return FALSE;
    }
    ULONGLONG size = ((ULONGLONG)data->nFileSizeHigh << 32) |
                     data->nFileSizeLow;
    return size <= ANIMATION_LOADER_MAX_FILE_BYTES;
}

BOOL AnimationLoader_IsSupportedExtension(const wchar_t* extension) {
    static const wchar_t* supported[] = {
        L".ico", L".png", L".bmp", L".jpg", L".jpeg",
        L".gif", L".webp", L".ani", L".tif", L".tiff"
    };
    if (!extension) return FALSE;
    for (size_t i = 0; i < sizeof(supported) / sizeof(supported[0]); ++i) {
        if (_wcsicmp(extension, supported[i]) == 0) return TRUE;
    }
    return FALSE;
}

BOOL AnimationLoader_IsSupportedFolderExtension(const wchar_t* extension) {
    return extension && _wcsicmp(extension, L".ani") != 0 &&
           AnimationLoader_IsSupportedExtension(extension);
}

BOOL IsValidAnimationSource(const char* name) {
    if (!name || !*name) return FALSE;
    AnimationSourceType type = DetectAnimationSourceType(name);
    if (type == ANIM_SOURCE_LOGO || type == ANIM_SOURCE_PERCENT ||
        type == ANIM_SOURCE_CAPSLOCK || _stricmp(name, "__none__") == 0) {
        return TRUE;
    }

    char fullPath[MAX_PATH] = {0};
    if (!AnimationLoader_BuildPath(name, fullPath, sizeof(fullPath))) {
        return FALSE;
    }
    wchar_t path[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, fullPath, -1,
                            path, MAX_PATH) <= 0) return FALSE;
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) return FALSE;

    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        wchar_t search[MAX_PATH] = {0};
        if (_snwprintf_s(search, MAX_PATH, _TRUNCATE,
                         L"%s\\*", path) < 0) return FALSE;
        WIN32_FIND_DATAW data;
        HANDLE find = FindFirstFileW(search, &data);
        if (find == INVALID_HANDLE_VALUE) return FALSE;
        BOOL hasImages = FALSE;
        int scanned = 0;
        do {
            if (++scanned > ANIMATION_LOADER_MAX_SCAN_ENTRIES) {
                WriteLog(LOG_LEVEL_WARNING,
                         "Animation validation scan limit reached (%d): %ls",
                         ANIMATION_LOADER_MAX_SCAN_ENTRIES, path);
                break;
            }
            const wchar_t* extension = wcsrchr(data.cFileName, L'.');
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                AnimationLoader_IsSupportedFolderExtension(extension) &&
                AnimationLoader_IsFindDataSizeAllowed(&data)) {
                hasImages = TRUE;
                break;
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
        return hasImages;
    }
    return AnimationLoader_IsSupportedExtension(wcsrchr(path, L'.')) &&
           AnimationLoader_IsFileSizeAllowed(fullPath);
}
