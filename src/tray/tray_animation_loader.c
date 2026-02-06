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
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * @brief Get CPU usage for builtin animation
 */
static int GetCpuValue(void) {
    float cpu = 0.0f, mem = 0.0f;
    if (SystemMonitor_GetUsage(&cpu, &mem)) {
        return (int)(cpu + 0.5f);
    }
    return 0;
}

/**
 * @brief Get Memory usage for builtin animation
 */
static int GetMemValue(void) {
    float cpu = 0.0f, mem = 0.0f;
    if (SystemMonitor_GetUsage(&cpu, &mem)) {
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

/**
 * @brief Initialize loaded animation
 */
void LoadedAnimation_Init(LoadedAnimation* anim) {
    if (!anim) return;
    memset(anim->icons, 0, sizeof(anim->icons));
    anim->count = 0;
    memset(anim->delays, 0, sizeof(anim->delays));
    anim->isAnimated = FALSE;
    anim->sourceType = ANIM_SOURCE_UNKNOWN;
}

/**
 * @brief Free loaded animation
 */
void LoadedAnimation_Free(LoadedAnimation* anim) {
    if (!anim) return;
    
    for (int i = 0; i < anim->count; i++) {
        if (anim->icons[i]) {
            DestroyIcon(anim->icons[i]);
            anim->icons[i] = NULL;
        }
    }
    
    anim->count = 0;
    anim->isAnimated = FALSE;
    anim->sourceType = ANIM_SOURCE_UNKNOWN;
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
static void BuildAnimationPath(const char* name, char* path, size_t size) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    
    size_t len = strlen(base);
    if (len > 0 && (base[len-1] == '/' || base[len-1] == '\\')) {
        snprintf(path, size, "%s%s", base, name);
    } else {
        snprintf(path, size, "%s\\%s", base, name);
    }
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

/**
 * @brief Load icons from folder with natural sorting
 */
BOOL LoadIconsFromFolder(const char* utf8FolderPath, HICON* icons, 
                         int* count, int maxCount) {
    if (!utf8FolderPath || !icons || !count || maxCount <= 0) return FALSE;
    
    *count = 0;
    
    wchar_t wFolder[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8FolderPath, -1, wFolder, MAX_PATH);
    
    AnimFile* files = (AnimFile*)malloc(sizeof(AnimFile) * maxCount);
    if (!files) return FALSE;
    
    int fileCount = 0;
    
    /* Scan for all supported image formats */
    const wchar_t* patterns[] = {
        L"\\*.ico", L"\\*.png", L"\\*.bmp", L"\\*.jpg", 
        L"\\*.jpeg", L"\\*.webp", L"\\*.tif", L"\\*.tiff"
    };
    
    for (int p = 0; p < sizeof(patterns)/sizeof(patterns[0]); ++p) {
        wchar_t wSearch[MAX_PATH] = {0};
        _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s%s", wFolder, patterns[p]);
        
        WIN32_FIND_DATAW ffd;
        HANDLE hFind = FindFirstFileW(wSearch, &ffd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                if (fileCount >= maxCount) break;
                
                wchar_t* dot = wcsrchr(ffd.cFileName, L'.');
                if (!dot) continue;
                
                size_t nameLen = (size_t)(dot - ffd.cFileName);
                if (nameLen == 0 || nameLen >= MAX_PATH) continue;
                
                /* Extract numeric component for sorting */
                int hasNum = 0, numVal = 0;
                for (size_t i = 0; i < nameLen; ++i) {
                    if (iswdigit(ffd.cFileName[i])) {
                        hasNum = 1;
                        numVal = 0;
                        while (i < nameLen && iswdigit(ffd.cFileName[i])) {
                            numVal = numVal * 10 + (ffd.cFileName[i] - L'0');
                            i++;
                        }
                        break;
                    }
                }
                
                files[fileCount].hasNum = hasNum;
                files[fileCount].num = numVal;
                wcsncpy(files[fileCount].name, ffd.cFileName, nameLen);
                files[fileCount].name[nameLen] = L'\0';
                _snwprintf_s(files[fileCount].path, MAX_PATH, _TRUNCATE, L"%s\\%s", wFolder, ffd.cFileName);
                fileCount++;
                
            } while (FindNextFileW(hFind, &ffd) && fileCount < maxCount);
            FindClose(hFind);
        }
    }
    
    if (fileCount == 0) {
        free(files);
        return FALSE;
    }
    
    /* Sort by numeric value then name */
    qsort(files, (size_t)fileCount, sizeof(AnimFile), CompareAnimFile);
    
    /* Load icons */
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    
    for (int i = 0; i < fileCount; ++i) {
        HICON hIcon = NULL;
        wchar_t* ext = wcsrchr(files[i].path, L'.');
        
        if (ext && _wcsicmp(ext, L".ico") == 0) {
            hIcon = (HICON)LoadImageW(NULL, files[i].path, IMAGE_ICON, 0, 0, 
                                      LR_LOADFROMFILE | LR_DEFAULTSIZE);
        } else {
            /* Use decoder for other formats */
            char utf8Path[MAX_PATH] = {0};
            WideCharToMultiByte(CP_UTF8, 0, files[i].path, -1, utf8Path, MAX_PATH, NULL, NULL);
            hIcon = DecodeStaticImage(utf8Path, cx, cy);
        }
        
        if (hIcon) {
            icons[(*count)++] = hIcon;
        }
    }
    
    free(files);
    return (*count > 0);
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
    BuildAnimationPath(name, fullPath, sizeof(fullPath));
    
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, fullPath, -1, wPath, MAX_PATH);
    
    DWORD attrs = GetFileAttributesW(wPath);
    if (attrs == INVALID_FILE_ATTRIBUTES) return FALSE;
    
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        /* Check if folder contains valid image files */
        wchar_t wSearch[MAX_PATH] = {0};
        _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", wPath);
        
        WIN32_FIND_DATAW ffd;
        HANDLE hFind = FindFirstFileW(wSearch, &ffd);
        if (hFind == INVALID_HANDLE_VALUE) return FALSE;
        
        BOOL hasImages = FALSE;
        do {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            
            wchar_t* ext = wcsrchr(ffd.cFileName, L'.');
            if (ext && (_wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 ||
                       _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 ||
                       _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".gif") == 0 ||
                       _wcsicmp(ext, L".webp") == 0 || _wcsicmp(ext, L".tif") == 0 ||
                       _wcsicmp(ext, L".tiff") == 0)) {
                hasImages = TRUE;
                break;
            }
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
        
        return hasImages;
    }
    
    /* File exists */
    return TRUE;
}

