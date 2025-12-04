/**
 * @file markdown_image.c
 * @brief Markdown image parsing and rendering
 */

#include "markdown/markdown_image.h"
#include "drawing/drawing_image.h"
#include "plugin/plugin_data.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")

/* ============================================================================
 * Path Resolution
 * ============================================================================ */

BOOL GetPluginsDirectory(wchar_t* buffer, size_t bufferSize) {
    DWORD result = ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Catime\\resources\\plugins",
        buffer, (DWORD)bufferSize);
    return (result > 0 && result < bufferSize);
}

BOOL GetImageCacheDirectory(wchar_t* buffer, size_t bufferSize) {
    /* Use system temp directory: %TEMP%\Catime\images */
    wchar_t tempDir[MAX_PATH];
    DWORD result = GetTempPathW(MAX_PATH, tempDir);
    if (result == 0 || result >= MAX_PATH) return FALSE;
    
    /* Build path: %TEMP%\Catime\images */
    wchar_t catimeDir[MAX_PATH];
    _snwprintf_s(catimeDir, MAX_PATH, _TRUNCATE, L"%sCatime", tempDir);
    CreateDirectoryW(catimeDir, NULL);
    
    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%sCatime\\images", tempDir);
    CreateDirectoryW(buffer, NULL);
    
    return TRUE;
}

/**
 * @brief Check if path is a network URL
 */
static BOOL IsNetworkUrl(const wchar_t* path) {
    if (!path) return FALSE;
    return (_wcsnicmp(path, L"http://", 7) == 0 || 
            _wcsnicmp(path, L"https://", 8) == 0);
}

/**
 * @brief Check if path is absolute
 */
static BOOL IsAbsolutePath(const wchar_t* path) {
    if (!path || wcslen(path) < 2) return FALSE;
    /* Check for drive letter (C:\...) or UNC path (\\...) */
    return ((path[1] == L':') || (path[0] == L'\\' && path[1] == L'\\'));
}

/**
 * @brief Generate cache filename from URL (simple hash)
 */
static void GenerateCacheFilename(const wchar_t* url, wchar_t* filename, size_t size) {
    /* Simple hash of URL */
    unsigned int hash = 0;
    const wchar_t* p = url;
    while (*p) {
        hash = hash * 31 + *p;
        p++;
    }
    
    /* Extract extension from URL */
    const wchar_t* ext = L".png";  /* Default */
    const wchar_t* lastDot = wcsrchr(url, L'.');
    if (lastDot) {
        if (_wcsnicmp(lastDot, L".jpg", 4) == 0 || _wcsnicmp(lastDot, L".jpeg", 5) == 0) {
            ext = L".jpg";
        } else if (_wcsnicmp(lastDot, L".gif", 4) == 0) {
            ext = L".gif";
        } else if (_wcsnicmp(lastDot, L".bmp", 4) == 0) {
            ext = L".bmp";
        } else if (_wcsnicmp(lastDot, L".webp", 5) == 0) {
            ext = L".webp";
        }
    }
    
    _snwprintf_s(filename, size, _TRUNCATE, L"%08X%s", hash, ext);
}

/* ============================================================================
 * Network Image Download
 * ============================================================================ */

