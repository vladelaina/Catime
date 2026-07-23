#include "font_manager_internal.h"

static void ClearFontPreviewState(void) {
    IS_PREVIEWING = FALSE;
    PREVIEW_FONT_NAME[0] = '\0';
    PREVIEW_INTERNAL_NAME[0] = '\0';
}

BOOL PreviewFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) {
        return FALSE;
    }

    BOOL hadPreview = IS_PREVIEWING;
    char pendingFontName[MAX_PATH] = {0};
    char loadedInternalName[MAX_PATH] = {0};
    if (!FontManager_CopyStringExact(
            fontName, pendingFontName, sizeof(pendingFontName))) {
        LOG_WARNING("Font preview name too long, ignoring preview: %s", fontName);
        if (hadPreview) {
            CancelFontPreview();
        } else {
            ClearFontPreviewState();
        }
        return FALSE;
    }
    if (!LoadFontByNameAndGetRealName(
            hInstance, pendingFontName,
            loadedInternalName, sizeof(loadedInternalName))) {
        if (hadPreview) {
            CancelFontPreview();
        } else {
            ClearFontPreviewState();
        }
        return FALSE;
    }

    FontManager_CopyStringExact(
        pendingFontName, PREVIEW_FONT_NAME, sizeof(PREVIEW_FONT_NAME));
    FontManager_CopyStringExact(
        loadedInternalName, PREVIEW_INTERNAL_NAME,
        sizeof(PREVIEW_INTERNAL_NAME));
    IS_PREVIEWING = TRUE;
    return TRUE;
}

void CancelFontPreview(void) {
    ClearFontPreviewState();
    HINSTANCE hInstance = GetModuleHandle(NULL);
    if (IsFontsFolderPath(FONT_RUNTIME_FILE_NAME)) {
        const char* relativePath = ExtractRelativePath(FONT_RUNTIME_FILE_NAME);
        if (relativePath) {
            LoadFontByNameAndGetRealName(
                hInstance, relativePath,
                FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        }
    } else if (FONT_RUNTIME_FILE_NAME[0] != '\0') {
        LoadFontByNameAndGetRealName(
            hInstance, FONT_RUNTIME_FILE_NAME,
            FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
    }
}

void ApplyFontPreview(void) {
    if (!IS_PREVIEWING || strlen(PREVIEW_FONT_NAME) == 0) {
        return;
    }

    char previousFontName[MAX_PATH] = {0};
    char previousRuntimeName[MAX_PATH] = {0};
    char previousInternalName[MAX_PATH] = {0};
    char committedFontName[MAX_PATH] = {0};
    char committedInternalName[MAX_PATH] = {0};
    FontManager_CopyStringExact(
        FONT_FILE_NAME, previousFontName, sizeof(previousFontName));
    FontManager_CopyStringExact(
        FONT_RUNTIME_FILE_NAME, previousRuntimeName,
        sizeof(previousRuntimeName));
    FontManager_CopyStringExact(
        FONT_INTERNAL_NAME, previousInternalName,
        sizeof(previousInternalName));
    if (!FontManager_CopyStringExact(
            PREVIEW_FONT_NAME, committedFontName,
            sizeof(committedFontName)) ||
        !FontManager_CopyStringExact(
            PREVIEW_INTERNAL_NAME, committedInternalName,
            sizeof(committedInternalName))) {
        CancelFontPreview();
        return;
    }

    FontManager_CopyStringExact(
        committedFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
    FontManager_CopyStringExact(
        committedFontName, FONT_RUNTIME_FILE_NAME,
        sizeof(FONT_RUNTIME_FILE_NAME));
    FontManager_CopyStringExact(
        committedInternalName, FONT_INTERNAL_NAME,
        sizeof(FONT_INTERNAL_NAME));
    if (!WriteConfigFont(FONT_FILE_NAME, FALSE)) {
        FontManager_CopyStringExact(
            previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        FontManager_CopyStringExact(
            previousRuntimeName, FONT_RUNTIME_FILE_NAME,
            sizeof(FONT_RUNTIME_FILE_NAME));
        FontManager_CopyStringExact(
            previousInternalName, FONT_INTERNAL_NAME,
            sizeof(FONT_INTERNAL_NAME));
        if (!FontManager_ReloadFromConfigName(
                GetModuleHandle(NULL), previousRuntimeName,
                FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
            LOG_WARNING(
                "Failed to restore previous preview font after config write failure: %s",
                previousFontName);
            FontManager_CopyStringExact(
                previousInternalName, FONT_INTERNAL_NAME,
                sizeof(FONT_INTERNAL_NAME));
        }
        ClearFontPreviewState();
        return;
    }
    ClearFontPreviewState();
}
