/**
 * @file tray_animation_loader.c
 * @brief Animation resource loading implementation
 */

#include "tray/tray_animation_loader.h"
#include "utils/natural_sort.h"
#include "config.h"
#include "../resource/resource.h"
#include "system_monitor.h"
#include "log.h"
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_FOLDER_ANIMATION_FRAMES 512
#define MAX_FOLDER_ANIMATION_SCAN_ENTRIES 4096
#define MAX_ANIMATION_FILE_BYTES (128ull * 1024ull * 1024ull)
#define TRAY_ANIMATION_ICON_FALLBACK_SIZE 16
#define TRAY_ANIMATION_ICON_MAX_SIZE 256

static int ClampTrayAnimationIconDimension(int value) {
    if (value <= 0) return TRAY_ANIMATION_ICON_FALLBACK_SIZE;
    if (value > TRAY_ANIMATION_ICON_MAX_SIZE) return TRAY_ANIMATION_ICON_MAX_SIZE;
    return value;
}

static void NormalizeTrayAnimationIconSize(int* iconWidth, int* iconHeight) {
    if (iconWidth) {
        *iconWidth = ClampTrayAnimationIconDimension(*iconWidth);
    }
    if (iconHeight) {
        *iconHeight = ClampTrayAnimationIconDimension(*iconHeight);
    }
}

static void GetTrayAnimationSystemIconSize(int* iconWidth, int* iconHeight) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    NormalizeTrayAnimationIconSize(&cx, &cy);
    if (iconWidth) *iconWidth = cx;
    if (iconHeight) *iconHeight = cy;
}

static BOOL IsAnimationLoadCancelRequested(HANDLE cancelEvent) {
    return cancelEvent && WaitForSingleObject(cancelEvent, 0) == WAIT_OBJECT_0;
}

/**
 * @brief Get CPU usage for builtin animation
 */
static int GetCpuValue(void) {
    float cpu = 0.0f;
    if (SystemMonitor_GetCpuUsage(&cpu)) {
        return (int)(cpu + 0.5f);
    }
    return 0;
}

/**
 * @brief Get Memory usage for builtin animation
 */
static int GetMemValue(void) {
    float mem = 0.0f;
    if (SystemMonitor_GetMemoryUsage(&mem)) {
        return (int)(mem + 0.5f);
    }
    return 0;
}

/**
 * @brief Get Battery percent for builtin animation
 */
static int GetBatteryValue(void) {
    int percent = 0;
    if (SystemMonitor_GetBatteryPercent(&percent)) {
        return percent;
    }
    return 0;
}

/**
 * @brief Builtin animation registry
 * @note Centralized configuration for all builtin types
 */
static const BuiltinAnimDef g_builtinAnims[] = {
    { "__logo__",    CLOCK_IDM_ANIMATIONS_USE_LOGO,    L"Use Logo",        ANIM_SOURCE_LOGO,    NULL },
    { "__cpu__",     CLOCK_IDM_ANIMATIONS_USE_CPU,     L"CPU Percent",     ANIM_SOURCE_PERCENT, GetCpuValue },
    { "__mem__",     CLOCK_IDM_ANIMATIONS_USE_MEM,     L"Memory Percent",  ANIM_SOURCE_PERCENT, GetMemValue },
    { "__battery__", CLOCK_IDM_ANIMATIONS_USE_BATTERY, L"Battery Percent", ANIM_SOURCE_PERCENT, GetBatteryValue },
    { "__capslock__", CLOCK_IDM_ANIMATIONS_USE_CAPSLOCK, L"Caps Lock",     ANIM_SOURCE_CAPSLOCK, NULL },
    { "__none__",    CLOCK_IDM_ANIMATIONS_USE_NONE,    L"None",            ANIM_SOURCE_UNKNOWN, NULL }
};

static const int g_builtinAnimCount = sizeof(g_builtinAnims) / sizeof(g_builtinAnims[0]);