BOOL DownloadImageToCache(const wchar_t* url, wchar_t* localPath) {
    if (!url || !localPath) return FALSE;
    
    /* Get cache directory */
    wchar_t cacheDir[MAX_PATH];
    if (!GetImageCacheDirectory(cacheDir, MAX_PATH)) {
        LOG_ERROR("Failed to get image cache directory");
        return FALSE;
    }
    
    /* Generate cache filename */
    wchar_t filename[64];
    GenerateCacheFilename(url, filename, 64);
    
    /* Build full cache path */
    _snwprintf_s(localPath, MAX_PATH, _TRUNCATE, L"%s\\%s", cacheDir, filename);
    
    /* Check if already cached */
    if (GetFileAttributesW(localPath) != INVALID_FILE_ATTRIBUTES) {
        LOG_INFO("Image already cached: %ls", filename);
        return TRUE;
    }
    
    LOG_INFO("Downloading image: %ls", url);
    
    /* Open Internet session */
    HINTERNET hInternet = InternetOpenW(L"Catime/1.0", INTERNET_OPEN_TYPE_DIRECT, 
                                         NULL, NULL, 0);
    if (!hInternet) {
        LOG_ERROR("Failed to open Internet session");
        return FALSE;
    }
    
    /* Open URL */
    HINTERNET hUrl = InternetOpenUrlW(hInternet, url, NULL, 0,
                                       INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hUrl) {
        LOG_ERROR("Failed to open URL: %ls", url);
        InternetCloseHandle(hInternet);
        return FALSE;
    }
    
    /* Create local file */
    HANDLE hFile = CreateFileW(localPath, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to create cache file: %ls", localPath);
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return FALSE;
    }
    
    /* Download and write */
    BYTE buffer[8192];
    DWORD bytesRead, bytesWritten;
    DWORD totalBytes = 0;
    BOOL success = TRUE;
    
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        if (!WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL) || 
            bytesWritten != bytesRead) {
            success = FALSE;
            break;
        }
        totalBytes += bytesRead;
        
        /* Limit to 10MB */
        if (totalBytes > 10 * 1024 * 1024) {
            LOG_ERROR("Image too large (>10MB)");
            success = FALSE;
            break;
        }
    }
    
    CloseHandle(hFile);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    
    if (success && totalBytes > 0) {
        LOG_INFO("Downloaded %lu bytes to: %ls", totalBytes, filename);
        return TRUE;
    } else {
        /* Delete incomplete file */
        DeleteFileW(localPath);
        return FALSE;
    }
}

/**
 * @brief Check if image is already cached
 */
BOOL IsImageCached(const wchar_t* url, wchar_t* localPath) {
    if (!url || !localPath) return FALSE;
    
    wchar_t cacheDir[MAX_PATH];
    if (!GetImageCacheDirectory(cacheDir, MAX_PATH)) return FALSE;
    
    wchar_t filename[64];
    GenerateCacheFilename(url, filename, 64);
    
    _snwprintf_s(localPath, MAX_PATH, _TRUNCATE, L"%s\\%s", cacheDir, filename);
    
    return (GetFileAttributesW(localPath) != INVALID_FILE_ATTRIBUTES);
}

/* ============================================================================
 * Async Download
 * ============================================================================ */

/* Track URLs currently being downloaded (simple hash set) */
#define MAX_DOWNLOADING 16
static unsigned int g_downloadingHashes[MAX_DOWNLOADING] = {0};
static int g_downloadingCount = 0;
static CRITICAL_SECTION g_downloadCS;
static volatile LONG g_downloadCSInit = 0;

static void EnsureDownloadCSInit(void) {
    if (InterlockedCompareExchange(&g_downloadCSInit, 1, 0) == 0) {
        InitializeCriticalSection(&g_downloadCS);
        InterlockedExchange(&g_downloadCSInit, 2);
    }
    /* Wait for initialization to complete */
    while (g_downloadCSInit == 1) Sleep(0);
}

static unsigned int HashUrl(const wchar_t* url) {
    unsigned int hash = 0;
    while (*url) {
        hash = hash * 31 + *url;
        url++;
    }
    return hash ? hash : 1;  /* Avoid 0 as it means empty slot */
}

static BOOL IsUrlDownloading(const wchar_t* url) {
    if (g_downloadCSInit != 2) return FALSE;
    unsigned int hash = HashUrl(url);
    
    EnterCriticalSection(&g_downloadCS);
    BOOL found = FALSE;
    for (int i = 0; i < g_downloadingCount; i++) {
        if (g_downloadingHashes[i] == hash) {
            found = TRUE;
            break;
        }
    }
    LeaveCriticalSection(&g_downloadCS);
    return found;
}

static void AddDownloadingUrl(const wchar_t* url) {
    EnsureDownloadCSInit();
    
    unsigned int hash = HashUrl(url);
    EnterCriticalSection(&g_downloadCS);
    if (g_downloadingCount < MAX_DOWNLOADING) {
        g_downloadingHashes[g_downloadingCount++] = hash;
    }
    LeaveCriticalSection(&g_downloadCS);
}

