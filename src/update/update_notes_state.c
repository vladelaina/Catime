#include "update_notes_internal.h"
#include "dialog/dialog_modern.h"
#include "language.h"
#include "utils/string_convert.h"
#include "../../resource/resource.h"

#include <commctrl.h>
#include <stdlib.h>
#include <wchar.h>

wchar_t* g_notesDisplayText;
MarkdownLink* g_notesLinks;
int g_notesLinkCount;
MarkdownHeading* g_notesHeadings;
int g_notesHeadingCount;
MarkdownStyle* g_notesStyles;
int g_notesStyleCount;
MarkdownListItem* g_notesListItems;
int g_notesListItemCount;
MarkdownBlockquote* g_notesBlockquotes;
int g_notesBlockquoteCount;
MarkdownColorTag* g_notesColorTags;
int g_notesColorTagCount;
MarkdownFontTag* g_notesFontTags;
int g_notesFontTagCount;
int g_notesTextHeight;

static void DetachNotesControl(HWND dialog) {
    HWND notes = dialog ? GetDlgItem(dialog, IDC_UPDATE_NOTES) : NULL;
    if (!notes) return;
    if (GetCapture() == notes) ReleaseCapture();
    RemoveWindowSubclass(notes, NotesControlProc, 0);
    static const wchar_t* properties[] = {
        L"ScrollPos", L"ScrollMax", L"ScrollPage", L"ThumbDragging",
        L"DragStartY", L"DragStartScrollPos", L"ThumbHovered",
        L"MarkdownLinks", L"LinkCount"
    };
    for (size_t i = 0; i < _countof(properties); i++) {
        RemovePropW(notes, properties[i]);
    }
}

void UpdateNotes_Cleanup(HWND dialog) {
    DetachNotesControl(dialog);
    FreeMarkdownLinks(g_notesLinks, g_notesLinkCount);
    g_notesLinks = NULL;
    g_notesLinkCount = 0;
    free(g_notesHeadings);
    g_notesHeadings = NULL;
    g_notesHeadingCount = 0;
    free(g_notesStyles);
    g_notesStyles = NULL;
    g_notesStyleCount = 0;
    free(g_notesListItems);
    g_notesListItems = NULL;
    g_notesListItemCount = 0;
    free(g_notesBlockquotes);
    g_notesBlockquotes = NULL;
    g_notesBlockquoteCount = 0;
    free(g_notesColorTags);
    g_notesColorTags = NULL;
    g_notesColorTagCount = 0;
    free(g_notesFontTags);
    g_notesFontTags = NULL;
    g_notesFontTagCount = 0;
    free(g_notesDisplayText);
    g_notesDisplayText = NULL;
    g_notesTextHeight = 0;
    UpdateNotes_ReleasePaintBuffer();
}

static void ParseNotes(const wchar_t* source) {
    ParseMarkdownLinks(
        source, &g_notesDisplayText, &g_notesLinks, &g_notesLinkCount,
        &g_notesHeadings, &g_notesHeadingCount,
        &g_notesStyles, &g_notesStyleCount,
        &g_notesListItems, &g_notesListItemCount,
        &g_notesBlockquotes, &g_notesBlockquoteCount,
        &g_notesColorTags, &g_notesColorTagCount,
        &g_notesFontTags, &g_notesFontTagCount);
}

static void ParseReleaseNotes(const char* releaseNotes) {
    wchar_t* notes = Utf8ToWideAlloc(releaseNotes);
    if (!notes || !notes[0]) {
        free(notes);
        ParseNotes(GetLocalizedString(NULL, L"No release notes available."));
        return;
    }

    size_t length = wcslen(notes);
    size_t wrappedCapacity = length + 16;
    wchar_t* wrapped =
        (wchar_t*)malloc(wrappedCapacity * sizeof(wchar_t));
    if (wrapped) {
        wcscpy_s(wrapped, wrappedCapacity, L"<md>\n");
        wcscat_s(wrapped, wrappedCapacity, notes);
        wcscat_s(wrapped, wrappedCapacity, L"\n</md>");
        ParseNotes(wrapped);
        free(wrapped);
    } else {
        ParseNotes(notes);
    }
    free(notes);
}

BOOL UpdateNotes_Initialize(HWND dialog, const char* releaseNotes) {
    UpdateNotes_Cleanup(NULL);
    ParseReleaseNotes(releaseNotes);
    HWND notes = GetDlgItem(dialog, IDC_UPDATE_NOTES);
    if (!notes || !g_notesDisplayText) return FALSE;

    SetWindowSubclass(notes, NotesControlProc, 0, 0);
    SetPropW(notes, L"ScrollPos", NULL);
    SetPropW(notes, L"MarkdownLinks", (HANDLE)g_notesLinks);
    SetPropW(notes, L"LinkCount", (HANDLE)(INT_PTR)g_notesLinkCount);
    UpdateNotes_Recalculate(dialog);
    return TRUE;
}

void UpdateNotes_Recalculate(HWND dialog) {
    HWND notes = dialog ? GetDlgItem(dialog, IDC_UPDATE_NOTES) : NULL;
    if (!notes || !g_notesDisplayText) return;

    HDC hdc = GetDC(notes);
    if (!hdc) return;
    HFONT font = (HFONT)SendMessageW(notes, WM_GETFONT, 0, 0);
    if (!font) font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HGDIOBJ previousFont = font ? SelectObject(hdc, font) : NULL;

    RECT rect = {0};
    GetClientRect(notes, &rect);
    int inset = DialogModern_Scale(DialogModern_GetDpi(dialog), 10);
    rect.left += inset;
    rect.top += inset;
    rect.right -= MODERN_SCROLLBAR_WIDTH + MODERN_SCROLLBAR_MARGIN + inset;
    rect.bottom -= inset;
    if (rect.right > rect.left && rect.bottom > rect.top) {
        g_notesTextHeight = CalculateMarkdownTextHeight(
            hdc, g_notesDisplayText, g_notesHeadings, g_notesHeadingCount,
            g_notesStyles, g_notesStyleCount, g_notesListItems,
            g_notesListItemCount, g_notesBlockquotes, g_notesBlockquoteCount,
            rect);
        int page = rect.bottom - rect.top;
        int position = (int)(INT_PTR)GetPropW(notes, L"ScrollPos");
        position = UpdateClampScrollPosition(
            position, g_notesTextHeight, page);
        SetPropW(notes, L"ScrollPos", (HANDLE)(INT_PTR)position);
        SetPropW(notes, L"ScrollMax", (HANDLE)(INT_PTR)g_notesTextHeight);
        SetPropW(notes, L"ScrollPage", (HANDLE)(INT_PTR)page);
    }

    if (previousFont) SelectObject(hdc, previousFont);
    ReleaseDC(notes, hdc);
    InvalidateRect(notes, NULL, TRUE);
}
