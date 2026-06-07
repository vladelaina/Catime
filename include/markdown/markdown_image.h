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
#include "drawing/drawing_image.h"

/**
 * @brief Parsed markdown image ![alt](path)
 */
typedef struct {
    wchar_t* imagePath;      /* Original path (URL or local) */
    wchar_t* resolvedPath;   /* Resolved local path (for rendering) */
    int specifiedWidth;      /* Width from ![WxH], 0 if not specified */
    int specifiedHeight;     /* Height from ![WxH], 0 if not specified */
    int intrinsicWidth;      /* Cached source image width, 0 if unknown */
    int intrinsicHeight;     /* Cached source image height, 0 if unknown */
    FILETIME resolvedLastWriteTime; /* Last write time for resolvedPath */
    ULONGLONG resolvedFileSize; /* File size for resolvedPath */
    BOOL resolvedFileInfoValid; /* TRUE when resolved file metadata is known */
    int startPos;            /* Position in display text */
    int endPos;              /* End position in display text */
    RECT imageRect;          /* Rendered rectangle (for click detection) */
    BOOL isNetworkImage;     /* TRUE if http/https URL */
    BOOL isDownloaded;       /* TRUE if network image is cached locally */
    BOOL isDownloading;      /* TRUE if download in progress */
    BOOL downloadFailed;     /* TRUE if download failed */
    BOOL downloadRetryScheduled; /* TRUE if downloadRetryTick is valid */
    DWORD downloadRetryTick; /* Tick count when failed network image may retry */
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
 * @brief Check if a network image URL is currently downloading
 * @param url HTTP/HTTPS URL
 * @return TRUE if a background download is active for this URL
 */
BOOL IsMarkdownImageDownloadInProgress(const wchar_t* url);

/**
 * @brief Get retry time for a recently failed network image URL
 * @param url HTTP/HTTPS URL
 * @param retryTick Output GetTickCount() value when retry is allowed
 * @return TRUE if URL is still inside the failure retry window
 */
BOOL GetMarkdownImageDownloadRetryTick(const wchar_t* url, DWORD* retryTick);

/**
 * @brief Check if image is cached locally
 * @param url Network URL
 * @param localPath Output local path if cached
 * @return TRUE if cached
 */
BOOL IsImageCached(const wchar_t* url, wchar_t* localPath);

/**
 * @brief Check whether an image path points to a non-empty regular file
 * @param path Local image path
 * @return TRUE if usable by the image renderer
 */
BOOL IsMarkdownImageFileUsable(const wchar_t* path);

/**
 * @brief Revalidate a resolved image file and clear cached dimensions on change
 * @param image Image with a resolvedPath
 * @return TRUE if the resolved file is still usable
 */
BOOL RefreshMarkdownImageResolvedFileState(MarkdownImage* image);

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
 * @brief Shutdown async download bookkeeping and release synchronization state
 */
void ShutdownMarkdownImage(void);

/**
 * @brief Free per-image path resources without freeing the image array itself
 * @param images Image array
 * @param imageCount Image count
 */
void FreeMarkdownImageEntries(MarkdownImage* images, int imageCount);

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

/**
 * @brief Render image using dimensions already calculated by CalculateImageRenderSize
 * @param hdc Device context
 * @param image Image with resolved path
 * @param x X position
 * @param y Y position
 * @param actualWidth Render width
 * @param actualHeight Render height
 * @return Actual height used
 */
int RenderMarkdownImageSized(HDC hdc, MarkdownImage* image, int x, int y,
                             int actualWidth, int actualHeight);

/**
 * @brief Render image using dimensions already calculated and a shared render context
 */
int RenderMarkdownImageSizedWithContext(ImageRenderContext* renderCtx,
                                        MarkdownImage* image, int x, int y,
                                        int actualWidth, int actualHeight);

#endif // MARKDOWN_IMAGE_H
