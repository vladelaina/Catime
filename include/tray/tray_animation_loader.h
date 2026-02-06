/**
 * @file tray_animation_loader.h
 * @brief Animation resource loading from various sources
 * 
 * Supports: folders (icon sequences), GIF, WebP, static images, builtin icons.
 * Natural sorting for folder frames. Memory pool for efficient loading.
 */

#ifndef TRAY_ANIMATION_LOADER_H
#define TRAY_ANIMATION_LOADER_H

#include <windows.h>
#include "tray_animation_decoder.h"
#include "utils/memory_pool.h"

/* Maximum frames per animation (64 provides smooth motion at 150ms interval = 9.6s loop) */
#define MAX_ANIMATION_FRAMES 2048

/**
 * @brief Animation source types
 */
typedef enum {
    ANIM_SOURCE_UNKNOWN = 0,
    ANIM_SOURCE_LOGO,
    ANIM_SOURCE_PERCENT,
    ANIM_SOURCE_CAPSLOCK,
    ANIM_SOURCE_GIF,
    ANIM_SOURCE_WEBP,
    ANIM_SOURCE_STATIC,
    ANIM_SOURCE_FOLDER
} AnimationSourceType;

/**
 * @brief Loaded animation frames
 */
typedef struct {
    HICON icons[MAX_ANIMATION_FRAMES];
    int count;
    UINT delays[MAX_ANIMATION_FRAMES];
    BOOL isAnimated;
    AnimationSourceType sourceType;
} LoadedAnimation;

/**
 * @brief Callback to get percentage value (0-100) for percent-based icons
 * @return Value 0-100, or -1 if invalid/error
 */
typedef int (*AnimValueCallback)(void);

/**
 * @brief Definition of a builtin animation type
 */
typedef struct {
    const char* name;           /**< Internal name (e.g. "__cpu__") */
    UINT menuId;                /**< Resource ID for menu */
    const wchar_t* menuLabel;   /**< Display label in menu */
    AnimationSourceType type;   /**< Animation source type */
    AnimValueCallback getValue; /**< Value getter (optional, for PERCENT type) */
} BuiltinAnimDef;

/**
 * @brief Initialize loaded animation structure
 */
void LoadedAnimation_Init(LoadedAnimation* anim);

/**
 * @brief Free all resources
 */
void LoadedAnimation_Free(LoadedAnimation* anim);

/**
 * @brief Detect animation source type from name
 * @param name Animation identifier (e.g., "__logo__", "cat.gif", "spinner")
 * @return Source type
 */
AnimationSourceType DetectAnimationSourceType(const char* name);

/**
 * @brief Get builtin animation definition by name
 * @return Pointer to definition or NULL if not found
 */
const BuiltinAnimDef* GetBuiltinAnimDef(const char* name);

/**
 * @brief Get builtin animation definition by menu ID
 * @return Pointer to definition or NULL if not found
 */
const BuiltinAnimDef* GetBuiltinAnimDefById(UINT id);

/**
 * @brief Get all builtin animation definitions
 * @param count Output count
 * @return Pointer to array
 */
const BuiltinAnimDef* GetBuiltinAnims(int* count);

/**
 * @brief Load animation frames by name
 * @param name Animation identifier
 * @param anim Output structure
 * @param pool Memory pool for temporary buffers (optional)
 * @param iconWidth Target icon width
 * @param iconHeight Target icon height
 * @return TRUE on success, FALSE on failure
 * 
 * @details
 * Resolves name to full path, detects type, and loads accordingly.
 * Caller must call LoadedAnimation_Free() when done.
 */
BOOL LoadAnimationByName(const char* name, LoadedAnimation* anim,
                         MemoryPool* pool, int iconWidth, int iconHeight);

/**
 * @brief Load icons from folder (naturally sorted)
 * @param utf8FolderPath Full path to folder
 * @param icons Output array
 * @param count Output count
 * @param maxCount Maximum icons to load
 * @return TRUE if any icons loaded
 */
BOOL LoadIconsFromFolder(const char* utf8FolderPath, HICON* icons, 
                         int* count, int maxCount);

/**
 * @brief Check if file/folder exists and is valid animation source
 * @param name Animation name
 * @return TRUE if valid and exists
 */
BOOL IsValidAnimationSource(const char* name);

/**
 * @brief Check if name is a builtin animation (not custom file/folder)
 * @param name Animation identifier
 * @return TRUE for __logo__, __cpu__, __mem__, __battery__, __none__
 * 
 * @note Add new builtin types here to avoid scattered checks
 */
BOOL IsBuiltinAnimationName(const char* name);

#endif /* TRAY_ANIMATION_LOADER_H */