static BOOL IsAnimationFileSizeAllowed(const char* utf8Path) {
    if (!utf8Path || !*utf8Path) return FALSE;

    wchar_t wPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH) <= 0) {
        return FALSE;
    }

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(wPath, GetFileExInfoStandard, &data)) {
        WriteLog(LOG_LEVEL_WARNING, "Failed to query animation file size: %s (error=%lu)",
                 utf8Path, GetLastError());
        return FALSE;
    }

    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return TRUE;
    }

    ULONGLONG fileSize = ((ULONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    if (fileSize > MAX_ANIMATION_FILE_BYTES) {
        WriteLog(LOG_LEVEL_WARNING, "Animation file too large: %s (%llu bytes, limit %llu bytes)",
                 utf8Path, fileSize, (ULONGLONG)MAX_ANIMATION_FILE_BYTES);
        return FALSE;
    }

    return TRUE;
}

static BOOL IsFindDataFileSizeAllowed(const WIN32_FIND_DATAW* data) {
    if (!data || (data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return FALSE;

    ULONGLONG fileSize = ((ULONGLONG)data->nFileSizeHigh << 32) | data->nFileSizeLow;
    return fileSize <= MAX_ANIMATION_FILE_BYTES;
}

/**
 * @brief Initialize loaded animation
 */
void LoadedAnimation_Init(LoadedAnimation* anim) {
    if (!anim) return;
    anim->icons = NULL;
    anim->ownsIcons = NULL;
    anim->count = 0;
    anim->capacity = 0;
    anim->delays = NULL;
    anim->isAnimated = FALSE;
    anim->sourceType = ANIM_SOURCE_UNKNOWN;
}

static BOOL LoadedAnimation_Reserve(LoadedAnimation* anim, int capacity) {
    if (!anim || capacity <= 0) return FALSE;
    if (capacity <= anim->capacity) return TRUE;

    HICON* newIcons = (HICON*)calloc((size_t)capacity, sizeof(HICON));
    BOOL* newOwnsIcons = (BOOL*)calloc((size_t)capacity, sizeof(BOOL));
    UINT* newDelays = (UINT*)calloc((size_t)capacity, sizeof(UINT));
    if (!newIcons || !newOwnsIcons || !newDelays) {
        free(newIcons);
        free(newOwnsIcons);
        free(newDelays);
        return FALSE;
    }

    if (anim->count > 0) {
        memcpy(newIcons, anim->icons, (size_t)anim->count * sizeof(HICON));
        if (anim->ownsIcons) {
            memcpy(newOwnsIcons, anim->ownsIcons, (size_t)anim->count * sizeof(BOOL));
        } else {
            for (int i = 0; i < anim->count; i++) {
                newOwnsIcons[i] = TRUE;
            }
        }
        memcpy(newDelays, anim->delays, (size_t)anim->count * sizeof(UINT));
    }

    free(anim->icons);
    free(anim->ownsIcons);
    free(anim->delays);
    anim->icons = newIcons;
    anim->ownsIcons = newOwnsIcons;
    anim->delays = newDelays;
    anim->capacity = capacity;
    return TRUE;
}

/**
 * @brief Free loaded animation
 */
void LoadedAnimation_Free(LoadedAnimation* anim) {
    if (!anim) return;
    
    for (int i = 0; i < anim->count; i++) {
        if (anim->icons[i] && (!anim->ownsIcons || anim->ownsIcons[i])) {
            DestroyIcon(anim->icons[i]);
            anim->icons[i] = NULL;
        }
    }
    
    free(anim->icons);
    free(anim->ownsIcons);
    free(anim->delays);
    anim->icons = NULL;
    anim->ownsIcons = NULL;
    anim->delays = NULL;
    anim->count = 0;
    anim->capacity = 0;
    anim->isAnimated = FALSE;
    anim->sourceType = ANIM_SOURCE_UNKNOWN;
}

static void MoveLoadedAnimationToOutput(LoadedAnimation* dst, LoadedAnimation* src) {
    if (!dst || !src || dst == src) return;
    LoadedAnimation_Free(dst);
    *dst = *src;
    LoadedAnimation_Init(src);
}

/**
 * @brief Check file extension (case-insensitive)
 */
static BOOL EndsWithIgnoreCase(const char* str, const char* suffix) {
    if (!str || !suffix) return FALSE;
    size_t ls = strlen(str);
    size_t lsuf = strlen(suffix);
    if (lsuf > ls) return FALSE;
    return _stricmp(str + (ls - lsuf), suffix) == 0;
}

static BOOL MoveDecodedAnimationToLoaded(DecodedAnimation* decoded, LoadedAnimation* anim) {
    if (!decoded || !anim || decoded->count <= 0) return FALSE;
    if (!LoadedAnimation_Reserve(anim, decoded->count)) return FALSE;

    for (int i = 0; i < decoded->count; i++) {
        anim->icons[i] = decoded->icons[i];
        anim->ownsIcons[i] = TRUE;
        anim->delays[i] = decoded->delays[i];
        decoded->icons[i] = NULL;
    }
    anim->count = decoded->count;
    anim->isAnimated = (anim->count > 1);
    return TRUE;
}

static BOOL LoadedAnimation_SetSingleIcon(LoadedAnimation* anim, HICON hIcon, BOOL ownsIcon) {
    if (!anim || !hIcon) return FALSE;
    if (!LoadedAnimation_Reserve(anim, 1)) {
        if (ownsIcon) DestroyIcon(hIcon);
        return FALSE;
    }
    anim->icons[0] = hIcon;
    anim->ownsIcons[0] = ownsIcon;
    anim->count = 1;
    anim->isAnimated = FALSE;
    return TRUE;
}

const BuiltinAnimDef* GetBuiltinAnimDef(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < g_builtinAnimCount; i++) {
        if (_stricmp(name, g_builtinAnims[i].name) == 0) {
            return &g_builtinAnims[i];
        }
    }
    return NULL;
}

const BuiltinAnimDef* GetBuiltinAnimDefById(UINT id) {
    for (int i = 0; i < g_builtinAnimCount; i++) {
        if (g_builtinAnims[i].menuId == id) {
            return &g_builtinAnims[i];
        }
    }
    return NULL;
}

const BuiltinAnimDef* GetBuiltinAnims(int* count) {
    if (count) *count = g_builtinAnimCount;
    return g_builtinAnims;
}

/**
 * @brief Detect animation source type
 */
AnimationSourceType DetectAnimationSourceType(const char* name) {
    if (!name || !*name) return ANIM_SOURCE_UNKNOWN;
    
    const BuiltinAnimDef* def = GetBuiltinAnimDef(name);
    if (def) return def->type;

    if (EndsWithIgnoreCase(name, ".gif")) return ANIM_SOURCE_GIF;
    if (EndsWithIgnoreCase(name, ".webp")) return ANIM_SOURCE_WEBP;
    if (EndsWithIgnoreCase(name, ".ani")) return ANIM_SOURCE_ANI;
    if (EndsWithIgnoreCase(name, ".ico") || EndsWithIgnoreCase(name, ".png") ||
        EndsWithIgnoreCase(name, ".bmp") || EndsWithIgnoreCase(name, ".jpg") ||
        EndsWithIgnoreCase(name, ".jpeg") || EndsWithIgnoreCase(name, ".tif") ||
        EndsWithIgnoreCase(name, ".tiff")) {
        return ANIM_SOURCE_STATIC;
    }
    
    return ANIM_SOURCE_FOLDER;
}

/**
 * @brief Build full path to animation resource
 */
static BOOL BuildAnimationPath(const char* name, char* path, size_t size) {
    if (!name || !path || size == 0) return FALSE;
    if (!IsSafeAnimationRelativePath(name)) return FALSE;

    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    if (base[0] == '\0') {
        path[0] = '\0';
        return FALSE;
    }
    
    size_t len = strlen(base);
    int pathLen = 0;
    if (len > 0 && (base[len-1] == '/' || base[len-1] == '\\')) {
        pathLen = snprintf(path, size, "%s%s", base, name);
    } else {
        pathLen = snprintf(path, size, "%s\\%s", base, name);
    }

    if (pathLen < 0 || (size_t)pathLen >= size) {
        path[0] = '\0';
        return FALSE;
    }

    return TRUE;
}

BOOL IsSafeAnimationRelativePath(const char* path) {
    if (!path || !*path) return FALSE;
    if (path[0] == '\\' || path[0] == '/') return FALSE;

    const char* segment = path;
    while (*segment) {
        const char* end = segment;
        while (*end && *end != '\\' && *end != '/') {
            end++;
        }

        size_t len = (size_t)(end - segment);
        if (len == 0 ||
            (len == 1 && segment[0] == '.') ||
            (len == 2 && segment[0] == '.' && segment[1] == '.')) {
            return FALSE;
        }

        segment = *end ? end + 1 : end;
    }

    return TRUE;
}

/**
 * @brief File sorting helper
 */
typedef struct {
    int hasNum;
    int num;
    wchar_t name[MAX_PATH];
    wchar_t path[MAX_PATH];
} AnimFile;

static int CompareAnimFile(const void* a, const void* b) {
    const AnimFile* fa = (const AnimFile*)a;
    const AnimFile* fb = (const AnimFile*)b;
    
    if (fa->hasNum && fb->hasNum) {
        if (fa->num != fb->num) {
            return fa->num < fb->num ? -1 : 1;
        }
    }
    
    return NaturalCompareW(fa->name, fb->name);
}

static BOOL IsSupportedAnimationExtensionW(const wchar_t* ext) {
    if (!ext) return FALSE;

    static const wchar_t* supportedExtensions[] = {
        L".ico", L".png", L".bmp", L".jpg", L".jpeg",
        L".gif", L".webp", L".ani", L".tif", L".tiff"
    };

    for (size_t i = 0; i < sizeof(supportedExtensions) / sizeof(supportedExtensions[0]); ++i) {
        if (_wcsicmp(ext, supportedExtensions[i]) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL IsSupportedFolderFrameExtensionW(const wchar_t* ext) {
    if (!ext || _wcsicmp(ext, L".ani") == 0) return FALSE;
    return IsSupportedAnimationExtensionW(ext);
}

/**
 * @brief Load icons from folder with natural sorting
 */
BOOL LoadIconsFromFolderWithCancel(const char* utf8FolderPath, LoadedAnimation* anim,
                                   HANDLE cancelEvent) {
    if (!utf8FolderPath || !anim) return FALSE;
    if (IsAnimationLoadCancelRequested(cancelEvent)) return FALSE;

    wchar_t wFolder[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8FolderPath, -1, wFolder, MAX_PATH) <= 0) {
        return FALSE;
    }
    if (IsAnimationLoadCancelRequested(cancelEvent)) return FALSE;
    
    int fileCapacity = 64;
    AnimFile* files = (AnimFile*)malloc(sizeof(AnimFile) * (size_t)fileCapacity);
    if (!files) return FALSE;

    int fileCount = 0;

    wchar_t wSearch[MAX_PATH] = {0};
    int searchWritten = _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", wFolder);
    if (searchWritten < 0) {
        free(files);
        return FALSE;
    }

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wSearch, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        BOOL allocationFailed = FALSE;
        BOOL reachedFrameLimit = FALSE;
        BOOL reachedScanLimit = FALSE;
        int scannedEntries = 0;
        BOOL canceled = FALSE;
        do {
            if (IsAnimationLoadCancelRequested(cancelEvent)) {
                canceled = TRUE;
                break;
            }

            if (++scannedEntries > MAX_FOLDER_ANIMATION_SCAN_ENTRIES) {
                reachedScanLimit = TRUE;
                break;
            }

            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

            const wchar_t* dot = wcsrchr(ffd.cFileName, L'.');
            if (!IsSupportedFolderFrameExtensionW(dot)) continue;
            if (!IsFindDataFileSizeAllowed(&ffd)) {
                WriteLog(LOG_LEVEL_WARNING, "Skipping oversized folder animation frame: %ls (%llu bytes)",
                         ffd.cFileName,
                         ((ULONGLONG)ffd.nFileSizeHigh << 32) | ffd.nFileSizeLow);
                continue;
            }

            size_t nameLen = (size_t)(dot - ffd.cFileName);
            if (nameLen == 0 || nameLen >= MAX_PATH) continue;

            if (fileCount >= fileCapacity) {
                if (fileCapacity >= MAX_FOLDER_ANIMATION_FRAMES) {
                    reachedFrameLimit = TRUE;
                    break;
                }
                int newCapacity = fileCapacity * 2;
                if (newCapacity > MAX_FOLDER_ANIMATION_FRAMES) {
                    newCapacity = MAX_FOLDER_ANIMATION_FRAMES;
                }
                AnimFile* newFiles = (AnimFile*)realloc(files, sizeof(AnimFile) * (size_t)newCapacity);
                if (!newFiles) {
                    allocationFailed = TRUE;
                    break;
                }
                files = newFiles;
                fileCapacity = newCapacity;
            }

            /* Extract numeric component for sorting */
            int hasNum = 0, numVal = 0;
            for (size_t i = 0; i < nameLen; ++i) {
                if (iswdigit(ffd.cFileName[i])) {
                    hasNum = 1;
                    numVal = 0;
                    while (i < nameLen && iswdigit(ffd.cFileName[i])) {
                        int digit = (int)(ffd.cFileName[i] - L'0');
                        if (numVal <= (INT_MAX - digit) / 10) {
                            numVal = numVal * 10 + digit;
                        } else {
                            numVal = INT_MAX;
                        }
                        i++;
                    }
                    break;
                }
            }

            files[fileCount].hasNum = hasNum;
            files[fileCount].num = numVal;
            wcsncpy(files[fileCount].name, ffd.cFileName, nameLen);
            files[fileCount].name[nameLen] = L'\0';
            int pathWritten = _snwprintf_s(files[fileCount].path, MAX_PATH, _TRUNCATE,
                                           L"%s\\%s", wFolder, ffd.cFileName);
            if (pathWritten < 0) continue;
            fileCount++;
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);

        if (canceled) {
            free(files);
            return FALSE;
        }

        if (reachedFrameLimit) {
            WriteLog(LOG_LEVEL_WARNING, "Folder animation frame limit reached (%d), ignoring remaining files",
                     MAX_FOLDER_ANIMATION_FRAMES);
        }
        if (reachedScanLimit) {
            WriteLog(LOG_LEVEL_WARNING, "Folder animation scan limit reached (%d), ignoring remaining files",
                     MAX_FOLDER_ANIMATION_SCAN_ENTRIES);
        }

        if (allocationFailed) {
            free(files);
            return FALSE;
        }
    }
    
    if (fileCount == 0) {
        free(files);
        return FALSE;
    }
    if (IsAnimationLoadCancelRequested(cancelEvent)) {
        free(files);
        return FALSE;
    }

    LoadedAnimation loaded;
    LoadedAnimation_Init(&loaded);
    loaded.sourceType = ANIM_SOURCE_FOLDER;

    if (!LoadedAnimation_Reserve(&loaded, fileCount)) {
        free(files);
        return FALSE;
    }
    
    /* Sort by numeric value then name */
    qsort(files, (size_t)fileCount, sizeof(AnimFile), CompareAnimFile);
    
    /* Load icons */
    int cx = 0;
    int cy = 0;
    GetTrayAnimationSystemIconSize(&cx, &cy);
    HRESULT wicInitResult = E_FAIL;
    IWICImagingFactory* staticImageFactory = NULL;
    BOOL attemptedStaticImageFactory = FALSE;
    BOOL canceled = FALSE;
    
    for (int i = 0; i < fileCount; ++i) {
        if (IsAnimationLoadCancelRequested(cancelEvent)) {
            canceled = TRUE;
            break;
        }

        HICON hIcon = NULL;
        const wchar_t* ext = wcsrchr(files[i].path, L'.');
        
        if (ext && _wcsicmp(ext, L".ico") == 0) {
            hIcon = (HICON)LoadImageW(NULL, files[i].path, IMAGE_ICON, cx, cy, LR_LOADFROMFILE);
        } else {
            if (!staticImageFactory && !attemptedStaticImageFactory) {
                attemptedStaticImageFactory = TRUE;
                wicInitResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
                if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                            &IID_IWICImagingFactory, (void**)&staticImageFactory))) {
                    staticImageFactory = NULL;
                }
            }
            hIcon = DecodeStaticImageWithFactory(staticImageFactory, files[i].path, cx, cy);
        }

        if (IsAnimationLoadCancelRequested(cancelEvent)) {
            if (hIcon) {
                DestroyIcon(hIcon);
            }
            canceled = TRUE;
            break;
        }

        if (hIcon) {
            loaded.icons[loaded.count++] = hIcon;
            loaded.ownsIcons[loaded.count - 1] = TRUE;
        }
    }

    if (staticImageFactory) {
        staticImageFactory->lpVtbl->Release(staticImageFactory);
    }
    if (SUCCEEDED(wicInitResult)) {
        CoUninitialize();
    }
    
    free(files);
    if (canceled || loaded.count == 0) {
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }

    loaded.isAnimated = (loaded.count > 1);
    MoveLoadedAnimationToOutput(anim, &loaded);
    return TRUE;
}

