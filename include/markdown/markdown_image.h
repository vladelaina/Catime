/**
 * @file markdown_image.h
 * @brief Markdown image support ![WxH](path)
 * 
 * Supports:
 * - Local paths (relative to plugins directory or absolute)
 * - Network images (http/https, cached locally)
 * - Optional size specification: ![100x50](path)
 */

#ifndef MARKDOWN_IMAGE_H
#define MARKDOWN_IMAGE_H

#include <windows.h>

/**
 * @brief Parsed markdown image ![alt](path)
 */
typedef struct {
    wchar_t* imagePath;      /* Original path (URL or local) */
    wchar_t* resolvedPath;   /* Resolved local path (for rendering) */
    int specifiedWidth;      /* Width from ![WxH], 0 if not specified */
    int specifiedHeight;     /* Height from ![WxH], 0 if not specified */
    int startPos;            /* Position in display text */
    int endPos;              /* End position in display text */
    RECT imageRect;          /* Rendered rectangle (for click detection) */
    BOOL isNetworkImage;     /* TRUE if http/https URL */
    BOOL isDownloaded;       /* TRUE if network image is cached locally */
    BOOL isDownloading;      /* TRUE if download in progress */
    BOOL downloadFailed;     /* TRUE if download failed */
} MarkdownImage;

/**
 * @brief Count ![...](...) patterns in text
 * @param input Input text
 * @return Number of images found
 */
int CountMarkdownImages(const wchar_t* input);

/**
 * @brief Extract image from ![WxH](path) syntax
 * @param src Current position in source (updated on success)
 * @param images Output image array
 * @param imageCount Current count (incremented on success)
 * @param imageCapacity Array capacity
 * @param currentPos Current position in display text
 * @return TRUE if image extracted
 */
BOOL ExtractMarkdownImage(const wchar_t** src, MarkdownImage* images, 
                          int* imageCount, int imageCapacity, int currentPos);

/**
 * @brief Resolve image path to local file
 * 
 * - Relative paths: resolved relative to plugins directory
 * - Absolute paths: used as-is
 * - Network URLs: downloaded to cache directory
 * 
 * @param image Image to resolve
 * @return TRUE if resolved successfully
 */
BOOL ResolveImagePath(MarkdownImage* image);

/**
 * @brief Download network image to local cache (synchronous)
 * @param url HTTP/HTTPS URL
 * @param localPath Output local path (caller provides buffer, MAX_PATH size)
 * @return TRUE if downloaded successfully
 */
BOOL DownloadImageToCache(const wchar_t* url, wchar_t* localPath);

/**
 * @brief Start async download for a network image
 * @param image Image to download (sets isDownloading flag)
 * @param hwnd Window to notify when complete (WM_USER+100)
 */
void StartAsyncImageDownload(MarkdownImage* image, HWND hwnd);

/**
 * @brief Check if image is cached locally
 * @param url Network URL
 * @param localPath Output local path if cached
 * @return TRUE if cached
 */
BOOL IsImageCached(const wchar_t* url, wchar_t* localPath);

/**
 * @brief Get plugins directory path
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @return TRUE if successful
 */
BOOL GetPluginsDirectory(wchar_t* buffer, size_t bufferSize);

/**
 * @brief Get image cache directory path
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @return TRUE if successful
 */
BOOL GetImageCacheDirectory(wchar_t* buffer, size_t bufferSize);

/**
 * @brief Free image resources
 * @param images Image array
 * @param imageCount Image count
 */
void FreeMarkdownImages(MarkdownImage* images, int imageCount);

/**
 * @brief Calculate actual render dimensions for an image
 * @param image Image with resolved path
 * @param maxWidth Maximum width constraint
 * @param maxHeight Maximum height constraint
 * @param outWidth Output actual width
 * @param outHeight Output actual height
 * @return TRUE if successful
 */
BOOL CalculateImageRenderSize(MarkdownImage* image, int maxWidth, int maxHeight,
                              int* outWidth, int* outHeight);

/**
 * @brief Render image at specified position
 * @param hdc Device context
 * @param image Image to render
 * @param x X position
 * @param y Y position
 * @param maxWidth Maximum width constraint
 * @param maxHeight Maximum height constraint
 * @return Actual height used
 */
int RenderMarkdownImage(HDC hdc, MarkdownImage* image, int x, int y, 
                        int maxWidth, int maxHeight);

#endif // MARKDOWN_IMAGE_H
