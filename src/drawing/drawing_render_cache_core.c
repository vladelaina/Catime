/**
 * @file drawing_render_cache_core.c
 * @brief Text and markdown cache signatures and stable measurement text.
 */

#include "drawing_render_internal.h"

void ClearTextMeasureCache(void) {
    ZeroMemory(&g_textMeasureCache, sizeof(g_textMeasureCache));
}
DWORD ComputeHeadingSignature(const MarkdownHeading* headings, int headingCount) {
    DWORD hash = 2166136261u;

    if (!headings || headingCount <= 0) {
        return (hash ^ 0u) * 16777619u;
    }

    hash = (hash ^ (DWORD)headingCount) * 16777619u;
    for (int i = 0; i < headingCount; ++i) {
        hash = (hash ^ (DWORD)headings[i].level) * 16777619u;
        hash = (hash ^ (DWORD)headings[i].startPos) * 16777619u;
        hash = (hash ^ (DWORD)headings[i].endPos) * 16777619u;
    }

    return hash;
}

DWORD ComputeFontTagSignature(const MarkdownFontTag* fontTags, int fontTagCount) {
    DWORD hash = 2166136261u;

    if (!fontTags || fontTagCount <= 0) {
        return (hash ^ 0u) * 16777619u;
    }

    hash = (hash ^ (DWORD)fontTagCount) * 16777619u;
    for (int i = 0; i < fontTagCount; ++i) {
        hash = (hash ^ (DWORD)fontTags[i].startPos) * 16777619u;
        hash = (hash ^ (DWORD)fontTags[i].endPos) * 16777619u;
        const wchar_t* p = fontTags[i].fontName;
        while (p && *p) {
            hash = (hash ^ (DWORD)*p++) * 16777619u;
        }
    }

    return hash;
}

void ClearMarkdownRenderCache(void) {
    if (g_markdownRenderCache.links) {
        FreeMarkdownLinks(g_markdownRenderCache.links, g_markdownRenderCache.linkCount);
    }
    free(g_markdownRenderCache.headings);
    free(g_markdownRenderCache.styles);
    free(g_markdownRenderCache.listItems);
    free(g_markdownRenderCache.blockquotes);
    free(g_markdownRenderCache.colorTags);
    free(g_markdownRenderCache.fontTags);
    free(g_markdownRenderCache.mdText);
    ZeroMemory(&g_markdownRenderCache, sizeof(g_markdownRenderCache));
}

void ClearPluginPaintCache(void) {
    if (g_pluginPaintCache.images) {
        FreeMarkdownImages(g_pluginPaintCache.images, g_pluginPaintCache.imageCount);
    }
    ZeroMemory(&g_pluginPaintCache, sizeof(g_pluginPaintCache));
}

void CopyCachedWideText(wchar_t* dest, size_t destCount,
                               size_t* storedLen, const wchar_t* src) {
    if (!dest || destCount == 0) {
        if (storedLen) {
            *storedLen = src ? wcslen(src) : 0;
        }
        return;
    }

    size_t srcLen = src ? wcslen(src) : 0;
    if (storedLen) {
        *storedLen = srcLen;
    }
    if (!src) {
        dest[0] = L'\0';
        return;
    }

    wcsncpy(dest, src, destCount - 1);
    dest[destCount - 1] = L'\0';
}

BOOL CachedWideTextEquals(const wchar_t* cached, size_t cachedCount,
                                 size_t storedLen, const wchar_t* text) {
    if (!cached || cachedCount == 0 || !text) return FALSE;

    size_t textLen = wcslen(text);
    if (storedLen != textLen) {
        return FALSE;
    }
    if (textLen >= cachedCount) {
        return FALSE;
    }

    return wcsncmp(cached, text, textLen) == 0 &&
           cached[textLen] == L'\0';
}

BOOL BuildStableDigitMeasureText(const wchar_t* source, wchar_t* dest, size_t destCount) {
    if (!source || !dest || destCount == 0) {
        return FALSE;
    }

    BOOL changed = FALSE;
    size_t i = 0;
    for (; source[i] && i + 1 < destCount; ++i) {
        wchar_t ch = source[i];
        if (ch >= L'0' && ch <= L'9') {
            dest[i] = L'8';
            changed = TRUE;
        } else {
            dest[i] = ch;
        }
    }
    dest[i] = L'\0';

    return changed;
}

void StabilizeScaleGestureText(HWND hwnd, wchar_t* text, size_t textCount) {
    if (!text || textCount == 0) return;

    DWORD gestureSerial = GetScaleWindowVisualSerial(hwnd);
    if (gestureSerial == 0) {
        ZeroMemory(&g_scaleGestureTextCache, sizeof(g_scaleGestureTextCache));
        return;
    }

    if (g_scaleGestureTextCache.valid &&
        g_scaleGestureTextCache.hwnd == hwnd &&
        g_scaleGestureTextCache.gestureSerial == gestureSerial) {
        wcscpy_s(text, textCount, g_scaleGestureTextCache.text);
        return;
    }

    g_scaleGestureTextCache.valid = TRUE;
    g_scaleGestureTextCache.hwnd = hwnd;
    g_scaleGestureTextCache.gestureSerial = gestureSerial;
    wcscpy_s(g_scaleGestureTextCache.text,
             _countof(g_scaleGestureTextCache.text),
             text);
}

BOOL CanUseMeasureCacheForFontTags(const MarkdownFontTag* fontTags, int fontTagCount) {
    if (!fontTags || fontTagCount <= 0) {
        return TRUE;
    }

    int uniqueCount = 0;
    for (int i = 0; i < fontTagCount; i++) {
        BOOL seen = FALSE;
        for (int j = 0; j < i; j++) {
            if (wcscmp(fontTags[i].fontName, fontTags[j].fontName) == 0) {
                seen = TRUE;
                break;
            }
        }
        if (seen) {
            continue;
        }

        if (++uniqueCount > MAX_CACHED_FONTS) {
            return FALSE;
        }
    }

    return TRUE;
}
