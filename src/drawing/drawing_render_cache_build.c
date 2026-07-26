/**
 * @file drawing_render_cache_build.c
 * @brief Markdown cache rebuilding and font-path resolution.
 */

#include "drawing_render_internal.h"

BOOL RefreshMeasureCacheFontTags(const MarkdownFontTag* fontTags, int fontTagCount) {
    if (!fontTags || fontTagCount <= 0) {
        return TRUE;
    }

    if (!CanUseMeasureCacheForFontTags(fontTags, fontTagCount)) {
        return FALSE;
    }

    if (!BeginFontUseSTB()) {
        return FALSE;
    }

    for (int i = 0; i < fontTagCount; i++) {
        BOOL seen = FALSE;
        for (int j = 0; j < i; j++) {
            if (wcscmp(fontTags[i].fontName, fontTags[j].fontName) == 0) {
                seen = TRUE;
                break;
            }
        }
        if (!seen) {
            (void)GetCachedFontSTB(fontTags[i].fontName);
        }
    }

    EndFontUseSTB();
    return TRUE;
}

void EnsureMarkdownRenderCache(const wchar_t* text) {
    if (!text) {
        ClearMarkdownRenderCache();
        return;
    }

    if (g_markdownRenderCache.valid &&
        CachedWideTextEquals(g_markdownRenderCache.sourceText,
                             _countof(g_markdownRenderCache.sourceText),
                             g_markdownRenderCache.sourceTextLen,
                             text)) {
        return;
    }

    ClearMarkdownRenderCache();
    CopyCachedWideText(g_markdownRenderCache.sourceText,
                       _countof(g_markdownRenderCache.sourceText),
                       &g_markdownRenderCache.sourceTextLen,
                       text);

    if (!HasPotentialMarkdownSyntax(text)) {
        g_markdownRenderCache.valid = TRUE;
        return;
    }

    BOOL parsedMarkdown = ParseMarkdownLinks(
        text,
        &g_markdownRenderCache.mdText,
        &g_markdownRenderCache.links, &g_markdownRenderCache.linkCount,
        &g_markdownRenderCache.headings, &g_markdownRenderCache.headingCount,
        &g_markdownRenderCache.styles, &g_markdownRenderCache.styleCount,
        &g_markdownRenderCache.listItems, &g_markdownRenderCache.listItemCount,
        &g_markdownRenderCache.blockquotes, &g_markdownRenderCache.blockquoteCount,
        &g_markdownRenderCache.colorTags, &g_markdownRenderCache.colorTagCount,
        &g_markdownRenderCache.fontTags, &g_markdownRenderCache.fontTagCount
    );
    g_markdownRenderCache.isMarkdown = parsedMarkdown;

    if (!parsedMarkdown) {
        if (g_markdownRenderCache.links) {
            FreeMarkdownLinks(g_markdownRenderCache.links, g_markdownRenderCache.linkCount);
            g_markdownRenderCache.links = NULL;
            g_markdownRenderCache.linkCount = 0;
        }
        free(g_markdownRenderCache.headings);
        free(g_markdownRenderCache.styles);
        free(g_markdownRenderCache.listItems);
        free(g_markdownRenderCache.blockquotes);
        free(g_markdownRenderCache.colorTags);
        free(g_markdownRenderCache.fontTags);
        free(g_markdownRenderCache.mdText);
        g_markdownRenderCache.headings = NULL;
        g_markdownRenderCache.styles = NULL;
        g_markdownRenderCache.listItems = NULL;
        g_markdownRenderCache.blockquotes = NULL;
        g_markdownRenderCache.colorTags = NULL;
        g_markdownRenderCache.fontTags = NULL;
        g_markdownRenderCache.mdText = NULL;
        g_markdownRenderCache.headingCount = 0;
        g_markdownRenderCache.styleCount = 0;
        g_markdownRenderCache.listItemCount = 0;
        g_markdownRenderCache.blockquoteCount = 0;
        g_markdownRenderCache.colorTagCount = 0;
        g_markdownRenderCache.fontTagCount = 0;
        g_markdownRenderCache.valid = TRUE;
        return;
    }

    g_markdownRenderCache.valid = TRUE;
}