BOOL LoadIconsFromFolder(const char* utf8FolderPath, LoadedAnimation* anim) {
    return LoadIconsFromFolderWithCancel(utf8FolderPath, anim, NULL);
}

/**
 * @brief Check if name is a builtin animation
 */
BOOL IsBuiltinAnimationName(const char* name) {
    return GetBuiltinAnimDef(name) != NULL;
}

/**
 * @brief Check if animation source is valid
 */
BOOL IsValidAnimationSource(const char* name) {
    if (!name || !*name) return FALSE;
    
    AnimationSourceType type = DetectAnimationSourceType(name);
    
    if (type == ANIM_SOURCE_LOGO || type == ANIM_SOURCE_PERCENT || type == ANIM_SOURCE_CAPSLOCK) {
        return TRUE;
    }
    
    /* __none__ is always valid */
    if (_stricmp(name, "__none__") == 0) {
        return TRUE;
    }
    
    char fullPath[MAX_PATH] = {0};
    if (!BuildAnimationPath(name, fullPath, sizeof(fullPath))) return FALSE;
    
    wchar_t wPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, fullPath, -1, wPath, MAX_PATH) <= 0) return FALSE;
    
    DWORD attrs = GetFileAttributesW(wPath);
    if (attrs == INVALID_FILE_ATTRIBUTES) return FALSE;
    
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        /* Check if folder contains valid image files */
        wchar_t wSearch[MAX_PATH] = {0};
        int searchWritten = _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", wPath);
        if (searchWritten < 0) return FALSE;
        
        WIN32_FIND_DATAW ffd;
        HANDLE hFind = FindFirstFileW(wSearch, &ffd);
        if (hFind == INVALID_HANDLE_VALUE) return FALSE;
        
        BOOL hasImages = FALSE;
        int scannedEntries = 0;
        do {
            if (++scannedEntries > MAX_FOLDER_ANIMATION_SCAN_ENTRIES) {
                WriteLog(LOG_LEVEL_WARNING, "Animation validation scan limit reached (%d): %ls",
                         MAX_FOLDER_ANIMATION_SCAN_ENTRIES, wPath);
                break;
            }

            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            
            const wchar_t* ext = wcsrchr(ffd.cFileName, L'.');
            if (IsSupportedFolderFrameExtensionW(ext) &&
                IsFindDataFileSizeAllowed(&ffd)) {
                hasImages = TRUE;
                break;
            }
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
        
        return hasImages;
    }
    
    const wchar_t* ext = wcsrchr(wPath, L'.');
    if (!IsSupportedAnimationExtensionW(ext)) {
        return FALSE;
    }

    return IsAnimationFileSizeAllowed(fullPath);
}