static void RemoveDownloadingUrl(const wchar_t* url) {
    if (g_downloadCSInit != 2) return;
    unsigned int hash = HashUrl(url);
    
    EnterCriticalSection(&g_downloadCS);
    for (int i = 0; i < g_downloadingCount; i++) {
        if (g_downloadingHashes[i] == hash) {
            g_downloadingHashes[i] = g_downloadingHashes[--g_downloadingCount];
            break;
        }
    }
    LeaveCriticalSection(&g_downloadCS);
}

typedef struct {
    wchar_t url[2048];
    wchar_t cachePath[MAX_PATH];
    HWND hwnd;
} AsyncDownloadParams;

static DWORD WINAPI AsyncDownloadThread(LPVOID param) {
    AsyncDownloadParams* p = (AsyncDownloadParams*)param;
    
    /* Download synchronously in background */
    DownloadImageToCache(p->url, p->cachePath);
    
    /* Remove from downloading list */
    RemoveDownloadingUrl(p->url);
    
    /* Trigger window repaint */
    if (p->hwnd && IsWindow(p->hwnd)) {
        InvalidateRect(p->hwnd, NULL, FALSE);
    }
    
    free(p);
    return 0;
}

void StartAsyncImageDownload(MarkdownImage* image, HWND hwnd) {
    if (!image || !image->imagePath) return;
    if (!IsNetworkUrl(image->imagePath)) return;
    
    /* Check if already cached */
    wchar_t cachePath[MAX_PATH];
    if (IsImageCached(image->imagePath, cachePath)) {
        image->resolvedPath = _wcsdup(cachePath);
        if (!image->resolvedPath) return;  /* Memory allocation failed */
        image->isDownloaded = TRUE;
        image->isDownloading = FALSE;
        return;
    }
    
    /* Check if already downloading */
    if (IsUrlDownloading(image->imagePath)) {
        image->isDownloading = TRUE;
        return;
    }
    
    /* Mark as downloading */
    image->isDownloading = TRUE;
    AddDownloadingUrl(image->imagePath);
    
    /* Prepare params */
    AsyncDownloadParams* params = (AsyncDownloadParams*)malloc(sizeof(AsyncDownloadParams));
    if (!params) {
        RemoveDownloadingUrl(image->imagePath);
        image->isDownloading = FALSE;
        image->downloadFailed = TRUE;
        return;
    }
    
    wcsncpy(params->url, image->imagePath, 2047);
    params->url[2047] = L'\0';
    params->cachePath[0] = L'\0';
    params->hwnd = hwnd;
    
    /* Start background thread */
    HANDLE hThread = CreateThread(NULL, 0, AsyncDownloadThread, params, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
        LOG_INFO("Started async download for: %ls", image->imagePath);
    } else {
        free(params);
        RemoveDownloadingUrl(image->imagePath);
        image->isDownloading = FALSE;
        image->downloadFailed = TRUE;
    }
}

/* ============================================================================
 * Path Resolution
 * ============================================================================ */