/**
 * @brief Load animation by name
 */
BOOL LoadAnimationByName(const char* name, LoadedAnimation* anim,
                         MemoryPool* pool, int iconWidth, int iconHeight) {
    if (!name || !anim) return FALSE;
    
    LoadedAnimation_Init(anim);
    
    AnimationSourceType type = DetectAnimationSourceType(name);
    anim->sourceType = type;
    
    if (type == ANIM_SOURCE_LOGO) {
        HICON hIcon = LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_CATIME));
        if (hIcon) {
            anim->icons[0] = hIcon;
            anim->count = 1;
            anim->isAnimated = FALSE;
            return TRUE;
        }
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_PERCENT) {
        /* Percent icons are generated dynamically, not pre-loaded */
        anim->count = 0;
        anim->isAnimated = FALSE;
        return TRUE;
    }
    
    if (type == ANIM_SOURCE_CAPSLOCK) {
        /* Caps Lock icons are generated dynamically, not pre-loaded */
        anim->count = 0;
        anim->isAnimated = FALSE;
        return TRUE;
    }
    
    /* Handle __none__ (transparent icon) - no frames needed */
    if (_stricmp(name, "__none__") == 0) {
        anim->count = 0;
        anim->isAnimated = FALSE;
        return TRUE;
    }
    
    char fullPath[MAX_PATH] = {0};
    BuildAnimationPath(name, fullPath, sizeof(fullPath));
    
    if (type == ANIM_SOURCE_GIF || type == ANIM_SOURCE_WEBP) {
        DecodedAnimation decoded;
        DecodedAnimation_Init(&decoded);
        
        if (DecodeAnimatedImage(fullPath, &decoded, pool, iconWidth, iconHeight)) {
            /* Copy to LoadedAnimation */
            int copyCount = decoded.count < MAX_ANIMATION_FRAMES ? decoded.count : MAX_ANIMATION_FRAMES;
            for (int i = 0; i < copyCount; i++) {
                anim->icons[i] = decoded.icons[i];
                anim->delays[i] = decoded.delays[i];
                decoded.icons[i] = NULL;
            }
            anim->count = copyCount;
            anim->isAnimated = (copyCount > 1);
            
            DecodedAnimation_Free(&decoded);
            return TRUE;
        }
        
        DecodedAnimation_Free(&decoded);
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_STATIC) {
        HICON hIcon = DecodeStaticImage(fullPath, iconWidth, iconHeight);
        if (hIcon) {
            anim->icons[0] = hIcon;
            anim->count = 1;
            anim->isAnimated = FALSE;
            return TRUE;
        }
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_FOLDER) {
        if (LoadIconsFromFolder(fullPath, anim->icons, &anim->count, MAX_ANIMATION_FRAMES)) {
            anim->isAnimated = (anim->count > 1);
            return TRUE;
        }
        return FALSE;
    }
    
    return FALSE;
}

