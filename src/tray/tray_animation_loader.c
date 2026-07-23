/**
 * @file tray_animation_loader.c
 * @brief Animation source dispatch for names and absolute paths.
 */

#include "tray_animation_loader_internal.h"

static BOOL LoadDecodedAnimation(const char* path, AnimationSourceType type,
                                 LoadedAnimation* loaded, MemoryPool* pool,
                                 int iconWidth, int iconHeight,
                                 HANDLE cancelEvent) {
    DecodedAnimation decoded;
    DecodedAnimation_Init(&decoded);
    BOOL decodedOk = type == ANIM_SOURCE_ANI
        ? DecodeAniCursorWithCancel(path, &decoded,
                                    iconWidth, iconHeight, cancelEvent)
        : DecodeAnimatedImageWithCancel(path, &decoded, pool,
                                        iconWidth, iconHeight, cancelEvent);
    BOOL moved = decodedOk &&
                 AnimationLoader_MoveDecoded(&decoded, loaded);
    DecodedAnimation_Free(&decoded);
    return moved;
}

static BOOL LoadStaticAnimation(const char* path, LoadedAnimation* loaded,
                                int iconWidth, int iconHeight,
                                HANDLE cancelEvent) {
    if (AnimationLoader_IsCanceled(cancelEvent)) return FALSE;
    HICON icon = DecodeStaticImage(path, iconWidth, iconHeight);
    if (AnimationLoader_IsCanceled(cancelEvent)) {
        if (icon) DestroyIcon(icon);
        return FALSE;
    }
    return AnimationLoader_SetSingleIcon(loaded, icon, TRUE);
}

static BOOL LoadBuiltinAnimation(AnimationSourceType type,
                                 LoadedAnimation* loaded) {
    if (type == ANIM_SOURCE_LOGO) {
        HICON icon = LoadIconW(GetModuleHandle(NULL),
                               MAKEINTRESOURCEW(IDI_CATIME));
        return AnimationLoader_SetSingleIcon(loaded, icon, FALSE);
    }
    if (type == ANIM_SOURCE_PERCENT || type == ANIM_SOURCE_CAPSLOCK) {
        loaded->count = 0;
        loaded->isAnimated = FALSE;
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadResolvedSource(const char* source, AnimationSourceType type,
                               LoadedAnimation* output, MemoryPool* pool,
                               int iconWidth, int iconHeight,
                               HANDLE cancelEvent) {
    LoadedAnimation loaded;
    LoadedAnimation_Init(&loaded);
    loaded.sourceType = type;

    BOOL result = FALSE;
    if (LoadBuiltinAnimation(type, &loaded)) {
        result = TRUE;
    } else if (source && _stricmp(source, "__none__") == 0) {
        result = TRUE;
    } else if (type == ANIM_SOURCE_GIF || type == ANIM_SOURCE_WEBP ||
               type == ANIM_SOURCE_ANI) {
        result = AnimationLoader_IsFileSizeAllowed(source) &&
                 LoadDecodedAnimation(source, type, &loaded, pool,
                                      iconWidth, iconHeight, cancelEvent);
    } else if (type == ANIM_SOURCE_STATIC) {
        result = AnimationLoader_IsFileSizeAllowed(source) &&
                 LoadStaticAnimation(source, &loaded, iconWidth,
                                     iconHeight, cancelEvent);
    } else if (type == ANIM_SOURCE_FOLDER) {
        result = LoadIconsFromFolderWithCancel(source, &loaded, cancelEvent);
        loaded.isAnimated = loaded.count > 1;
    }

    if (result) AnimationLoader_MoveToOutput(output, &loaded);
    LoadedAnimation_Free(&loaded);
    return result;
}

BOOL LoadAnimationByName(const char* name, LoadedAnimation* anim,
                         MemoryPool* pool, int iconWidth, int iconHeight) {
    return LoadAnimationByNameWithCancel(
        name, anim, pool, iconWidth, iconHeight, NULL);
}

BOOL LoadAnimationByNameWithCancel(const char* name, LoadedAnimation* anim,
                                   MemoryPool* pool, int iconWidth,
                                   int iconHeight, HANDLE cancelEvent) {
    if (!name || !anim || AnimationLoader_IsCanceled(cancelEvent)) return FALSE;
    AnimationLoader_NormalizeIconSize(&iconWidth, &iconHeight);
    AnimationSourceType type = DetectAnimationSourceType(name);

    const char* source = name;
    char fullPath[MAX_PATH] = {0};
    if (!IsBuiltinAnimationName(name) &&
        _stricmp(name, "__none__") != 0) {
        if (!AnimationLoader_BuildPath(name, fullPath, sizeof(fullPath))) {
            return FALSE;
        }
        source = fullPath;
    }
    if (AnimationLoader_IsCanceled(cancelEvent)) return FALSE;
    return LoadResolvedSource(source, type, anim, pool,
                              iconWidth, iconHeight, cancelEvent);
}

BOOL LoadAnimationFromPath(const char* path, LoadedAnimation* anim,
                           MemoryPool* pool, int iconWidth, int iconHeight) {
    return LoadAnimationFromPathWithCancel(
        path, anim, pool, iconWidth, iconHeight, NULL);
}

BOOL LoadAnimationFromPathWithCancel(const char* path, LoadedAnimation* anim,
                                     MemoryPool* pool, int iconWidth,
                                     int iconHeight, HANDLE cancelEvent) {
    if (!path || !anim || AnimationLoader_IsCanceled(cancelEvent)) return FALSE;
    AnimationLoader_NormalizeIconSize(&iconWidth, &iconHeight);
    return LoadResolvedSource(path, DetectAnimationSourceType(path),
                              anim, pool, iconWidth, iconHeight, cancelEvent);
}
