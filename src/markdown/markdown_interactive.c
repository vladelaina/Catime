#include "markdown/markdown_interactive.h"
#include "markdown/markdown_image.h"
#include "markdown_interactive_internal.h"
#include "utils/url_safety.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <shellapi.h>
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
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
BOOL MarkdownInteractive_IsValidWindow(HWND hwnd) {
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
void InitMarkdownInteractive(void) {
    AcquireSRWLockExclusive(&g_interactiveLifecycleLock);
    if (IsInteractiveInitialized()) {
        ReleaseSRWLockExclusive(&g_interactiveLifecycleLock);
        return;
    }
    InitializeMarkdownImage();
    if (InterlockedCompareExchange(&g_initialized, INTERACTIVE_CS_INITIALIZING, INTERACTIVE_CS_UNINITIALIZED) == INTERACTIVE_CS_UNINITIALIZED) {
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
    if (width <= 0 || height <= 0 || (size_t)width > ((size_t)-1) / (size_t)height / sizeof(DWORD)) {
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
    for (int i = 0; i < regionCount; i++) {
        RECT* r = &regions[i];
        int left = r->left < 0 ? 0 : r->left;
        int top = r->top < 0 ? 0 : r->top;
        int right = r->right > width ? width : r->right;
        int bottom = r->bottom > height ? height : r->bottom;
        if (left >= right || top >= bottom) {
            continue;
        }
        for (int y = top; y < bottom; y++) {
            for (int x = left; x < right; x++) {
                DWORD* pixel = &pixels[(size_t)y * (size_t)width + (size_t)x];
                if ((*pixel & 0xFF000000) == 0) {
                    *pixel = 0x01000000;  /* Minimal alpha, invisible */
                }
            }
        }
    }
}
static BOOL HandleRegionClick(const ClickableRegion* region, HWND hwnd) {
    if (!region) return FALSE;
    switch (region->type) {
        case CLICK_TYPE_LINK:
            if (region->url && IsSafeOpenUrlW(region->url)) {
                ShellExecuteW(NULL, L"open", region->url, NULL, NULL, SW_SHOWNORMAL);
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