/**
 * @brief Load animation from absolute path
 */
BOOL LoadAnimationFromPath(const char* path, LoadedAnimation* anim,
                          MemoryPool* pool, int iconWidth, int iconHeight) {
    if (!path || !anim) return FALSE;
    
    LoadedAnimation_Init(anim);
    
    AnimationSourceType type = DetectAnimationSourceType(path);
    anim->sourceType = type;
    
    /* Handle special types if path happens to be one of them (unlikely but safe) */
    if (type == ANIM_SOURCE_LOGO) {
        HICON hIcon = LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_CATIME));
        if (hIcon) {
            anim->icons[0] = hIcon;
            anim->count = 1;
            anim->isAnimated = FALSE;
            return TRUE;
        }
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_PERCENT) {
        anim->count = 0;
        anim->isAnimated = FALSE;
        return TRUE;
    }
    
    if (type == ANIM_SOURCE_GIF || type == ANIM_SOURCE_WEBP) {
        DecodedAnimation decoded;
        DecodedAnimation_Init(&decoded);
        
        if (DecodeAnimatedImage(path, &decoded, pool, iconWidth, iconHeight)) {
            int copyCount = decoded.count < MAX_ANIMATION_FRAMES ? decoded.count : MAX_ANIMATION_FRAMES;
            for (int i = 0; i < copyCount; i++) {
                anim->icons[i] = decoded.icons[i];
                anim->delays[i] = decoded.delays[i];
                decoded.icons[i] = NULL;
            }
            anim->count = copyCount;
            anim->isAnimated = (copyCount > 1);
            
            DecodedAnimation_Free(&decoded);
            return TRUE;
        }
        
        DecodedAnimation_Free(&decoded);
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_STATIC) {
        HICON hIcon = DecodeStaticImage(path, iconWidth, iconHeight);
        if (hIcon) {
            anim->icons[0] = hIcon;
            anim->count = 1;
            anim->isAnimated = FALSE;
            return TRUE;
        }
        return FALSE;
    }
    
    if (type == ANIM_SOURCE_FOLDER) {
        if (LoadIconsFromFolder(path, anim->icons, &anim->count, MAX_ANIMATION_FRAMES)) {
            anim->isAnimated = (anim->count > 1);
            return TRUE;
        }
        return FALSE;
    }
    
    return FALSE;
}

