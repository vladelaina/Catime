#include "font_manager_internal.h"

FontResource fontResources[] = {
    {IDR_FONT_RECMONO, "RecMonoCasual Nerd Font Mono Essence.ttf"},
    {IDR_FONT_DEPARTURE, "DepartureMono Nerd Font Propo Essence.ttf"},
    {IDR_FONT_TERMINESS, "Terminess Nerd Font Propo Essence.ttf"},
    {IDR_FONT_JACQUARD, "Jacquard 12 Essence.ttf"},
    {IDR_FONT_JACQUARDA, "Jacquarda Bastarda 9 Essence.ttf"},
    {IDR_FONT_PIXELIFY, "Pixelify Sans Medium Essence.ttf"},
    {IDR_FONT_RUBIK_BURNED, "Rubik Burned Essence.ttf"},
    {IDR_FONT_RUBIK_GLITCH, "Rubik Glitch Essence.ttf"},
    {IDR_FONT_RUBIK_MARKER_HATCH, "Rubik Marker Hatch Essence.ttf"},
    {IDR_FONT_RUBIK_PUDDLES, "Rubik Puddles Essence.ttf"},
    {IDR_FONT_WALLPOET, "Wallpoet Essence.ttf"},
    {IDR_FONT_PROFONT, "ProFont IIx Nerd Font Essence.ttf"},
    {IDR_FONT_DADDYTIME, "DaddyTimeMono Nerd Font Propo Essence.ttf"},
};

const int FONT_RESOURCES_COUNT =
    (int)(sizeof(fontResources) / sizeof(FontResource));

BOOL FontManager_WriteDataToFile(
    const void* fontData, size_t fontLength, const char* outputPath) {
    if (!fontData || fontLength == 0 || fontLength > MAXDWORD || !outputPath) {
        return FALSE;
    }

    wchar_t wideOutputPath[MAX_PATH];
    wchar_t wideOutputDir[MAX_PATH];
    if (!Utf8ToWide(outputPath, wideOutputPath, MAX_PATH) ||
        !ExtractDirectoryW(wideOutputPath, wideOutputDir, MAX_PATH)) {
        return FALSE;
    }

    wchar_t tempPath[MAX_PATH];
    if (GetTempFileNameW(wideOutputDir, L"ctf", 0, tempPath) == 0) {
        return FALSE;
    }

    HANDLE file = CreateFileW(
        tempPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DeleteFileW(tempPath);
        return FALSE;
    }

    DWORD bytesWritten = 0;
    DWORD writeLength = (DWORD)fontLength;
    BOOL result = WriteFile(
        file, fontData, writeLength, &bytesWritten, NULL) &&
                  bytesWritten == writeLength;
    if (result && !FlushFileBuffers(file)) {
        result = FALSE;
    }
    if (!CloseHandle(file)) {
        result = FALSE;
    }
    if (result) {
        result = MoveFileExW(
            tempPath, wideOutputPath,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    }
    if (!result) {
        DeleteFileW(tempPath);
    }
    return result;
}

BOOL ExtractFontResourceToFile(
    HINSTANCE hInstance, int resourceId, const char* outputPath) {
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
    CompressedResourceGroup* group = NULL;
    if (!CompressedResource_LoadGroup(
            hInstance, COMPRESSED_RESOURCE_GROUP_FONTS, &group)) {
        return FALSE;
    }
    const BYTE* fontData = NULL;
    size_t fontLength = 0;
    BOOL result =
        CompressedResource_GetMember(
            group, (UINT)resourceId, &fontData, &fontLength, NULL) &&
        FontManager_WriteDataToFile(fontData, fontLength, outputPath);
    CompressedResource_FreeGroup(group);
    return result;
#else
    if (!outputPath) {
        return FALSE;
    }
    HRSRC resource = FindResourceW(
        hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
    if (!resource) {
        return FALSE;
    }
    HGLOBAL memory = LoadResource(hInstance, resource);
    const void* fontData = memory ? LockResource(memory) : NULL;
    DWORD fontLength = memory ? SizeofResource(hInstance, resource) : 0;
    return fontData && fontLength > 0 &&
           FontManager_WriteDataToFile(
               fontData, (size_t)fontLength, outputPath);
#endif
}

BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance) {
    wchar_t wideFontsFolder[MAX_PATH] = {0};
    if (!GetFontsFolderW(wideFontsFolder, MAX_PATH, TRUE)) {
        return FALSE;
    }
    char fontsFolder[MAX_PATH];
    if (!WideToUtf8(wideFontsFolder, fontsFolder, MAX_PATH)) {
        return FALSE;
    }

#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
    CompressedResourceGroup* group = NULL;
    if (!CompressedResource_LoadGroup(
            hInstance, COMPRESSED_RESOURCE_GROUP_FONTS, &group)) {
        return FALSE;
    }
#endif

    BOOL allExtracted = TRUE;
    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
        char outputPath[MAX_PATH];
        int pathLength = snprintf(
            outputPath, MAX_PATH, "%s\\%s", fontsFolder,
            fontResources[i].fontName);
        if (pathLength < 0 || pathLength >= MAX_PATH) {
            LOG_WARNING("Font output path too long: %s",
                        fontResources[i].fontName);
            allExtracted = FALSE;
            continue;
        }
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
        const BYTE* fontData = NULL;
        size_t fontLength = 0;
        BOOL extracted =
            CompressedResource_GetMember(
                group, (UINT)fontResources[i].resourceId,
                &fontData, &fontLength, NULL) &&
            FontManager_WriteDataToFile(fontData, fontLength, outputPath);
#else
        BOOL extracted = ExtractFontResourceToFile(
            hInstance, fontResources[i].resourceId, outputPath);
#endif
        if (!extracted) {
            LOG_WARNING("Failed to extract embedded font: %s",
                        fontResources[i].fontName);
            allExtracted = FALSE;
        }
    }

#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
    CompressedResource_FreeGroup(group);
#endif
    return allExtracted;
}
