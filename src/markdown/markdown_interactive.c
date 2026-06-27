/**
 * @file markdown_interactive.c
 * @brief Markdown interactive elements implementation
 */

#include "markdown/markdown_interactive.h"
#include "markdown/markdown_image.h"
#include "plugin/plugin_data.h"
#include "log.h"
#include "utils/string_convert.h"
#include "utils/url_safety.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <shellapi.h>

/*
 * Design note: checkbox writes must use the same source path as the content
 * currently being displayed. For plugins this is output.txt; for custom text
 * display this is custom_display.txt.
 */
#define CHECKBOX_OUTPUT_MAX_BYTES (1024ll * 1024ll)
#define CHECKBOX_OUTPUT_FILE_SHARE (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

/* ============================================================================
 * Global State
 * ============================================================================ */

static ClickableRegion g_regions[MAX_CLICKABLE_REGIONS];
static int g_regionCount = 0;
static volatile LONG g_regionCountVisible = 0;
static int g_windowOffsetX = 0;
static int g_windowOffsetY = 0;
static CRITICAL_SECTION g_interactiveCS;
static volatile LONG g_initialized = 0;
static SRWLOCK g_interactiveLifecycleLock = SRWLOCK_INIT;

#define INTERACTIVE_CS_UNINITIALIZED 0
#define INTERACTIVE_CS_INITIALIZING 1
#define INTERACTIVE_CS_INITIALIZED 2

static BOOL HandleRegionClick(const ClickableRegion* region, HWND hwnd);

static BOOL IsValidMarkdownInteractiveWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static BOOL IsInteractiveInitialized(void) {
    return InterlockedCompareExchange(&g_initialized, 0, 0) == INTERACTIVE_CS_INITIALIZED;
}

static BOOL BeginInteractiveUse(void) {
    AcquireSRWLockShared(&g_interactiveLifecycleLock);
    if (!IsInteractiveInitialized()) {
        ReleaseSRWLockShared(&g_interactiveLifecycleLock);
        return FALSE;
    }
    return TRUE;
}

static void EndInteractiveUse(void) {
    ReleaseSRWLockShared(&g_interactiveLifecycleLock);
}