/**
 * @note Static buffers avoid per-frame allocation
 */
BOOL ExpandFontPathEnvironmentUtf8(const char* fontFileName, char* outPath, size_t outPathSize) {
    if (!fontFileName || !outPath || outPathSize == 0) return FALSE;

    if (_strnicmp(fontFileName, "%LOCALAPPDATA%", strlen("%LOCALAPPDATA%")) == 0) {
        return ExpandEffectiveLocalAppDataPath(fontFileName, outPath, outPathSize);
    }

    wchar_t fontFileNameW[MAX_PATH] = {0};
    wchar_t expandedW[MAX_PATH] = {0};
    if (!Utf8ToWide(fontFileName, fontFileNameW, MAX_PATH)) {
        return FALSE;
    }

    DWORD expandedLen = ExpandEnvironmentStringsW(fontFileNameW, expandedW, _countof(expandedW));
    if (expandedLen == 0 || expandedLen >= _countof(expandedW)) {
        return FALSE;
    }

    return WideToUtf8(expandedW, outPath, outPathSize);
}

BOOL ResolveFontPathFromName(const char* fontFileName, char* outPath) {
    if (!fontFileName || !outPath) return FALSE;

    const char* relPath = ExtractRelativePath(fontFileName);
    if (relPath) {
        return BuildFullFontPath(relPath, outPath, MAX_PATH);
    }

    if (ExpandFontPathEnvironmentUtf8(fontFileName, outPath, MAX_PATH)) {
        if (!strchr(outPath, ':')) {
            char simpleName[MAX_PATH];
            strcpy_s(simpleName, MAX_PATH, outPath);
            return BuildFullFontPath(simpleName, outPath, MAX_PATH);
        }
        return TRUE;
    }

    outPath[0] = '\0';
    return FALSE;
}

BOOL ResolveFontPathFromNameCached(const char* fontFileName,
                                          char* outPath,
                                          size_t outPathSize) {
    if (!fontFileName || !outPath || outPathSize == 0) return FALSE;
    outPath[0] = '\0';

    if (g_fontPathResolveCache.valid &&
        strcmp(g_fontPathResolveCache.fontFileName, fontFileName) == 0) {
        if (!g_fontPathResolveCache.resolved) {
            DWORD now = GetTickCount();
            if ((LONG)(g_fontPathResolveCache.retryAfterFailureTick - now) > 0) {
                return FALSE;
            }
        } else {
            if (strlen(g_fontPathResolveCache.absoluteFontPath) >= outPathSize) {
                return FALSE;
            }
            strcpy_s(outPath, outPathSize, g_fontPathResolveCache.absoluteFontPath);
            return TRUE;
        }
    }

    char resolvedPath[MAX_PATH] = {0};
    BOOL resolved = ResolveFontPathFromName(fontFileName, resolvedPath);

    ZeroMemory(&g_fontPathResolveCache, sizeof(g_fontPathResolveCache));
    g_fontPathResolveCache.valid = TRUE;
    strcpy_s(g_fontPathResolveCache.fontFileName,
             sizeof(g_fontPathResolveCache.fontFileName),
             fontFileName);
    g_fontPathResolveCache.resolved = resolved;
    if (resolved) {
        strcpy_s(g_fontPathResolveCache.absoluteFontPath,
                 sizeof(g_fontPathResolveCache.absoluteFontPath),
                 resolvedPath);
    } else {
        DWORD retryAfter = GetTickCount() + FONT_PATH_RESOLVE_FAILURE_RETRY_MS;
        g_fontPathResolveCache.retryAfterFailureTick = retryAfter ? retryAfter : 1;
    }

    if (!resolved) {
        return FALSE;
    }

    if (strlen(resolvedPath) >= outPathSize) {
        return FALSE;
    }
    strcpy_s(outPath, outPathSize, resolvedPath);
    return TRUE;
}
