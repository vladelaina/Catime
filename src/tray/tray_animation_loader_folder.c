/**
 * @file tray_animation_loader_folder.c
 * @brief Bounded natural-order loading of image sequences from folders.
 */

#include "tray_animation_loader_internal.h"
#include <wctype.h>

static int CompareFolderFiles(const void* left, const void* right) {
    const AnimationFolderFile* a = (const AnimationFolderFile*)left;
    const AnimationFolderFile* b = (const AnimationFolderFile*)right;
    if (a->hasNumber && b->hasNumber && a->number != b->number) {
        return a->number < b->number ? -1 : 1;
    }
    return NaturalCompareW(a->name, b->name);
}

static void ExtractSortNumber(AnimationFolderFile* file, size_t nameLength) {
    file->hasNumber = 0;
    file->number = 0;
    for (size_t i = 0; i < nameLength; ++i) {
        if (!iswdigit(file->name[i])) continue;
        file->hasNumber = 1;
        while (i < nameLength && iswdigit(file->name[i])) {
            int digit = file->name[i] - L'0';
            file->number = file->number <= (INT_MAX - digit) / 10
                ? file->number * 10 + digit : INT_MAX;
            ++i;
        }
        break;
    }
}

static BOOL AddFolderFile(AnimationFolderFile** files, int* count,
                          int* capacity, const wchar_t* folder,
                          const WIN32_FIND_DATAW* data) {
    const wchar_t* extension = wcsrchr(data->cFileName, L'.');
    if (!AnimationLoader_IsSupportedFolderExtension(extension)) return TRUE;
    if (!AnimationLoader_IsFindDataSizeAllowed(data)) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Skipping oversized folder animation frame: %ls (%llu bytes)",
                 data->cFileName,
                 ((ULONGLONG)data->nFileSizeHigh << 32) |
                    data->nFileSizeLow);
        return TRUE;
    }
    size_t nameLength = (size_t)(extension - data->cFileName);
    if (!nameLength || nameLength >= MAX_PATH) return TRUE;

    if (*count >= *capacity) {
        if (*capacity >= ANIMATION_LOADER_MAX_FOLDER_FRAMES) return FALSE;
        int newCapacity = *capacity * 2;
        if (newCapacity > ANIMATION_LOADER_MAX_FOLDER_FRAMES) {
            newCapacity = ANIMATION_LOADER_MAX_FOLDER_FRAMES;
        }
        AnimationFolderFile* resized = (AnimationFolderFile*)realloc(
            *files, sizeof(**files) * (size_t)newCapacity);
        if (!resized) return FALSE;
        *files = resized;
        *capacity = newCapacity;
    }

    AnimationFolderFile* file = &(*files)[*count];
    ZeroMemory(file, sizeof(*file));
    wcsncpy(file->name, data->cFileName, nameLength);
    file->name[nameLength] = L'\0';
    if (_snwprintf_s(file->path, MAX_PATH, _TRUNCATE,
                     L"%s\\%s", folder, data->cFileName) < 0) {
        return TRUE;
    }
    ExtractSortNumber(file, nameLength);
    ++*count;
    return TRUE;
}

