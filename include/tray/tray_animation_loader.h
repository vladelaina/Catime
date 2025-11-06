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

#define MAX_ANIMATION_FRAMES 64

/**
 * @brief Animation source types
 */
typedef enum {
    ANIM_SOURCE_UNKNOWN = 0,
    ANIM_SOURCE_LOGO,       /**< Builtin resource */
    ANIM_SOURCE_PERCENT,    /**< Dynamic CPU/Memory percent */
    ANIM_SOURCE_GIF,        /**< Single GIF file */
    ANIM_SOURCE_WEBP,       /**< Single WebP file */
    ANIM_SOURCE_STATIC,     /**< Static image (ICO/PNG/BMP/JPG) */
    ANIM_SOURCE_FOLDER      /**< Folder with sequenced icons */
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

#endif /* TRAY_ANIMATION_LOADER_H */