BOOL LoadAnimationByName(const char* name, LoadedAnimation* anim,
                         MemoryPool* pool, int iconWidth, int iconHeight) {
    return LoadAnimationByNameWithCancel(name, anim, pool, iconWidth, iconHeight, NULL);
}

BOOL LoadAnimationByNameWithCancel(const char* name, LoadedAnimation* anim,
                                   MemoryPool* pool, int iconWidth, int iconHeight,
                                   HANDLE cancelEvent) {
    if (!name || !anim) return FALSE;
    if (IsAnimationLoadCancelRequested(cancelEvent)) return FALSE;

    NormalizeTrayAnimationIconSize(&iconWidth, &iconHeight);

    LoadedAnimation loaded;
    LoadedAnimation_Init(&loaded);
    
    AnimationSourceType type = DetectAnimationSourceType(name);
    loaded.sourceType = type;
    
    if (type == ANIM_SOURCE_LOGO) {
        HICON hIcon = LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_CATIME));
        if (LoadedAnimation_SetSingleIcon(&loaded, hIcon, FALSE)) {
            MoveLoadedAnimationToOutput(anim, &loaded);
            return TRUE;
        }
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_PERCENT) {
        /* Percent icons are generated dynamically, not pre-loaded */
        loaded.count = 0;
        loaded.isAnimated = FALSE;
        MoveLoadedAnimationToOutput(anim, &loaded);
        return TRUE;
    }
    
    if (type == ANIM_SOURCE_CAPSLOCK) {
        /* Caps Lock icons are generated dynamically, not pre-loaded */
        loaded.count = 0;
        loaded.isAnimated = FALSE;
        MoveLoadedAnimationToOutput(anim, &loaded);
        return TRUE;
    }
    
    /* Handle __none__ (transparent icon) - no frames needed */
    if (_stricmp(name, "__none__") == 0) {
        loaded.count = 0;
        loaded.isAnimated = FALSE;
        MoveLoadedAnimationToOutput(anim, &loaded);
        return TRUE;
    }
    
    char fullPath[MAX_PATH] = {0};
    if (!BuildAnimationPath(name, fullPath, sizeof(fullPath))) {
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }
    if (IsAnimationLoadCancelRequested(cancelEvent)) {
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_GIF || type == ANIM_SOURCE_WEBP) {
        if (!IsAnimationFileSizeAllowed(fullPath)) {
            LoadedAnimation_Free(&loaded);
            return FALSE;
        }

        DecodedAnimation decoded;
        DecodedAnimation_Init(&decoded);
        
        if (DecodeAnimatedImageWithCancel(fullPath, &decoded, pool,
                                          iconWidth, iconHeight, cancelEvent)) {
            BOOL moved = MoveDecodedAnimationToLoaded(&decoded, &loaded);
            DecodedAnimation_Free(&decoded);
            if (moved) {
                MoveLoadedAnimationToOutput(anim, &loaded);
                return TRUE;
            }
        }
        
        DecodedAnimation_Free(&decoded);
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }

    if (type == ANIM_SOURCE_ANI) {
        if (!IsAnimationFileSizeAllowed(fullPath)) {
            LoadedAnimation_Free(&loaded);
            return FALSE;
        }

        DecodedAnimation decoded;
        DecodedAnimation_Init(&decoded);

        if (DecodeAniCursorWithCancel(fullPath, &decoded,
                                      iconWidth, iconHeight, cancelEvent)) {
            BOOL moved = MoveDecodedAnimationToLoaded(&decoded, &loaded);
            DecodedAnimation_Free(&decoded);
            if (moved) {
                MoveLoadedAnimationToOutput(anim, &loaded);
                return TRUE;
            }
        }

        DecodedAnimation_Free(&decoded);
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_STATIC) {
        if (!IsAnimationFileSizeAllowed(fullPath)) {
            LoadedAnimation_Free(&loaded);
            return FALSE;
        }
        if (IsAnimationLoadCancelRequested(cancelEvent)) {
            LoadedAnimation_Free(&loaded);
            return FALSE;
        }

        HICON hIcon = DecodeStaticImage(fullPath, iconWidth, iconHeight);
        if (IsAnimationLoadCancelRequested(cancelEvent)) {
            if (hIcon) {
                DestroyIcon(hIcon);
            }
            LoadedAnimation_Free(&loaded);
            return FALSE;
        }
        if (LoadedAnimation_SetSingleIcon(&loaded, hIcon, TRUE)) {
            MoveLoadedAnimationToOutput(anim, &loaded);
            return TRUE;
        }
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_FOLDER) {
        if (LoadIconsFromFolderWithCancel(fullPath, &loaded, cancelEvent)) {
            loaded.isAnimated = (loaded.count > 1);
            MoveLoadedAnimationToOutput(anim, &loaded);
            return TRUE;
        }
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }
    
    LoadedAnimation_Free(&loaded);
    return FALSE;
}