static BOOL ScanFolderFiles(const wchar_t* folder,
                            AnimationFolderFile** files,
                            int* count, HANDLE cancelEvent) {
    wchar_t search[MAX_PATH] = {0};
    if (_snwprintf_s(search, MAX_PATH, _TRUNCATE,
                     L"%s\\*", folder) < 0) return FALSE;
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW(search, &data);
    if (find == INVALID_HANDLE_VALUE) return TRUE;

    int capacity = 64;
    int scanned = 0;
    BOOL result = TRUE;
    do {
        if (AnimationLoader_IsCanceled(cancelEvent)) {
            result = FALSE;
            break;
        }
        if (++scanned > ANIMATION_LOADER_MAX_SCAN_ENTRIES) {
            WriteLog(LOG_LEVEL_WARNING,
                     "Folder animation scan limit reached (%d), ignoring remaining files",
                     ANIMATION_LOADER_MAX_SCAN_ENTRIES);
            break;
        }
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!AddFolderFile(files, count, &capacity, folder, &data)) {
            if (*count >= ANIMATION_LOADER_MAX_FOLDER_FRAMES) {
                WriteLog(LOG_LEVEL_WARNING,
                         "Folder animation frame limit reached (%d), ignoring remaining files",
                         ANIMATION_LOADER_MAX_FOLDER_FRAMES);
                break;
            }
            result = FALSE;
            break;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return result;
}

static HICON LoadFolderFrame(IWICImagingFactory** factory,
                             BOOL* factoryAttempted, HRESULT* coInit,
                             const AnimationFolderFile* file,
                             int width, int height) {
    const wchar_t* extension = wcsrchr(file->path, L'.');
    if (extension && _wcsicmp(extension, L".ico") == 0) {
        return (HICON)LoadImageW(NULL, file->path, IMAGE_ICON,
                                 width, height, LR_LOADFROMFILE);
    }
    if (!*factory && !*factoryAttempted) {
        *factoryAttempted = TRUE;
        *coInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (FAILED(CoCreateInstance(
                &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                &IID_IWICImagingFactory, (void**)factory))) {
            *factory = NULL;
        }
    }
    return DecodeStaticImageWithFactory(
        *factory, file->path, width, height);
}

BOOL LoadIconsFromFolderWithCancel(const char* utf8FolderPath,
                                   LoadedAnimation* anim,
                                   HANDLE cancelEvent) {
    if (!utf8FolderPath || !anim ||
        AnimationLoader_IsCanceled(cancelEvent)) return FALSE;
    wchar_t folder[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8FolderPath, -1,
                            folder, MAX_PATH) <= 0) return FALSE;

    AnimationFolderFile* files = (AnimationFolderFile*)malloc(
        sizeof(*files) * 64u);
    if (!files) return FALSE;
    int count = 0;
    if (!ScanFolderFiles(folder, &files, &count, cancelEvent) || !count ||
        AnimationLoader_IsCanceled(cancelEvent)) {
        free(files);
        return FALSE;
    }
    qsort(files, (size_t)count, sizeof(*files), CompareFolderFiles);

    LoadedAnimation loaded;
    LoadedAnimation_Init(&loaded);
    loaded.sourceType = ANIM_SOURCE_FOLDER;
    if (!AnimationLoader_Reserve(&loaded, count)) {
        free(files);
        return FALSE;
    }

    int width = 0;
    int height = 0;
    AnimationLoader_GetSystemIconSize(&width, &height);
    HRESULT coInit = E_FAIL;
    IWICImagingFactory* factory = NULL;
    BOOL factoryAttempted = FALSE;
    BOOL canceled = FALSE;
    for (int i = 0; i < count; ++i) {
        if (AnimationLoader_IsCanceled(cancelEvent)) {
            canceled = TRUE;
            break;
        }
        HICON icon = LoadFolderFrame(&factory, &factoryAttempted, &coInit,
                                     &files[i], width, height);
        if (AnimationLoader_IsCanceled(cancelEvent)) {
            if (icon) DestroyIcon(icon);
            canceled = TRUE;
            break;
        }
        if (icon) {
            loaded.icons[loaded.count] = icon;
            loaded.ownsIcons[loaded.count++] = TRUE;
        }
    }
    if (factory) factory->lpVtbl->Release(factory);
    if (SUCCEEDED(coInit)) CoUninitialize();
    free(files);

    if (canceled || !loaded.count) {
        LoadedAnimation_Free(&loaded);
        return FALSE;
    }
    loaded.isAnimated = loaded.count > 1;
    AnimationLoader_MoveToOutput(anim, &loaded);
    return TRUE;
}

BOOL LoadIconsFromFolder(const char* utf8FolderPath, LoadedAnimation* anim) {
    return LoadIconsFromFolderWithCancel(utf8FolderPath, anim, NULL);
}
