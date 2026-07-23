/**
 * @file tray_animation_loader_runtime.c
 * @brief Loaded-animation storage and icon ownership management.
 */

#include "tray_animation_loader_internal.h"

static int ClampIconDimension(int value) {
    if (value <= 0) return ANIMATION_LOADER_FALLBACK_ICON_SIZE;
    if (value > ANIMATION_LOADER_MAX_ICON_SIZE) {
        return ANIMATION_LOADER_MAX_ICON_SIZE;
    }
    return value;
}

void AnimationLoader_NormalizeIconSize(int* iconWidth, int* iconHeight) {
    if (iconWidth) *iconWidth = ClampIconDimension(*iconWidth);
    if (iconHeight) *iconHeight = ClampIconDimension(*iconHeight);
}

void AnimationLoader_GetSystemIconSize(int* iconWidth, int* iconHeight) {
    int width = GetSystemMetrics(SM_CXSMICON);
    int height = GetSystemMetrics(SM_CYSMICON);
    AnimationLoader_NormalizeIconSize(&width, &height);
    if (iconWidth) *iconWidth = width;
    if (iconHeight) *iconHeight = height;
}

BOOL AnimationLoader_IsCanceled(HANDLE cancelEvent) {
    return cancelEvent &&
           WaitForSingleObject(cancelEvent, 0) == WAIT_OBJECT_0;
}

void LoadedAnimation_Init(LoadedAnimation* anim) {
    if (!anim) return;
    ZeroMemory(anim, sizeof(*anim));
    anim->sourceType = ANIM_SOURCE_UNKNOWN;
}

BOOL AnimationLoader_Reserve(LoadedAnimation* anim, int capacity) {
    if (!anim || capacity <= 0) return FALSE;
    if (capacity <= anim->capacity) return TRUE;
    HICON* icons = (HICON*)calloc((size_t)capacity, sizeof(*icons));
    BOOL* ownsIcons = (BOOL*)calloc((size_t)capacity, sizeof(*ownsIcons));
    UINT* delays = (UINT*)calloc((size_t)capacity, sizeof(*delays));
    if (!icons || !ownsIcons || !delays) {
        free(icons);
        free(ownsIcons);
        free(delays);
        return FALSE;
    }

    if (anim->count > 0) {
        memcpy(icons, anim->icons, (size_t)anim->count * sizeof(*icons));
        if (anim->ownsIcons) {
            memcpy(ownsIcons, anim->ownsIcons,
                   (size_t)anim->count * sizeof(*ownsIcons));
        } else {
            for (int i = 0; i < anim->count; ++i) ownsIcons[i] = TRUE;
        }
        memcpy(delays, anim->delays, (size_t)anim->count * sizeof(*delays));
    }
    free(anim->icons);
    free(anim->ownsIcons);
    free(anim->delays);
    anim->icons = icons;
    anim->ownsIcons = ownsIcons;
    anim->delays = delays;
    anim->capacity = capacity;
    return TRUE;
}

void LoadedAnimation_Free(LoadedAnimation* anim) {
    if (!anim) return;
    for (int i = 0; i < anim->count; ++i) {
        if (anim->icons[i] && (!anim->ownsIcons || anim->ownsIcons[i])) {
            DestroyIcon(anim->icons[i]);
        }
    }
    free(anim->icons);
    free(anim->ownsIcons);
    free(anim->delays);
    LoadedAnimation_Init(anim);
}

void AnimationLoader_MoveToOutput(LoadedAnimation* output,
                                  LoadedAnimation* loaded) {
    if (!output || !loaded || output == loaded) return;
    LoadedAnimation_Free(output);
    *output = *loaded;
    LoadedAnimation_Init(loaded);
}

BOOL AnimationLoader_MoveDecoded(DecodedAnimation* decoded,
                                 LoadedAnimation* loaded) {
    if (!decoded || !loaded || decoded->count <= 0 ||
        !AnimationLoader_Reserve(loaded, decoded->count)) {
        return FALSE;
    }
    for (int i = 0; i < decoded->count; ++i) {
        loaded->icons[i] = decoded->icons[i];
        loaded->ownsIcons[i] = TRUE;
        loaded->delays[i] = decoded->delays[i];
        decoded->icons[i] = NULL;
    }
    loaded->count = decoded->count;
    loaded->isAnimated = loaded->count > 1;
    return TRUE;
}

BOOL AnimationLoader_SetSingleIcon(LoadedAnimation* anim, HICON icon,
                                   BOOL ownsIcon) {
    if (!anim || !icon) return FALSE;
    if (!AnimationLoader_Reserve(anim, 1)) {
        if (ownsIcon) DestroyIcon(icon);
        return FALSE;
    }
    anim->icons[0] = icon;
    anim->ownsIcons[0] = ownsIcon;
    anim->count = 1;
    anim->isAnimated = FALSE;
    return TRUE;
}
