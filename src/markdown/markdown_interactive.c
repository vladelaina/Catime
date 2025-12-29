/**
 * @file markdown_interactive.c
 * @brief Markdown interactive elements implementation
 */

#include "markdown/markdown_interactive.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <shellapi.h>

/* Plugin output file path */
#define PLUGIN_OUTPUT_FILENAME "output.txt"

/* ============================================================================
 * Global State
 * ============================================================================ */

static ClickableRegion g_regions[MAX_CLICKABLE_REGIONS];
static int g_regionCount = 0;
static int g_windowOffsetX = 0;
static int g_windowOffsetY = 0;
static CRITICAL_SECTION g_interactiveCS;
static volatile LONG g_initialized = 0;

/* ============================================================================
 * Initialization
 * ============================================================================ */

void InitMarkdownInteractive(void) {
    /* Use atomic operation to prevent double initialization */
    if (InterlockedCompareExchange(&g_initialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_interactiveCS);
        g_regionCount = 0;
    }
}

void CleanupMarkdownInteractive(void) {
    if (g_initialized != 1) return;
    ClearClickableRegions();
    DeleteCriticalSection(&g_interactiveCS);
    InterlockedExchange(&g_initialized, 0);
}

/* ============================================================================
 * Region Management
 * ============================================================================ */

void ClearClickableRegions(void) {
    if (g_initialized != 1) return;
    EnterCriticalSection(&g_interactiveCS);
    
    for (int i = 0; i < g_regionCount; i++) {
        if (g_regions[i].url) {
            free(g_regions[i].url);
            g_regions[i].url = NULL;
        }
    }
    g_regionCount = 0;
    
    LeaveCriticalSection(&g_interactiveCS);
}

void AddLinkRegion(const RECT* rect, const wchar_t* url) {
    if (g_initialized != 1 || !rect || !url) return;
    EnterCriticalSection(&g_interactiveCS);
    
    if (g_regionCount < MAX_CLICKABLE_REGIONS) {
        wchar_t* urlCopy = _wcsdup(url);
        if (urlCopy) {
            ClickableRegion* r = &g_regions[g_regionCount];
            r->type = CLICK_TYPE_LINK;
            r->rect = *rect;
            r->url = urlCopy;
            r->checkboxIndex = -1;
            r->isChecked = FALSE;
            g_regionCount++;
        }
    }
    
    LeaveCriticalSection(&g_interactiveCS);
}

void AddCheckboxRegion(const RECT* rect, int index, BOOL isChecked) {
    if (g_initialized != 1 || !rect) return;
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
    }
    
    LeaveCriticalSection(&g_interactiveCS);
}

void UpdateRegionPositions(int windowX, int windowY) {
    if (g_initialized != 1) return;
    EnterCriticalSection(&g_interactiveCS);
    
    /* Just store window position - regions are in window coords */
    g_windowOffsetX = windowX;
    g_windowOffsetY = windowY;
    
    LeaveCriticalSection(&g_interactiveCS);
}

const ClickableRegion* GetClickableRegionAt(POINT pt) {
    if (g_initialized != 1) return NULL;
    
    const ClickableRegion* result = NULL;
    EnterCriticalSection(&g_interactiveCS);
    
    /* Convert screen point to window-relative coords */
    POINT localPt = { pt.x - g_windowOffsetX, pt.y - g_windowOffsetY };
    
    for (int i = 0; i < g_regionCount; i++) {
        if (PtInRect(&g_regions[i].rect, localPt)) {
            result = &g_regions[i];
            break;
        }
    }
    
    LeaveCriticalSection(&g_interactiveCS);
    return result;
}

BOOL HasClickableRegions(void) {
    if (g_initialized != 1) return FALSE;
    BOOL result;
    EnterCriticalSection(&g_interactiveCS);
    result = (g_regionCount > 0);
    LeaveCriticalSection(&g_interactiveCS);
    return result;
}

void FillClickableRegionsAlpha(DWORD* pixels, int width, int height) {
    if (g_initialized != 1 || !pixels) return;
    EnterCriticalSection(&g_interactiveCS);
    
    /* Fill each clickable region with minimal alpha so Windows sends mouse messages */
    for (int i = 0; i < g_regionCount; i++) {
        RECT* r = &g_regions[i].rect;
        
        /* Clamp to buffer bounds */
        int left = r->left < 0 ? 0 : r->left;
        int top = r->top < 0 ? 0 : r->top;
        int right = r->right > width ? width : r->right;
        int bottom = r->bottom > height ? height : r->bottom;
        
        /* Fill region pixels with minimal alpha if they're transparent */
        for (int y = top; y < bottom; y++) {
            for (int x = left; x < right; x++) {
                DWORD* pixel = &pixels[y * width + x];
                /* Only set alpha if pixel is fully transparent */
                if ((*pixel & 0xFF000000) == 0) {
                    *pixel = 0x01000000;  /* Minimal alpha, invisible */
                }
            }
        }
    }
    
    LeaveCriticalSection(&g_interactiveCS);
}

/* ============================================================================
 * Click Handling
 * ============================================================================ */

static BOOL GetPluginOutputPath(char* buffer, size_t bufferSize) {
    DWORD result = ExpandEnvironmentStringsA(
        "%LOCALAPPDATA%\\Catime\\resources\\plugins\\" PLUGIN_OUTPUT_FILENAME,
        buffer, (DWORD)bufferSize);
    return (result > 0 && result < bufferSize);
}

BOOL ToggleCheckboxInOutput(int index, HWND hwnd) {
    char filePath[MAX_PATH];
    if (!GetPluginOutputPath(filePath, sizeof(filePath))) {
        return FALSE;
    }
    
    /* Read file content */
    FILE* f = fopen(filePath, "rb");
    if (!f) return FALSE;
    
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (fileSize <= 0 || fileSize > 1024 * 1024) {
        fclose(f);
        return FALSE;
    }
    
    char* content = (char*)malloc(fileSize + 1);
    if (!content) {
        fclose(f);
        return FALSE;
    }
    
    size_t bytesRead = fread(content, 1, fileSize, f);
    fclose(f);
    content[bytesRead] = '\0';
    
    /* Find and toggle the checkbox at given index */
    int currentIndex = 0;
    char* p = content;
    BOOL modified = FALSE;
    
    while (*p) {
        /* Look for checkbox pattern: "- [ ] " or "- [x] " or "- [X] " */
        if (p[0] == '-' && p[1] == ' ' && p[2] == '[' && 
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
        /* Write back to file */
        f = fopen(filePath, "wb");
        if (f) {
            fwrite(content, 1, bytesRead, f);
            fclose(f);
            LOG_INFO("Toggled checkbox %d in output file", index);
            
            /* Force redraw */
            if (hwnd) {
                InvalidateRect(hwnd, NULL, TRUE);
            }
        } else {
            modified = FALSE;
        }
    }
    
    free(content);
    return modified;
}

BOOL HandleRegionClick(const ClickableRegion* region, HWND hwnd) {
    if (!region) return FALSE;
    
    switch (region->type) {
        case CLICK_TYPE_LINK:
            if (region->url) {
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
