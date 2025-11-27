/**
 * @file markdown_interactive.h
 * @brief Markdown interactive elements (clickable links and checkboxes)
 */

#ifndef MARKDOWN_INTERACTIVE_H
#define MARKDOWN_INTERACTIVE_H

#include <windows.h>

/* Maximum number of clickable regions to track */
#define MAX_CLICKABLE_REGIONS 64

/* Clickable region types */
typedef enum {
    CLICK_TYPE_NONE = 0,
    CLICK_TYPE_LINK,
    CLICK_TYPE_CHECKBOX
} ClickableType;

/* Clickable region info */
typedef struct {
    ClickableType type;
    RECT rect;              /* Screen coordinates */
    wchar_t* url;           /* For links: URL to open */
    int checkboxIndex;      /* For checkboxes: index in output file */
    BOOL isChecked;         /* For checkboxes: current state */
} ClickableRegion;

/**
 * @brief Initialize interactive markdown system
 */
void InitMarkdownInteractive(void);

/**
 * @brief Cleanup interactive markdown system
 */
void CleanupMarkdownInteractive(void);

/**
 * @brief Clear all clickable regions (called before each render)
 */
void ClearClickableRegions(void);

/**
 * @brief Add a link region
 * @param rect Region rectangle (window coordinates)
 * @param url Link URL
 */
void AddLinkRegion(const RECT* rect, const wchar_t* url);

/**
 * @brief Add a checkbox region
 * @param rect Region rectangle (window coordinates)
 * @param index Checkbox index in source
 * @param isChecked Current checked state
 */
void AddCheckboxRegion(const RECT* rect, int index, BOOL isChecked);

/**
 * @brief Update window position offset for all regions
 * @param windowX Window X position
 * @param windowY Window Y position
 */
void UpdateRegionPositions(int windowX, int windowY);

/**
 * @brief Check if a point is in any clickable region
 * @param pt Point in screen coordinates
 * @return Pointer to region or NULL
 */
const ClickableRegion* GetClickableRegionAt(POINT pt);

/**
 * @brief Handle click on a region
 * @param region The clicked region
 * @param hwnd Window handle for redraw
 * @return TRUE if handled
 */
BOOL HandleRegionClick(const ClickableRegion* region, HWND hwnd);

/**
 * @brief Toggle checkbox in output file
 * @param index Checkbox index
 * @param hwnd Window handle for redraw
 * @return TRUE if toggled successfully
 */
BOOL ToggleCheckboxInOutput(int index, HWND hwnd);

/**
 * @brief Check if there are any clickable regions
 * @return TRUE if there are clickable regions
 */
BOOL HasClickableRegions(void);

/**
 * @brief Fill clickable regions with minimal alpha for mouse hit-testing
 * @param pixels Pixel buffer (ARGB format)
 * @param width Buffer width
 * @param height Buffer height
 */
void FillClickableRegionsAlpha(DWORD* pixels, int width, int height);

#endif /* MARKDOWN_INTERACTIVE_H */