BOOL LoadAnimationFromPath(const char* path, LoadedAnimation* anim,
                          MemoryPool* pool, int iconWidth, int iconHeight) {
    return LoadAnimationFromPathWithCancel(path, anim, pool, iconWidth, iconHeight, NULL);
}

BOOL LoadAnimationFromPathWithCancel(const char* path, LoadedAnimation* anim,
                                     MemoryPool* pool, int iconWidth, int iconHeight,
                                     HANDLE cancelEvent) {
    if (!path || !anim) return FALSE;
    if (IsAnimationLoadCancelRequested(cancelEvent)) return FALSE;

    NormalizeTrayAnimationIconSize(&iconWidth, &iconHeight);

    LoadedAnimation loaded;
    LoadedAnimation_Init(&loaded);
    
    AnimationSourceType type = DetectAnimationSourceType(path);
    loaded.sourceType = type;
    
    /* Handle special types if path happens to be one of them (unlikely but safe) */
    if (type == ANIM_SOURCE_LOGO) {
        HICON hIcon = LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_CATIME));
        if (LoadedAnimation_SetSingleIcon(&loaded, hIcon, FALSE)) {
            MoveLoadedAnimationToOutput(anim, &loaded);
            return TRUE;
        }
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_PERCENT) {
        loaded.count = 0;
        loaded.isAnimated = FALSE;
        MoveLoadedAnimationToOutput(anim, &loaded);
        return TRUE;
    }

    if (type == ANIM_SOURCE_CAPSLOCK) {
        loaded.count = 0;
        loaded.isAnimated = FALSE;
        MoveLoadedAnimationToOutput(anim, &loaded);
        return TRUE;
    }
    
    if (type == ANIM_SOURCE_GIF || type == ANIM_SOURCE_WEBP) {
        if (!IsAnimationFileSizeAllowed(path)) {
            LoadedAnimation_Free(&loaded);
            return FALSE;
        }

        DecodedAnimation decoded;
        DecodedAnimation_Init(&decoded);
        
        if (DecodeAnimatedImageWithCancel(path, &decoded, pool,
                                          iconWidth, iconHeight, cancelEvent)) {
            BOOL moved = MoveDecodedAnimationToLoaded(&decoded, &loaded);
            DecodedAnimation_Free(&decoded);
            if (moved) {
                MoveLoadedAnimationToOutput(anim, &loaded);
                return TRUE;
            }
        }
        
        DecodedAnimation_Free(&decoded);
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }

    if (type == ANIM_SOURCE_ANI) {
        if (!IsAnimationFileSizeAllowed(path)) {
            LoadedAnimation_Free(&loaded);
            return FALSE;
        }

        DecodedAnimation decoded;
        DecodedAnimation_Init(&decoded);

        if (DecodeAniCursorWithCancel(path, &decoded,
                                      iconWidth, iconHeight, cancelEvent)) {
            BOOL moved = MoveDecodedAnimationToLoaded(&decoded, &loaded);
            DecodedAnimation_Free(&decoded);
            if (moved) {
                MoveLoadedAnimationToOutput(anim, &loaded);
                return TRUE;
            }
        }

        DecodedAnimation_Free(&decoded);
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_STATIC) {
        if (!IsAnimationFileSizeAllowed(path)) {
            LoadedAnimation_Free(&loaded);
            return FALSE;
        }
        if (IsAnimationLoadCancelRequested(cancelEvent)) {
            LoadedAnimation_Free(&loaded);
            return FALSE;
        }

        HICON hIcon = DecodeStaticImage(path, iconWidth, iconHeight);
        if (IsAnimationLoadCancelRequested(cancelEvent)) {
            if (hIcon) {
                DestroyIcon(hIcon);
            }
            LoadedAnimation_Free(&loaded);
            return FALSE;
        }
        if (LoadedAnimation_SetSingleIcon(&loaded, hIcon, TRUE)) {
            MoveLoadedAnimationToOutput(anim, &loaded);
            return TRUE;
        }
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_FOLDER) {
        if (LoadIconsFromFolderWithCancel(path, &loaded, cancelEvent)) {
            loaded.isAnimated = (loaded.count > 1);
            MoveLoadedAnimationToOutput(anim, &loaded);
            return TRUE;
        }
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }
    
    LoadedAnimation_Free(&loaded);
    return FALSE;
}
