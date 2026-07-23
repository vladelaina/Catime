#include "update_notes_internal.h"
#include "dialog/dialog_modern.h"
#include "../../resource/resource.h"

#include <stdlib.h>

#define UPDATE_NOTES_MAX_PAINT_PIXELS (4096u * 4096u)
#define UPDATE_NOTES_SHRINK_THRESHOLD 4u

typedef struct {
    HDC hdc;
    HBITMAP bitmap;
    HBITMAP previousBitmap;
    int width;
    int height;
} NotesPaintBuffer;

static NotesPaintBuffer g_paintBuffer;

void UpdateNotes_ReleasePaintBuffer(void) {
    if (g_paintBuffer.hdc && g_paintBuffer.previousBitmap) {
        SelectObject(g_paintBuffer.hdc, g_paintBuffer.previousBitmap);
    }
    if (g_paintBuffer.bitmap) DeleteObject(g_paintBuffer.bitmap);
    if (g_paintBuffer.hdc) DeleteDC(g_paintBuffer.hdc);
    ZeroMemory(&g_paintBuffer, sizeof(g_paintBuffer));
}

static BOOL EnsurePaintBuffer(HDC target, int width, int height,
                              HDC* paintDc) {
    if (!target || !paintDc || width <= 0 || height <= 0) return FALSE;
    if ((size_t)width > UPDATE_NOTES_MAX_PAINT_PIXELS / (size_t)height) {
        return FALSE;
    }

    if (g_paintBuffer.hdc && g_paintBuffer.width >= width &&
        g_paintBuffer.height >= height) {
        size_t requested = (size_t)width * (size_t)height;
        size_t cached = (size_t)g_paintBuffer.width *
                        (size_t)g_paintBuffer.height;
        if (requested > 0 &&
            cached / UPDATE_NOTES_SHRINK_THRESHOLD <= requested) {
            *paintDc = g_paintBuffer.hdc;
            return TRUE;
        }
    }

    HDC memoryDc = CreateCompatibleDC(target);
    if (!memoryDc) return FALSE;
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    if (!bitmap) {
        DeleteDC(memoryDc);
        return FALSE;
    }
    HBITMAP previous = (HBITMAP)SelectObject(memoryDc, bitmap);
    if (!previous) {
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
        return FALSE;
    }

    UpdateNotes_ReleasePaintBuffer();
    g_paintBuffer.hdc = memoryDc;
    g_paintBuffer.bitmap = bitmap;
    g_paintBuffer.previousBitmap = previous;
    g_paintBuffer.width = width;
    g_paintBuffer.height = height;
    *paintDc = memoryDc;
    return TRUE;
}

static void DrawNotesContent(HWND dialog, const DRAWITEMSTRUCT* item,
                             HDC paintDc, RECT rect,
                             const DialogModernPalette* palette) {
    if (!g_notesDisplayText) return;
    int position = (int)(INT_PTR)GetPropW(item->hwndItem, L"ScrollPos");
    int maximum = (int)(INT_PTR)GetPropW(item->hwndItem, L"ScrollMax");
    int page = (int)(INT_PTR)GetPropW(item->hwndItem, L"ScrollPage");
    BOOL dragging = (BOOL)(INT_PTR)GetPropW(
        item->hwndItem, L"ThumbDragging");
    BOOL hovered = (BOOL)(INT_PTR)GetPropW(
        item->hwndItem, L"ThumbHovered");

    SetBkMode(paintDc, TRANSPARENT);
    HFONT font = (HFONT)SendMessageW(item->hwndItem, WM_GETFONT, 0, 0);
    if (!font) font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HGDIOBJ previousFont = font ? SelectObject(paintDc, font) : NULL;

    RECT drawRect = rect;
    int inset = DialogModern_Scale(DialogModern_GetDpi(dialog), 10);
    drawRect.left += inset;
    drawRect.top += inset;
    drawRect.right -= MODERN_SCROLLBAR_WIDTH + MODERN_SCROLLBAR_MARGIN + inset;
    drawRect.bottom -= inset;

    POINT previousOrigin = {0};
    BOOL originChanged = SetViewportOrgEx(
        paintDc, 0, -position, &previousOrigin);
    RenderMarkdownText(
        paintDc, g_notesDisplayText, g_notesLinks, g_notesLinkCount,
        g_notesHeadings, g_notesHeadingCount, g_notesStyles,
        g_notesStyleCount, g_notesListItems, g_notesListItemCount,
        g_notesBlockquotes, g_notesBlockquoteCount, drawRect,
        palette->accent, palette->text);
    if (originChanged) {
        SetViewportOrgEx(
            paintDc, previousOrigin.x, previousOrigin.y, NULL);
    }
    if (previousFont) SelectObject(paintDc, previousFont);

    if (maximum > page) {
        RECT thumb;
        CalculateScrollbarThumbRect(
            rect, position, maximum, page, &thumb);
        COLORREF thumbColor = dragging
            ? palette->accentHover
            : (hovered ? palette->accent : palette->border);
        DrawRoundedRect(paintDc, thumb, 4, thumbColor);
    }
}

BOOL UpdateNotes_Paint(HWND dialog, const DRAWITEMSTRUCT* item) {
    if (!dialog || !item || item->CtlID != IDC_UPDATE_NOTES) return FALSE;
    RECT targetRect = item->rcItem;
    RECT rect = {0, 0, targetRect.right - targetRect.left,
                 targetRect.bottom - targetRect.top};
    int width = rect.right;
    int height = rect.bottom;
    if (width <= 0 || height <= 0) return TRUE;

    HDC paintDc = NULL;
    if (!EnsurePaintBuffer(item->hDC, width, height, &paintDc)) return TRUE;
    DialogModernPalette palette;
    DialogModern_CopyPalette(dialog, &palette);
    HBRUSH background = CreateSolidBrush(palette.surface);
    if (background) {
        FillRect(paintDc, &rect, background);
        DeleteObject(background);
    }
    RECT panel = rect;
    InflateRect(&panel, -1, -1);
    DialogModern_DrawRoundedRect(
        paintDc, &panel, DialogModern_Scale(DialogModern_GetDpi(dialog), 14),
        palette.field, palette.border, 1);
    DrawNotesContent(dialog, item, paintDc, rect, &palette);
    BitBlt(item->hDC, targetRect.left, targetRect.top, width, height,
           paintDc, 0, 0, SRCCOPY);
    return TRUE;
}