BOOL ResolveImagePath(MarkdownImage* image) {
    if (!image || !image->imagePath) return FALSE;
    
    /* Free previous resolved path */
    if (image->resolvedPath) {
        free(image->resolvedPath);
        image->resolvedPath = NULL;
    }
    
    const wchar_t* path = image->imagePath;
    
    /* Network URL - check cache only, don't block */
    if (IsNetworkUrl(path)) {
        image->isNetworkImage = TRUE;
        
        /* Check if already cached */
        wchar_t cachePath[MAX_PATH];
        if (IsImageCached(path, cachePath)) {
            image->resolvedPath = _wcsdup(cachePath);
            if (!image->resolvedPath) return FALSE;
            image->isDownloaded = TRUE;
            image->isDownloading = FALSE;
            return TRUE;
        }
        
        /* Not cached - caller should use StartAsyncImageDownload */
        return FALSE;
    }
    
    image->isNetworkImage = FALSE;
    
    /* Absolute path */
    if (IsAbsolutePath(path)) {
        image->resolvedPath = _wcsdup(path);
        if (!image->resolvedPath) return FALSE;
        return (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
    }
    
    /* Relative path - resolve relative to plugins directory */
    wchar_t pluginsDir[MAX_PATH];
    if (!GetPluginsDirectory(pluginsDir, MAX_PATH)) {
        return FALSE;
    }
    
    wchar_t fullPath[MAX_PATH];
    _snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", pluginsDir, path);
    
    image->resolvedPath = _wcsdup(fullPath);
    if (!image->resolvedPath) return FALSE;
    return (GetFileAttributesW(fullPath) != INVALID_FILE_ATTRIBUTES);
}

/* ============================================================================
 * Parsing
 * ============================================================================ */

int CountMarkdownImages(const wchar_t* input) {
    if (!input) return 0;
    
    int count = 0;
    const wchar_t* p = input;
    
    while (*p) {
        if (*p == L'!' && *(p + 1) == L'[') {
            /* Find ]( */
            const wchar_t* bracketEnd = wcschr(p + 2, L']');
            if (bracketEnd && *(bracketEnd + 1) == L'(') {
                const wchar_t* parenEnd = wcschr(bracketEnd + 2, L')');
                if (parenEnd) {
                    count++;
                    p = parenEnd + 1;
                    continue;
                }
            }
        }
        p++;
    }
    
    return count;
}

/**
 * @brief Parse size from ![WxH] or ![W] format
 */
static void ParseImageSize(const wchar_t* sizeStr, size_t len, int* width, int* height) {
    *width = 0;
    *height = 0;
    
    if (len == 0) return;
    
    /* Find 'x' separator */
    const wchar_t* xPos = NULL;
    for (size_t i = 0; i < len; i++) {
        if (sizeStr[i] == L'x' || sizeStr[i] == L'X') {
            xPos = &sizeStr[i];
            break;
        }
    }
    
    if (xPos) {
        /* WxH format */
        wchar_t widthStr[16] = {0};
        wchar_t heightStr[16] = {0};
        
        size_t wLen = xPos - sizeStr;
        if (wLen > 0 && wLen < 16) {
            wcsncpy(widthStr, sizeStr, wLen);
            *width = _wtoi(widthStr);
        }
        
        size_t hLen = len - wLen - 1;
        if (hLen > 0 && hLen < 16) {
            wcsncpy(heightStr, xPos + 1, hLen);
            *height = _wtoi(heightStr);
        }
    } else {
        /* Single number - treat as width, height auto */
        wchar_t numStr[16] = {0};
        if (len < 16) {
            wcsncpy(numStr, sizeStr, len);
            *width = _wtoi(numStr);
        }
    }
}

BOOL ExtractMarkdownImage(const wchar_t** src, MarkdownImage* images, 
                          int* imageCount, int imageCapacity, int currentPos) {
    if (!src || !*src || !images || !imageCount) return FALSE;
    if (*imageCount >= imageCapacity) return FALSE;
    
    const wchar_t* p = *src;
    
    /* Must start with ![ */
    if (*p != L'!' || *(p + 1) != L'[') return FALSE;
    
    p += 2;  /* Skip ![ */
    
    /* Find ] */
    const wchar_t* bracketEnd = wcschr(p, L']');
    if (!bracketEnd || *(bracketEnd + 1) != L'(') return FALSE;
    
    /* Extract size string (between [ and ]) */
    size_t sizeLen = bracketEnd - p;
    int specWidth = 0, specHeight = 0;
    if (sizeLen > 0) {
        ParseImageSize(p, sizeLen, &specWidth, &specHeight);
    }
    
    /* Find path between ( and ) */
    const wchar_t* pathStart = bracketEnd + 2;
    const wchar_t* pathEnd = wcschr(pathStart, L')');
    if (!pathEnd || pathEnd == pathStart) return FALSE;
    
    size_t pathLen = pathEnd - pathStart;
    
    /* Allocate and copy path */
    wchar_t* imagePath = (wchar_t*)malloc((pathLen + 1) * sizeof(wchar_t));
    if (!imagePath) return FALSE;
    
    wcsncpy(imagePath, pathStart, pathLen);
    imagePath[pathLen] = L'\0';
    
    /* Fill image structure */
    MarkdownImage* img = &images[*imageCount];
    memset(img, 0, sizeof(MarkdownImage));
    img->imagePath = imagePath;
    img->specifiedWidth = specWidth;
    img->specifiedHeight = specHeight;
    img->startPos = currentPos;
    img->endPos = currentPos;  /* Images don't occupy text space */
    
    /* Check if network image */
    img->isNetworkImage = IsNetworkUrl(imagePath);
    
    (*imageCount)++;
    *src = pathEnd + 1;
    
    return TRUE;
}

/* ============================================================================
 * Rendering
 * ============================================================================ */

/* External scale factors */
extern float CLOCK_FONT_SCALE_FACTOR;
extern float PLUGIN_FONT_SCALE_FACTOR;

BOOL CalculateImageRenderSize(MarkdownImage* image, int maxWidth, int maxHeight,
                              int* outWidth, int* outHeight) {
    if (!image || !outWidth || !outHeight) return FALSE;
    if (maxWidth <= 0 || maxHeight <= 0) return FALSE;
    
    *outWidth = 0;
    *outHeight = 0;
    
    /* Resolve path if not already done */
    if (!image->resolvedPath) {
        if (!ResolveImagePath(image)) return FALSE;
    }
    
    /* Get actual image dimensions */
    int imgW = 0, imgH = 0;
    if (!GetImageDimensions(image->resolvedPath, &imgW, &imgH) || imgW <= 0 || imgH <= 0) {
        return FALSE;
    }
    
    /* Apply scale factor based on current mode */
    float scale = PluginData_IsActive() ? PLUGIN_FONT_SCALE_FACTOR : CLOCK_FONT_SCALE_FACTOR;
    if (scale < 0.1f) scale = 1.0f;
    
    /* Determine target size */
    int targetW, targetH;
    if (image->specifiedWidth > 0 && image->specifiedHeight > 0) {
        /* User specified both dimensions */
        targetW = (int)(image->specifiedWidth * scale);
        targetH = (int)(image->specifiedHeight * scale);
    } else if (image->specifiedWidth > 0) {
        /* User specified width only - calculate height from aspect ratio */
        targetW = (int)(image->specifiedWidth * scale);
        targetH = (int)(imgH * ((float)targetW / imgW));
    } else if (image->specifiedHeight > 0) {
        /* User specified height only - calculate width from aspect ratio */
        targetH = (int)(image->specifiedHeight * scale);
        targetW = (int)(imgW * ((float)targetH / imgH));
    } else {
        /* No size specified - use original image size with scale */
        targetW = (int)(imgW * scale);
        targetH = (int)(imgH * scale);
    }
    
    /* Clamp to max bounds */
    if (targetW > maxWidth) targetW = maxWidth;
    if (targetH > maxHeight) targetH = maxHeight;
    
    /* Calculate actual size maintaining aspect ratio */
    float scaleX = (float)targetW / imgW;
    float scaleY = (float)targetH / imgH;
    float fitScale = (scaleX < scaleY) ? scaleX : scaleY;
    
    *outWidth = (int)(imgW * fitScale);
    *outHeight = (int)(imgH * fitScale);
    
    return TRUE;
}

int RenderMarkdownImage(HDC hdc, MarkdownImage* image, int x, int y, 
                        int maxWidth, int maxHeight) {
    if (!hdc || !image) return 0;
    
    /* Calculate render size (also resolves path) */
    int actualWidth = 0, actualHeight = 0;
    if (!CalculateImageRenderSize(image, maxWidth, maxHeight, &actualWidth, &actualHeight)) {
        return 0;
    }
    
    /* Render using GDI+ */
    if (RenderImageGDIPlus(hdc, x, y, actualWidth, actualHeight, image->resolvedPath)) {
        /* Update rect for click detection */
        image->imageRect.left = x;
        image->imageRect.top = y;
        image->imageRect.right = x + actualWidth;
        image->imageRect.bottom = y + actualHeight;
        
        return actualHeight;
    }
    
    return 0;
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void FreeMarkdownImages(MarkdownImage* images, int imageCount) {
    if (!images) return;
    
    for (int i = 0; i < imageCount; i++) {
        if (images[i].imagePath) {
            free(images[i].imagePath);
        }
        if (images[i].resolvedPath) {
            free(images[i].resolvedPath);
        }
    }
    free(images);
}
