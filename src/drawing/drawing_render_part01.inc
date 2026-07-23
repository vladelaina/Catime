    RedrawWindow(hwnd, NULL, NULL,
                 RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);

    /* A successful paint resets the controller synchronously. If Windows did
     * not dispatch WM_PAINT, keep probing at the current bounded delay. */
    if (RenderRetry_IsActive(&s_mainRenderRetry) &&
        !RenderRetry_IsTimerArmed(&s_mainRenderRetry)) {
        ArmMainWindowRenderRetry(
            hwnd,
            RenderRetry_GetDelay(&s_mainRenderRetry,
                                 MAIN_RENDER_RETRY_BASE_MS,
                                 MAIN_RENDER_RETRY_MAX_MS));
    }
    return TRUE;
}

typedef enum {
    PLUGIN_TEXT_MARKER_NONE = 0,
    PLUGIN_TEXT_MARKER_IMAGE,
    PLUGIN_TEXT_MARKER_CATIME
} PluginTextMarkerKind;

static void AppendWideSpan(wchar_t** dst, size_t* remaining,
                           const wchar_t* text, size_t textLen) {
    if (!dst || !*dst || !remaining || !text || *remaining == 0 || textLen == 0) {
        return;
    }

    if (textLen > *remaining) {
        textLen = *remaining;
    }

    memcpy(*dst, text, textLen * sizeof(wchar_t));
    *dst += textLen;
    *remaining -= textLen;
}

static const wchar_t* FindNextPluginTextMarker(const wchar_t* src,
                                               BOOL includeImages,
                                               PluginTextMarkerKind* markerKind) {
    if (markerKind) {
        *markerKind = PLUGIN_TEXT_MARKER_NONE;
    }
    if (!src) {
        return NULL;
    }

    for (const wchar_t* p = src; *p; ++p) {
        if (includeImages && p[0] == L'!' && p[1] == L'[') {
            if (markerKind) {
                *markerKind = PLUGIN_TEXT_MARKER_IMAGE;
            }
            return p;
        }

        if (p[0] == L'<' &&
            wcsncmp(p, CATIME_OPEN_TAG, CATIME_OPEN_TAG_LEN) == 0) {
            if (markerKind) {
                *markerKind = PLUGIN_TEXT_MARKER_CATIME;
            }
            return p;
        }
    }

    return NULL;
}

static COLORREF ParseColorString(const char* colorStr, const GradientInfo* gradientInfo) {
    if (!colorStr) {
        return RGB(255, 255, 255);
    }

    size_t colorLen = strlen(colorStr);
    if (colorLen == 0) {
        return RGB(255, 255, 255);
    }

    /* Use gradient start color for fallback GDI drawing paths. */
    if (gradientInfo) {
        return gradientInfo->startColor;
    }

    COLORREF parsed = RGB(255, 255, 255);
    return ColorStringToColorRef(colorStr, &parsed) ? parsed :
           RGB(255, 255, 255);
}

static int CalculateRenderFontSize(int baseFontSize, float scaleFactor) {
    double scaled = (double)baseFontSize * (double)scaleFactor;
    if (!isfinite(scaled) || scaled < 1.0) {
        return 1;
    }
    if (scaled > (double)INT_MAX) {
        return INT_MAX;
    }
    return (int)scaled;
}

static BOOL HasPotentialMarkdownSyntax(const wchar_t* text) {
    if (!text) return FALSE;

    for (const wchar_t* p = text; *p; ++p) {
        switch (*p) {
            case L'!':
            case L'[':
            case L']':
            case L'(':
            case L')':
            case L'<':
            case L'>':
            case L'*':
            case L'_':
            case L'`':
            case L'#':
                return TRUE;
            default:
                break;
        }
    }

    return FALSE;
}

typedef struct {
    BOOL valid;
    BOOL isMarkdown;
    wchar_t sourceText[TIME_TEXT_MAX_LEN];
    size_t sourceTextLen;
    wchar_t* mdText;
    MarkdownLink* links;
    int linkCount;
    MarkdownHeading* headings;
    int headingCount;
    MarkdownStyle* styles;
    int styleCount;
    MarkdownListItem* listItems;
    int listItemCount;
    MarkdownBlockquote* blockquotes;
    int blockquoteCount;
    MarkdownColorTag* colorTags;
    int colorTagCount;
    MarkdownFontTag* fontTags;
    int fontTagCount;
} MarkdownRenderCache;

static MarkdownRenderCache g_markdownRenderCache = {0};

typedef struct {
    BOOL valid;
    wchar_t sourceText[TIME_TEXT_MAX_LEN];
    size_t sourceTextLen;
    wchar_t renderedText[TIME_TEXT_MAX_LEN];
    MarkdownImage* images;
    int imageCount;
} PluginPaintCache;

static PluginPaintCache g_pluginPaintCache = {0};

typedef struct {
    BOOL valid;
    BOOL isMarkdown;
    int fontSize;
    float fontScaleFactor;
    DWORD headingSignature;
    DWORD fontTagSignature;
    DWORD fontStateGeneration;
    char fontPath[MAX_PATH];
    wchar_t text[TIME_TEXT_MAX_LEN];
    size_t textLen;
    SIZE size;
} TextMeasureCache;

static TextMeasureCache g_textMeasureCache = {0};

static void ClearTextMeasureCache(void) {
    ZeroMemory(&g_textMeasureCache, sizeof(g_textMeasureCache));
}

typedef struct {
    BOOL valid;
    char fontFileName[MAX_PATH];
    char absoluteFontPath[MAX_PATH];
    BOOL resolved;
    DWORD retryAfterFailureTick;
} FontPathResolveCache;

static FontPathResolveCache g_fontPathResolveCache = {0};

static DWORD ComputeHeadingSignature(const MarkdownHeading* headings, int headingCount) {
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

static DWORD ComputeFontTagSignature(const MarkdownFontTag* fontTags, int fontTagCount) {
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

static void ClearMarkdownRenderCache(void) {
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

static void ClearPluginPaintCache(void) {
    if (g_pluginPaintCache.images) {
        FreeMarkdownImages(g_pluginPaintCache.images, g_pluginPaintCache.imageCount);
    }
    ZeroMemory(&g_pluginPaintCache, sizeof(g_pluginPaintCache));
}

static void CopyCachedWideText(wchar_t* dest, size_t destCount,
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
