/**
 * @file tray_animation_loader_internal.h
 * @brief Shared animation loader limits and ownership helpers.
 */

#ifndef CATIME_TRAY_ANIMATION_LOADER_INTERNAL_H
#define CATIME_TRAY_ANIMATION_LOADER_INTERNAL_H

#include "tray/tray_animation_loader.h"
#include "config.h"
#include "log.h"
#include "system_monitor.h"
#include "utils/natural_sort.h"
#include "../resource/resource.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANIMATION_LOADER_MAX_FOLDER_FRAMES 512
#define ANIMATION_LOADER_MAX_SCAN_ENTRIES 4096
#define ANIMATION_LOADER_MAX_FILE_BYTES (128ull * 1024ull * 1024ull)
#define ANIMATION_LOADER_FALLBACK_ICON_SIZE 16
#define ANIMATION_LOADER_MAX_ICON_SIZE 256

typedef struct {
    int hasNumber;
    int number;
    wchar_t name[MAX_PATH];
    wchar_t path[MAX_PATH];
} AnimationFolderFile;

BOOL AnimationLoader_IsCanceled(HANDLE cancelEvent);
void AnimationLoader_NormalizeIconSize(int* iconWidth, int* iconHeight);
void AnimationLoader_GetSystemIconSize(int* iconWidth, int* iconHeight);
BOOL AnimationLoader_IsFileSizeAllowed(const char* path);
BOOL AnimationLoader_IsFindDataSizeAllowed(const WIN32_FIND_DATAW* data);
BOOL AnimationLoader_Reserve(LoadedAnimation* anim, int capacity);
BOOL AnimationLoader_SetSingleIcon(LoadedAnimation* anim, HICON icon,
                                   BOOL ownsIcon);
BOOL AnimationLoader_MoveDecoded(DecodedAnimation* decoded,
                                 LoadedAnimation* loaded);
void AnimationLoader_MoveToOutput(LoadedAnimation* output,
                                  LoadedAnimation* loaded);
BOOL AnimationLoader_BuildPath(const char* name, char* path, size_t size);
BOOL AnimationLoader_IsSupportedExtension(const wchar_t* extension);
BOOL AnimationLoader_IsSupportedFolderExtension(const wchar_t* extension);

#endif /* CATIME_TRAY_ANIMATION_LOADER_INTERNAL_H */