static void ClearClickableRegionsLocked(void) {
    for (int i = 0; i < g_regionCount && i < MAX_CLICKABLE_REGIONS; ++i) {
        free(g_regions[i].url);
        g_regions[i].url = NULL;
    }
    ZeroMemory(g_regions, sizeof(g_regions));
    g_regionCount = 0;
    InterlockedExchange(&g_regionCountVisible, 0);
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

void InitMarkdownInteractive(void) {
    AcquireSRWLockExclusive(&g_interactiveLifecycleLock);
    if (IsInteractiveInitialized()) {
        ReleaseSRWLockExclusive(&g_interactiveLifecycleLock);
        return;
    }

    InitializeMarkdownImage();

    if (InterlockedCompareExchange(&g_initialized,
                                   INTERACTIVE_CS_INITIALIZING,
                                   INTERACTIVE_CS_UNINITIALIZED) == INTERACTIVE_CS_UNINITIALIZED) {
        InitializeCriticalSection(&g_interactiveCS);
        g_regionCount = 0;
        g_windowOffsetX = 0;
        g_windowOffsetY = 0;
        InterlockedExchange(&g_regionCountVisible, 0);
        InterlockedExchange(&g_initialized, INTERACTIVE_CS_INITIALIZED);
    }
    ReleaseSRWLockExclusive(&g_interactiveLifecycleLock);
}

void CleanupMarkdownInteractive(void) {
    AcquireSRWLockExclusive(&g_interactiveLifecycleLock);
    if (!IsInteractiveInitialized()) {
        ReleaseSRWLockExclusive(&g_interactiveLifecycleLock);
        return;
    }

    EnterCriticalSection(&g_interactiveCS);
    ClearClickableRegionsLocked();
    LeaveCriticalSection(&g_interactiveCS);

    DeleteCriticalSection(&g_interactiveCS);
    InterlockedExchange(&g_initialized, INTERACTIVE_CS_UNINITIALIZED);

    ShutdownMarkdownImage();
    ReleaseSRWLockExclusive(&g_interactiveLifecycleLock);
}

/* ============================================================================
 * Region Management
 * ============================================================================ */

void ClearClickableRegions(void) {
    if (!BeginInteractiveUse()) return;
    EnterCriticalSection(&g_interactiveCS);

    ClearClickableRegionsLocked();

    LeaveCriticalSection(&g_interactiveCS);
    EndInteractiveUse();
}

void AddLinkRegion(const RECT* rect, const wchar_t* url) {
    if (!rect || !url) return;
    if (!BeginInteractiveUse()) {
        return;
    }
    EnterCriticalSection(&g_interactiveCS);

    if (g_regionCount < MAX_CLICKABLE_REGIONS) {
        wchar_t* urlCopy = _wcsdup(url);
        if (!urlCopy) {
            LeaveCriticalSection(&g_interactiveCS);
            EndInteractiveUse();
            return;
        }

        ClickableRegion* r = &g_regions[g_regionCount];
        r->type = CLICK_TYPE_LINK;
        r->rect = *rect;
        r->url = urlCopy;
        r->checkboxIndex = -1;
        r->isChecked = FALSE;
        g_regionCount++;
        InterlockedExchange(&g_regionCountVisible, g_regionCount);
    }

    LeaveCriticalSection(&g_interactiveCS);
    EndInteractiveUse();
}

void AddCheckboxRegion(const RECT* rect, int index, BOOL isChecked) {
    if (!rect) return;
    if (!BeginInteractiveUse()) return;
    EnterCriticalSection(&g_interactiveCS);
    
    if (g_regionCount < MAX_CLICKABLE_REGIONS) {
        ClickableRegion* r = &g_regions[g_regionCount];
        r->type = CLICK_TYPE_CHECKBOX;
        /* Add small padding around the checkbox for easier clicking */
        #define CHECKBOX_PADDING 4
        r->rect.left = rect->left - CHECKBOX_PADDING;
        r->rect.top = rect->top;
        r->rect.right = rect->right + CHECKBOX_PADDING;
        r->rect.bottom = rect->bottom;
        r->url = NULL;
        r->checkboxIndex = index;
        r->isChecked = isChecked;
        g_regionCount++;
        InterlockedExchange(&g_regionCountVisible, g_regionCount);
    }

    LeaveCriticalSection(&g_interactiveCS);
    EndInteractiveUse();
}

void UpdateRegionPositions(int windowX, int windowY) {
    if (!BeginInteractiveUse()) return;

    EnterCriticalSection(&g_interactiveCS);
    if (g_windowOffsetX == windowX && g_windowOffsetY == windowY) {
        LeaveCriticalSection(&g_interactiveCS);
        EndInteractiveUse();
        return;
    }

    /* Just store window position - regions are in window coords */
    g_windowOffsetX = windowX;
    g_windowOffsetY = windowY;

    LeaveCriticalSection(&g_interactiveCS);
    EndInteractiveUse();
}

static BOOL CopyClickableRegionAtLocked(POINT localPt, ClickableRegion* outRegion) {
    if (!outRegion) return FALSE;

    for (int i = 0; i < g_regionCount; i++) {
        if (PtInRect(&g_regions[i].rect, localPt)) {
            *outRegion = g_regions[i];
            if (g_regions[i].url) {
                outRegion->url = _wcsdup(g_regions[i].url);
                if (!outRegion->url) {
                    return FALSE;
                }
            }
            return TRUE;
        }
    }

    return FALSE;
}

BOOL IsClickableRegionAt(POINT pt) {
    if (!BeginInteractiveUse()) return FALSE;
    if (InterlockedCompareExchange(&g_regionCountVisible, 0, 0) <= 0) {
        EndInteractiveUse();
        return FALSE;
    }

    BOOL result = FALSE;
    EnterCriticalSection(&g_interactiveCS);

    POINT localPt = { pt.x - g_windowOffsetX, pt.y - g_windowOffsetY };
    for (int i = 0; i < g_regionCount; i++) {
        if (PtInRect(&g_regions[i].rect, localPt)) {
            result = TRUE;
            break;
        }
    }

    LeaveCriticalSection(&g_interactiveCS);
    EndInteractiveUse();
    return result;
}

BOOL HandleRegionClickAt(POINT pt, HWND hwnd) {
    if (!BeginInteractiveUse()) return FALSE;
    if (InterlockedCompareExchange(&g_regionCountVisible, 0, 0) <= 0) {
        EndInteractiveUse();
        return FALSE;
    }

    ClickableRegion region = {0};
    BOOL found = FALSE;

    EnterCriticalSection(&g_interactiveCS);
    POINT localPt = { pt.x - g_windowOffsetX, pt.y - g_windowOffsetY };
    found = CopyClickableRegionAtLocked(localPt, &region);
    LeaveCriticalSection(&g_interactiveCS);
    EndInteractiveUse();

    if (!found) {
        return FALSE;
    }

    BOOL handled = HandleRegionClick(&region, hwnd);
    if (region.url) {
        free(region.url);
    }
    return handled;
}

BOOL HasClickableRegions(void) {
    if (!BeginInteractiveUse()) return FALSE;
    BOOL hasRegions = InterlockedCompareExchange(&g_regionCountVisible, 0, 0) > 0;
    EndInteractiveUse();
    return hasRegions;
}

void FillClickableRegionsAlpha(DWORD* pixels, int width, int height) {
    if (!pixels) return;
    if (!BeginInteractiveUse()) return;
    if (InterlockedCompareExchange(&g_regionCountVisible, 0, 0) <= 0) {
        EndInteractiveUse();
        return;
    }
    if (width <= 0 || height <= 0 ||
        (size_t)width > ((size_t)-1) / (size_t)height / sizeof(DWORD)) {
        EndInteractiveUse();
        return;
    }

    RECT regions[MAX_CLICKABLE_REGIONS];
    int regionCount = 0;

    EnterCriticalSection(&g_interactiveCS);

    regionCount = g_regionCount;
    if (regionCount > MAX_CLICKABLE_REGIONS) {
        regionCount = MAX_CLICKABLE_REGIONS;
    }
    for (int i = 0; i < regionCount; i++) {
        regions[i] = g_regions[i].rect;
    }

    LeaveCriticalSection(&g_interactiveCS);
    EndInteractiveUse();

    /* Fill each clickable region with minimal alpha so Windows sends mouse messages */
    for (int i = 0; i < regionCount; i++) {
        RECT* r = &regions[i];

        /* Clamp to buffer bounds */
        int left = r->left < 0 ? 0 : r->left;
        int top = r->top < 0 ? 0 : r->top;
        int right = r->right > width ? width : r->right;
        int bottom = r->bottom > height ? height : r->bottom;

        if (left >= right || top >= bottom) {
            continue;
        }

        /* Fill region pixels with minimal alpha if they're transparent */
        for (int y = top; y < bottom; y++) {
            for (int x = left; x < right; x++) {
                DWORD* pixel = &pixels[(size_t)y * (size_t)width + (size_t)x];
                /* Only set alpha if pixel is fully transparent */
                if ((*pixel & 0xFF000000) == 0) {
                    *pixel = 0x01000000;  /* Minimal alpha, invisible */
                }
            }
        }
    }
}

/* ============================================================================
 * Click Handling
 * ============================================================================ */

static BOOL WriteDisplaySourceContentAtomicW(const wchar_t* filePath,
                                             const char* content,
                                             DWORD contentSize) {
    if (!filePath || !content) return FALSE;

    wchar_t tempDir[MAX_PATH] = {0};
    wchar_t tempPath[MAX_PATH] = {0};
    const wchar_t* lastSlash = wcsrchr(filePath, L'\\');
    if (!lastSlash || lastSlash == filePath) {
        return FALSE;
    }
    size_t dirLen = (size_t)(lastSlash - filePath + 1);
    if (dirLen >= _countof(tempDir)) return FALSE;
    wmemcpy(tempDir, filePath, dirLen);
    tempDir[dirLen] = L'\0';

    if (GetTempFileNameW(tempDir, L"cto", 0, tempPath) == 0) return FALSE;

    HANDLE hFile = CreateFileW(tempPath, GENERIC_WRITE, CHECKBOX_OUTPUT_FILE_SHARE,
                               NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DeleteFileW(tempPath);
        return FALSE;
    }

    DWORD bytesWritten = 0;
    BOOL writeOk = WriteFile(hFile, content, contentSize, &bytesWritten, NULL) &&
                   bytesWritten == contentSize;
    if (writeOk && !FlushFileBuffers(hFile)) {
        writeOk = FALSE;
    }
    if (!CloseHandle(hFile)) {
        writeOk = FALSE;
    }

    if (!writeOk) {
        DeleteFileW(tempPath);
        return FALSE;
    }

    if (!MoveFileExW(tempPath, filePath,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tempPath);
        return FALSE;
    }

    return TRUE;
}

BOOL ToggleCheckboxInOutput(int index, HWND hwnd) {
    wchar_t filePath[MAX_PATH];
    if (!PluginData_GetDisplaySourcePath(filePath, MAX_PATH)) {
        return FALSE;
    }

    /* Read file content */
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, CHECKBOX_OUTPUT_FILE_SHARE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    LARGE_INTEGER fileSizeValue;
    if (!GetFileSizeEx(hFile, &fileSizeValue) ||
        fileSizeValue.QuadPart <= 0 ||
        fileSizeValue.QuadPart > CHECKBOX_OUTPUT_MAX_BYTES) {
        CloseHandle(hFile);
        return FALSE;
    }

    DWORD contentSize = (DWORD)fileSizeValue.QuadPart;
    char* content = (char*)malloc(contentSize + 1);
    if (!content) {
        CloseHandle(hFile);
        return FALSE;
    }

    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(hFile, content, contentSize, &bytesRead, NULL) &&
                  bytesRead == contentSize;
    CloseHandle(hFile);
    if (!readOk) {
        free(content);
        return FALSE;
    }
    content[bytesRead] = '\0';
    
    /* Find and toggle the checkbox at given index */
    int currentIndex = 0;
    char* p = content;
    const char* end = content + bytesRead;
    BOOL modified = FALSE;

    while (end - p >= 6) {
        /* Look for checkbox pattern: "- [ ] ", "+ [ ] ", or "* [ ] ". */
        if ((p[0] == '-' || p[0] == '+' || p[0] == '*') &&
            p[1] == ' ' && p[2] == '[' &&
            (p[3] == ' ' || p[3] == 'x' || p[3] == 'X') &&
            p[4] == ']' && p[5] == ' ') {
            
            if (currentIndex == index) {
                /* Toggle the checkbox */
                if (p[3] == ' ') {
                    p[3] = 'x';  /* Unchecked -> Checked */
                } else {
                    p[3] = ' ';  /* Checked -> Unchecked */
                }
                modified = TRUE;
                break;
            }
            currentIndex++;
        }
        p++;
    }

    if (modified) {
        if (WriteDisplaySourceContentAtomicW(filePath, content, bytesRead)) {
            LOG_INFO("Toggled checkbox %d in display source file", index);

            wchar_t* updatedText = Utf8ToWideAlloc(content);
            if (updatedText) {
                PluginData_SetPreviewTextWithSource(updatedText, filePath);
                free(updatedText);
            }

            /* Force redraw */
            if (IsValidMarkdownInteractiveWindow(hwnd)) {
                InvalidateRect(hwnd, NULL, TRUE);
            }
        } else {
            modified = FALSE;
        }
    }
    
    free(content);
    return modified;
}

static BOOL HandleRegionClick(const ClickableRegion* region, HWND hwnd) {
    if (!region) return FALSE;
    
    switch (region->type) {
        case CLICK_TYPE_LINK:
            if (region->url && IsSafeOpenUrlW(region->url)) {
                ShellExecuteW(NULL, L"open", region->url, NULL, NULL, SW_SHOWNORMAL);
                LOG_INFO("Opened link: %ls", region->url);
                return TRUE;
            }
            break;
            
        case CLICK_TYPE_CHECKBOX:
            return ToggleCheckboxInOutput(region->checkboxIndex, hwnd);
            
        default:
            break;
    }
    
    return FALSE;
}
