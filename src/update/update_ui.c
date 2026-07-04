/**
 * @file update_ui.c
 * @brief Update dialogs and notification UI
 */
#include "update/update_internal.h"
#include "markdown/markdown_parser.h"
#include "dialog/dialog_procedure.h"
#include "dialog/dialog_language.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "log.h"
#include "utils/string_convert.h"
#include "utils/url_safety.h"
#include "../../resource/resource.h"
#include <strsafe.h>
#include <commctrl.h>
#include <stdlib.h>
#include <wchar.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

/* Global dialog handles for modeless dialogs */
static HWND g_hwndUpdateDialog = NULL;
static HWND g_hwndNoUpdateDialog = NULL;
static HWND g_hwndExitMsgDialog = NULL;

/* Store version info for modeless update dialog */
static VersionInfo g_updateVersionInfo = {0};
static char g_dialogCurrentVersion[64] = {0};
static char g_dialogLatestVersion[64] = {0};
static char g_dialogDownloadUrl[URL_BUFFER_SIZE] = {0};
static char g_dialogReleaseNotes[NOTES_BUFFER_SIZE] = {0};

/* Store download URL copy for async update */
static char g_downloadUrlCopy[512] = {0};
static SRWLOCK g_downloadUrlLock = SRWLOCK_INIT;

/* Release notes markdown state owned by the modeless update dialog. */
static wchar_t* g_notesDisplayText = NULL;
static MarkdownLink* g_notesLinks = NULL;
static int g_notesLinkCount = 0;
static MarkdownHeading* g_notesHeadings = NULL;
static int g_notesHeadingCount = 0;
static MarkdownStyle* g_notesStyles = NULL;
static int g_notesStyleCount = 0;
static MarkdownListItem* g_notesListItems = NULL;
static int g_notesListItemCount = 0;
static MarkdownBlockquote* g_notesBlockquotes = NULL;
static int g_notesBlockquoteCount = 0;
static MarkdownColorTag* g_notesColorTags = NULL;
static int g_notesColorTagCount = 0;
static MarkdownFontTag* g_notesFontTags = NULL;
static int g_notesFontTagCount = 0;
static int g_textHeight = 0;

typedef struct {
    HDC hdc;
    HBITMAP bitmap;
    HBITMAP oldBitmap;
    int width;
    int height;
} NotesPaintBuffer;

static NotesPaintBuffer g_notesPaintBuffer = {0};

#define UPDATE_NOTES_MAX_PAINT_PIXELS (4096u * 4096u)
#define UPDATE_NOTES_PAINT_SHRINK_THRESHOLD_MULTIPLIER 4u

/* Thin wrappers using utils/string_convert.h */
static inline wchar_t* LocalUtf8ToWideAlloc(const char* utf8Str) {
    return Utf8ToWideAlloc(utf8Str);
}

static void CopyUpdateString(char* dest, size_t destSize, const char* src) {
    if (!dest || destSize == 0) {
        return;
    }

    if (!src) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

static BOOL CopyPendingUpdateDownloadUrl(char* dest, size_t destSize) {
    if (!dest || destSize == 0) return FALSE;

    AcquireSRWLockShared(&g_downloadUrlLock);
    CopyUpdateString(dest, destSize, g_downloadUrlCopy);
    ReleaseSRWLockShared(&g_downloadUrlLock);

    return dest[0] != '\0';
}

static void ClearPendingUpdateDownloadUrl(void) {
    AcquireSRWLockExclusive(&g_downloadUrlLock);
    g_downloadUrlCopy[0] = '\0';
    ReleaseSRWLockExclusive(&g_downloadUrlLock);
}

static BOOL IsValidUpdateDialogParentWindow(HWND hwnd) {
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

static HWND GetUpdateDialogParent(HWND hwndDlg) {
    HWND hwndParent = hwndDlg ? GetParent(hwndDlg) : NULL;
    return IsValidUpdateDialogParentWindow(hwndParent) ? hwndParent : NULL;
}

static void CloseUpdateDialogInstance(DialogInstanceType type) {
    HWND hwndDlg = Dialog_GetInstance(type);
    if (hwndDlg && IsWindow(hwndDlg)) {
        DestroyWindow(hwndDlg);
    }
}

/** @brief Initialize dialog (center, localize) */
static void InitializeDialog(HWND hwndDlg, int dialogId) {
    ApplyDialogLanguage(hwndDlg, dialogId);
    MoveDialogToPrimaryScreen(hwndDlg);
}

/* Flag to indicate program should exit after exit dialog closes */
static BOOL g_shouldExitAfterDialog = FALSE;

/** @brief Exit notification dialog */
INT_PTR CALLBACK ExitMsgDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_EXIT_MSG, hwndDlg);
            g_hwndExitMsgDialog = hwndDlg;
            
            InitializeDialog(hwndDlg, IDD_EXIT_DIALOG);
            
            SetDlgItemTextW(hwndDlg, IDC_EXIT_TEXT, 
                GetLocalizedString(NULL, L"The application will exit now"));
            SetDlgItemTextW(hwndDlg, IDOK, 
                GetLocalizedString(NULL, L"OK"));
            SetWindowTextW(hwndDlg, 
                GetLocalizedString(NULL, L"Catime - Update Notice"));
            return TRUE;
        }
        
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
            
        case WM_DESTROY:
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_EXIT_MSG, hwndDlg);
            g_hwndExitMsgDialog = NULL;
            /* Exit program after dialog is destroyed */
            if (g_shouldExitAfterDialog) {
                g_shouldExitAfterDialog = FALSE;
                PostQuitMessage(0);
            }
            break;
    }
    return FALSE;
}

/** @brief Update available dialog */
static void ReleaseUpdateNotesPaintBuffer(void) {
    if (g_notesPaintBuffer.hdc && g_notesPaintBuffer.oldBitmap) {
        SelectObject(g_notesPaintBuffer.hdc, g_notesPaintBuffer.oldBitmap);
    }
    if (g_notesPaintBuffer.bitmap) {
        DeleteObject(g_notesPaintBuffer.bitmap);
    }
    if (g_notesPaintBuffer.hdc) {
        DeleteDC(g_notesPaintBuffer.hdc);
    }
    ZeroMemory(&g_notesPaintBuffer, sizeof(g_notesPaintBuffer));
}

static BOOL EnsureUpdateNotesPaintBuffer(HDC hdc, int width, int height, HDC* outHdc) {
    if (!hdc || !outHdc || width <= 0 || height <= 0) return FALSE;
    if ((size_t)width > (size_t)UPDATE_NOTES_MAX_PAINT_PIXELS / (size_t)height) {
        return FALSE;
    }

    if (g_notesPaintBuffer.hdc &&
        g_notesPaintBuffer.width >= width &&
        g_notesPaintBuffer.height >= height) {
        size_t requestedPixels = (size_t)width * (size_t)height;
        size_t cachedPixels = (size_t)g_notesPaintBuffer.width * (size_t)g_notesPaintBuffer.height;
        if (requestedPixels > 0 &&
            cachedPixels / UPDATE_NOTES_PAINT_SHRINK_THRESHOLD_MULTIPLIER <= requestedPixels) {
            *outHdc = g_notesPaintBuffer.hdc;
            return TRUE;
        }
    }

    if (g_notesPaintBuffer.hdc &&
        g_notesPaintBuffer.width == width &&
        g_notesPaintBuffer.height == height) {
        *outHdc = g_notesPaintBuffer.hdc;
        return TRUE;
    }

    HDC memDC = CreateCompatibleDC(hdc);
    if (!memDC) {
        return FALSE;
    }

    HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
    if (!bitmap) {
        DeleteDC(memDC);
        return FALSE;
    }

    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, bitmap);
    if (!oldBitmap) {
        DeleteObject(bitmap);
        DeleteDC(memDC);
        return FALSE;
    }

    ReleaseUpdateNotesPaintBuffer();

    g_notesPaintBuffer.hdc = memDC;
    g_notesPaintBuffer.bitmap = bitmap;
    g_notesPaintBuffer.oldBitmap = oldBitmap;
    g_notesPaintBuffer.width = width;
    g_notesPaintBuffer.height = height;

    *outHdc = memDC;
    return TRUE;
}

static void CleanupUpdateNotesState(HWND hwndDlg) {
    HWND hwndNotes = hwndDlg ? GetDlgItem(hwndDlg, IDC_UPDATE_NOTES) : NULL;
    if (hwndNotes) {
        if (GetCapture() == hwndNotes) {
            ReleaseCapture();
        }
        RemoveWindowSubclass(hwndNotes, NotesControlProc, 0);
        RemoveProp(hwndNotes, L"ScrollPos");
        RemoveProp(hwndNotes, L"ScrollMax");
        RemoveProp(hwndNotes, L"ScrollPage");
        RemoveProp(hwndNotes, L"ThumbDragging");
        RemoveProp(hwndNotes, L"DragStartY");
        RemoveProp(hwndNotes, L"DragStartScrollPos");
        RemoveProp(hwndNotes, L"ThumbHovered");
        RemoveProp(hwndNotes, L"MarkdownLinks");
        RemoveProp(hwndNotes, L"LinkCount");
    }

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
    g_textHeight = 0;

    ReleaseUpdateNotesPaintBuffer();
}

INT_PTR CALLBACK UpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            CleanupUpdateNotesState(NULL);
            Dialog_RegisterInstance(DIALOG_INSTANCE_UPDATE, hwndDlg);
            g_hwndUpdateDialog = hwndDlg;
            
            InitializeDialog(hwndDlg, IDD_UPDATE_DIALOG);
            g_textHeight = 0;

            const VersionInfo* versionInfo = &g_updateVersionInfo;
            if (versionInfo->currentVersion) {
                wchar_t* currentVerW = LocalUtf8ToWideAlloc(versionInfo->currentVersion);
                wchar_t* latestVerW = LocalUtf8ToWideAlloc(versionInfo->latestVersion);

                if (currentVerW && latestVerW) {
                    wchar_t displayText[256];
                    StringCbPrintfW(displayText, sizeof(displayText), L"%s %s\n%s %s",
                        GetLocalizedString(NULL, L"Current version:"), currentVerW,
                        GetLocalizedString(NULL, L"New version:"), latestVerW);
                    SetDlgItemTextW(hwndDlg, IDC_UPDATE_TEXT, displayText);
                }

                free(currentVerW);
                free(latestVerW);

                wchar_t* notesW = LocalUtf8ToWideAlloc(versionInfo->releaseNotes);
                if (notesW && notesW[0]) {
                    /* Wrap release notes with <md> tags for markdown parsing */
                    size_t notesLen = wcslen(notesW);
                    size_t bufSize = notesLen + 16;
                    wchar_t* wrappedNotes = (wchar_t*)malloc(bufSize * sizeof(wchar_t));
                    if (wrappedNotes) {
                        wcscpy_s(wrappedNotes, bufSize, L"<md>\n");
                        wcscat_s(wrappedNotes, bufSize, notesW);
                        wcscat_s(wrappedNotes, bufSize, L"\n</md>");
                        ParseMarkdownLinks(wrappedNotes, &g_notesDisplayText, &g_notesLinks, &g_notesLinkCount,
                                           &g_notesHeadings, &g_notesHeadingCount,
                                           &g_notesStyles, &g_notesStyleCount,
                                           &g_notesListItems, &g_notesListItemCount,
                                           &g_notesBlockquotes, &g_notesBlockquoteCount,
                                           &g_notesColorTags, &g_notesColorTagCount,
                                           &g_notesFontTags, &g_notesFontTagCount);
                        free(wrappedNotes);
                    } else {
                        ParseMarkdownLinks(notesW, &g_notesDisplayText, &g_notesLinks, &g_notesLinkCount,
                                           &g_notesHeadings, &g_notesHeadingCount,
                                           &g_notesStyles, &g_notesStyleCount,
                                           &g_notesListItems, &g_notesListItemCount,
                                           &g_notesBlockquotes, &g_notesBlockquoteCount,
                                           &g_notesColorTags, &g_notesColorTagCount,
                                           &g_notesFontTags, &g_notesFontTagCount);
                    }
                    free(notesW);
                } else {
                    free(notesW);
                    const wchar_t* noNotes = GetLocalizedString(NULL, L"No release notes available.");
                    ParseMarkdownLinks(noNotes, &g_notesDisplayText, &g_notesLinks, &g_notesLinkCount,
                                       &g_notesHeadings, &g_notesHeadingCount,
                                       &g_notesStyles, &g_notesStyleCount,
                                       &g_notesListItems, &g_notesListItemCount,
                                       &g_notesBlockquotes, &g_notesBlockquoteCount,
                                       &g_notesColorTags, &g_notesColorTagCount,
                                       &g_notesFontTags, &g_notesFontTagCount);
                }

                HWND hwndNotes = GetDlgItem(hwndDlg, IDC_UPDATE_NOTES);

                if (hwndNotes && g_notesDisplayText) {
                    SetWindowSubclass(hwndNotes, NotesControlProc, 0, 0);
                    SetProp(hwndNotes, L"ScrollPos", (HANDLE)0);
                    SetProp(hwndNotes, L"MarkdownLinks", (HANDLE)g_notesLinks);
                    SetProp(hwndNotes, L"LinkCount", (HANDLE)(INT_PTR)g_notesLinkCount);

                    HDC hdc = GetDC(hwndNotes);
                    if (hdc) {
                        HFONT hFont = (HFONT)SendMessage(hwndNotes, WM_GETFONT, 0, 0);
                        if (!hFont) hFont = GetStockObject(DEFAULT_GUI_FONT);
                        HFONT hOldFont = hFont ? (HFONT)SelectObject(hdc, hFont) : NULL;

                        RECT rect;
                        GetClientRect(hwndNotes, &rect);
                        rect.left += 5;
                        rect.top += 5;
                        rect.right -= MODERN_SCROLLBAR_WIDTH + MODERN_SCROLLBAR_MARGIN + 5;

                        RECT drawRect = rect;
                        g_textHeight = CalculateMarkdownTextHeight(hdc, g_notesDisplayText,
                                                                    g_notesHeadings, g_notesHeadingCount,
                                                                    g_notesStyles, g_notesStyleCount,
                                                                    g_notesListItems, g_notesListItemCount,
                                                                    g_notesBlockquotes, g_notesBlockquoteCount,
                                                                    drawRect);

                        if (hOldFont) {
                            SelectObject(hdc, hOldFont);
                        }
                        ReleaseDC(hwndNotes, hdc);

                        int clientHeight = rect.bottom - rect.top;
                        SetProp(hwndNotes, L"ScrollMax", (HANDLE)(INT_PTR)g_textHeight);
                        SetProp(hwndNotes, L"ScrollPage", (HANDLE)(INT_PTR)clientHeight);
                    }
                }

                SetDlgItemTextW(hwndDlg, IDYES, GetLocalizedString(NULL, L"Update Now"));
                SetDlgItemTextW(hwndDlg, IDNO, GetLocalizedString(NULL, L"Later"));
                SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Catime - Update Notice"));

                ShowWindow(GetDlgItem(hwndDlg, IDYES), SW_SHOW);
                ShowWindow(GetDlgItem(hwndDlg, IDNO), SW_SHOW);
                ShowWindow(GetDlgItem(hwndDlg, IDOK), SW_HIDE);
            }
            return TRUE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO) {
                /* Store the result and notify parent before destroying */
                HWND hwndParent = GetUpdateDialogParent(hwndDlg);
                int result = LOWORD(wParam);
                CleanupUpdateNotesState(hwndDlg);
                DestroyWindow(hwndDlg);
                
                /* If user chose to update, trigger the update process */
                if (result == IDYES && hwndParent) {
                    PostMessage(hwndParent, WM_DIALOG_UPDATE, IDYES, 0);
                }
                return TRUE;
            }
            break;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == IDC_UPDATE_NOTES) {
                HDC hdc = lpDrawItem->hDC;
                RECT rect = lpDrawItem->rcItem;
                RECT localRect = {0, 0, rect.right - rect.left, rect.bottom - rect.top};

                int paintWidth = localRect.right - localRect.left;
                int paintHeight = localRect.bottom - localRect.top;
                if (paintWidth <= 0 || paintHeight <= 0) {
                    return TRUE;
                }

                HDC hdcMem = NULL;
                if (!EnsureUpdateNotesPaintBuffer(hdc, paintWidth, paintHeight, &hdcMem)) {
                    return TRUE;
                }

                FillRect(hdcMem, &localRect, GetSysColorBrush(COLOR_BTNFACE));

                if (g_notesDisplayText) {
                    int scrollPos = (int)(INT_PTR)GetProp(lpDrawItem->hwndItem, L"ScrollPos");
                    int scrollMax = (int)(INT_PTR)GetProp(lpDrawItem->hwndItem, L"ScrollMax");
                    int scrollPage = (int)(INT_PTR)GetProp(lpDrawItem->hwndItem, L"ScrollPage");
                    BOOL thumbDragging = (BOOL)(INT_PTR)GetProp(lpDrawItem->hwndItem, L"ThumbDragging");
                    BOOL thumbHovered = (BOOL)(INT_PTR)GetProp(lpDrawItem->hwndItem, L"ThumbHovered");

                    SetBkMode(hdcMem, TRANSPARENT);

                    HFONT hFont = (HFONT)SendMessage(lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    HFONT hOldFont = hFont ? (HFONT)SelectObject(hdcMem, hFont) : NULL;

                    RECT drawRect = localRect;
                    drawRect.left += 5;
                    drawRect.top += 5;
                    drawRect.right -= MODERN_SCROLLBAR_WIDTH + MODERN_SCROLLBAR_MARGIN + 5;

                    POINT oldOrg = {0, 0};
                    BOOL viewportChanged = SetViewportOrgEx(hdcMem, 0, -scrollPos, &oldOrg);

                    RenderMarkdownText(hdcMem, g_notesDisplayText, g_notesLinks, g_notesLinkCount,
                                       g_notesHeadings, g_notesHeadingCount,
                                       g_notesStyles, g_notesStyleCount,
                                       g_notesListItems, g_notesListItemCount,
                                       g_notesBlockquotes, g_notesBlockquoteCount,
                                       drawRect, MARKDOWN_DEFAULT_LINK_COLOR, MARKDOWN_DEFAULT_TEXT_COLOR);

                    if (viewportChanged) {
                        SetViewportOrgEx(hdcMem, oldOrg.x, oldOrg.y, NULL);
                    }

                    if (hOldFont) {
                        SelectObject(hdcMem, hOldFont);
                    }

                    if (scrollMax > scrollPage) {
                        RECT thumbRect;
                        CalculateScrollbarThumbRect(localRect, scrollPos, scrollMax, scrollPage, &thumbRect);

                        if (!IsRectEmpty(&thumbRect)) {
                            COLORREF thumbColor = thumbDragging ? MODERN_SCROLLBAR_THUMB_DRAG_COLOR :
                                                  (thumbHovered ? MODERN_SCROLLBAR_THUMB_HOVER_COLOR :
                                                   MODERN_SCROLLBAR_THUMB_COLOR);
                            DrawRoundedRect(hdcMem, thumbRect, 4, thumbColor);
                        }
                    }
                }

                BitBlt(hdc, rect.left, rect.top, paintWidth, paintHeight, hdcMem, 0, 0, SRCCOPY);

                return TRUE;
            }
            break;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                CleanupUpdateNotesState(hwndDlg);
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            CleanupUpdateNotesState(hwndDlg);
            DestroyWindow(hwndDlg);
            return TRUE;

        case WM_DESTROY:
            CleanupUpdateNotesState(hwndDlg);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_UPDATE, hwndDlg);
            g_hwndUpdateDialog = NULL;
            break;
    }
    return FALSE;
}

/** @brief Update error dialog */
INT_PTR CALLBACK UpdateErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            Dialog_RegisterInstance(DIALOG_INSTANCE_UPDATE_ERROR, hwndDlg);
            InitializeDialog(hwndDlg, IDD_UPDATE_ERROR_DIALOG);
            if (lParam) {
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_ERROR_TEXT, (const wchar_t*)lParam);
            }
            return TRUE;
            
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            break;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
            
        case WM_DESTROY:
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_UPDATE_ERROR, hwndDlg);
            break;
    }
    return FALSE;
}

/** @brief No update dialog */
static char g_noUpdateVersion[64] = {0};

INT_PTR CALLBACK NoUpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_NO_UPDATE, hwndDlg);
            g_hwndNoUpdateDialog = hwndDlg;
            
            InitializeDialog(hwndDlg, IDD_NO_UPDATE_DIALOG);
            SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Catime - Update Notice"));
            
            if (g_noUpdateVersion[0]) {
                const wchar_t* baseText = GetDialogLocalizedString(IDD_NO_UPDATE_DIALOG, IDC_NO_UPDATE_TEXT);
                if (!baseText) baseText = L"You are already using the latest version!";
                
                wchar_t fullMessage[256];
                StringCbPrintfW(fullMessage, sizeof(fullMessage), L"%s\n%s %hs",
                    baseText,
                    GetLocalizedString(NULL, L"Current version:"),
                    g_noUpdateVersion);
                SetDlgItemTextW(hwndDlg, IDC_NO_UPDATE_TEXT, fullMessage);
            }
            return TRUE;
        }
        
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
            
        case WM_DESTROY:
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_NO_UPDATE, hwndDlg);
            g_hwndNoUpdateDialog = NULL;
            break;
    }
    return FALSE;
}

void ShowExitMessageDialog(HWND hwnd) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_EXIT_MSG)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_EXIT_MSG);
        SetForegroundWindow(existing);
        return;
    }

    if (!IsValidUpdateDialogParentWindow(hwnd)) {
        return;
    }

    HWND hwndDlg = CreateDialogW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_EXIT_DIALOG), hwnd, ExitMsgDlgProc);
    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

int ShowUpdateNotification(HWND hwnd, const char* currentVersion, const char* latestVersion,
                                  const char* downloadUrl, const char* releaseNotes) {
    if (!IsValidUpdateDialogParentWindow(hwnd)) {
        LOG_WARNING("Update dialog not shown: invalid parent window hwnd=0x%p", hwnd);
        return IDNO;
    }

    CloseUpdateDialogInstance(DIALOG_INSTANCE_NO_UPDATE);
    CloseUpdateDialogInstance(DIALOG_INSTANCE_UPDATE);

    CopyUpdateString(g_dialogCurrentVersion, sizeof(g_dialogCurrentVersion), currentVersion);
    CopyUpdateString(g_dialogLatestVersion, sizeof(g_dialogLatestVersion), latestVersion);
    CopyUpdateString(g_dialogDownloadUrl, sizeof(g_dialogDownloadUrl), downloadUrl);
    CopyUpdateString(g_dialogReleaseNotes, sizeof(g_dialogReleaseNotes), releaseNotes);

    /* Store version info for modeless dialog */
    g_updateVersionInfo.currentVersion = g_dialogCurrentVersion;
    g_updateVersionInfo.latestVersion = g_dialogLatestVersion;
    g_updateVersionInfo.downloadUrl = g_dialogDownloadUrl;
    g_updateVersionInfo.releaseNotes = g_dialogReleaseNotes;

    /* Store download URL copy for async update handling */
    AcquireSRWLockExclusive(&g_downloadUrlLock);
    CopyUpdateString(g_downloadUrlCopy, sizeof(g_downloadUrlCopy), downloadUrl);
    ReleaseSRWLockExclusive(&g_downloadUrlLock);

    HWND hwndDlg = CreateDialogW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_UPDATE_DIALOG),
                          hwnd, UpdateDlgProc);
    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    } else {
        LOG_WARNING("Update dialog creation failed: error=%lu", GetLastError());
    }

    /* Return IDNO since we can't block for result in modeless mode */
    return IDNO;
}

void ShowUpdateErrorDialog(HWND hwnd, const wchar_t* errorMsg) {
    /* Don't show blocking error dialog for update failures
     * Update checks should be transparent to the user - failures should be silent
     * Only log errors for troubleshooting purposes */
    (void)hwnd;
    
    /* Log the error for debugging */
    if (errorMsg) {
        char errorMsgUtf8[512] = {0};
        WideCharToMultiByte(CP_UTF8, 0, errorMsg, -1, errorMsgUtf8, sizeof(errorMsgUtf8), NULL, NULL);
        LOG_WARNING("Update check failed: %s", errorMsgUtf8);
    }
    
    /* Original blocking dialog implementation commented out:
    DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_UPDATE_ERROR_DIALOG), 
                   hwnd, UpdateErrorDlgProc, (LPARAM)errorMsg);
    */
}

void ShowNoUpdateDialog(HWND hwnd, const char* currentVersion) {
    CloseUpdateDialogInstance(DIALOG_INSTANCE_UPDATE);
    ClearPendingUpdateDownloadUrl();

    if (Dialog_IsOpen(DIALOG_INSTANCE_NO_UPDATE)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_NO_UPDATE);
        SetForegroundWindow(existing);
        return;
    }

    if (!IsValidUpdateDialogParentWindow(hwnd)) {
        LOG_WARNING("No-update dialog not shown: invalid parent window hwnd=0x%p", hwnd);
        return;
    }

    /* Store version for modeless dialog */
    CopyUpdateString(g_noUpdateVersion, sizeof(g_noUpdateVersion), currentVersion);

    HWND hwndDlg = CreateDialogW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_NO_UPDATE_DIALOG),
                   hwnd, NoUpdateDlgProc);
    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    } else {
        LOG_WARNING("No-update dialog creation failed: error=%lu", GetLastError());
    }
}

/**
 * @brief Get the pending update download URL
 * @return Pointer to stored download URL, or NULL if none
 */
const char* GetPendingUpdateDownloadUrl(void) {
#if defined(_MSC_VER)
    __declspec(thread) static char urlSnapshot[sizeof(g_downloadUrlCopy)] = {0};
#elif defined(__GNUC__)
    static __thread char urlSnapshot[sizeof(g_downloadUrlCopy)] = {0};
#else
    static char urlSnapshot[sizeof(g_downloadUrlCopy)] = {0};
#endif
    return CopyPendingUpdateDownloadUrl(urlSnapshot, sizeof(urlSnapshot)) ? urlSnapshot : NULL;
}

/**
 * @brief Trigger update download and exit
 * @param hwnd Parent window handle
 */
void TriggerUpdateDownload(HWND hwnd) {
    char url[sizeof(g_downloadUrlCopy)] = {0};

    if (CopyPendingUpdateDownloadUrl(url, sizeof(url))) {
        LOG_INFO("User chose to update now (from modeless dialog)");

        if (!IsSafeUpdateDownloadUrlA(url)) {
            LOG_ERROR("Blocked unsafe update URL: %s", url);
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(NULL, L"Unsafe download URL was blocked"));
            return;
        }

        /* Open browser with download URL */
        wchar_t* urlW = Utf8ToWideAlloc(url);
        if (urlW) {
            ShellExecuteW(NULL, L"open", urlW, NULL, NULL, SW_SHOWNORMAL);
            free(urlW);
        }
        
        /* Set flag to exit after dialog closes, then show exit message */
        g_shouldExitAfterDialog = TRUE;
        ShowExitMessageDialog(hwnd);
        /* PostQuitMessage will be called in WM_DESTROY of exit dialog */
    }
}

/* ============================================================================
 * Thread-safe update result storage
 * ============================================================================ */

static char g_storedCurrentVersion[64] = {0};
static char g_storedLatestVersion[64] = {0};
static char g_storedDownloadUrl[512] = {0};
static char g_storedReleaseNotes[NOTES_BUFFER_SIZE] = {0};
static BOOL g_storedHasUpdate = FALSE;
static SRWLOCK g_storedUpdateLock = SRWLOCK_INIT;

void StoreUpdateResult(BOOL hasUpdate, const char* currentVersion, const char* latestVersion,
                       const char* downloadUrl, const char* releaseNotes) {
    AcquireSRWLockExclusive(&g_storedUpdateLock);
    g_storedHasUpdate = hasUpdate;
    CopyUpdateString(g_storedCurrentVersion, sizeof(g_storedCurrentVersion), currentVersion);
    CopyUpdateString(g_storedLatestVersion, sizeof(g_storedLatestVersion), latestVersion);
    CopyUpdateString(g_storedDownloadUrl, sizeof(g_storedDownloadUrl), downloadUrl);
    CopyUpdateString(g_storedReleaseNotes, sizeof(g_storedReleaseNotes), releaseNotes);
    ReleaseSRWLockExclusive(&g_storedUpdateLock);
}

void ShowStoredUpdateDialog(HWND hwnd) {
    char currentVersion[64];
    char latestVersion[64];
    char downloadUrl[URL_BUFFER_SIZE];
    char* releaseNotes = (char*)malloc(NOTES_BUFFER_SIZE);
    const char* releaseNotesToShow = releaseNotes ? releaseNotes : "";
    BOOL hasUpdate = FALSE;

    AcquireSRWLockShared(&g_storedUpdateLock);
    hasUpdate = g_storedHasUpdate;
    CopyUpdateString(currentVersion, sizeof(currentVersion), g_storedCurrentVersion);
    CopyUpdateString(latestVersion, sizeof(latestVersion), g_storedLatestVersion);
    CopyUpdateString(downloadUrl, sizeof(downloadUrl), g_storedDownloadUrl);
    if (releaseNotes) {
        CopyUpdateString(releaseNotes, NOTES_BUFFER_SIZE, g_storedReleaseNotes);
    }
    ReleaseSRWLockShared(&g_storedUpdateLock);

    if (!hasUpdate || latestVersion[0] == '\0') {
        LOG_WARNING("Stored update dialog requested without complete update data");
        free(releaseNotes);
        return;
    }

    ShowUpdateNotification(hwnd, currentVersion, latestVersion, downloadUrl, releaseNotesToShow);
    free(releaseNotes);
}

void ShowStoredNoUpdateDialog(HWND hwnd) {
    char currentVersion[64];

    AcquireSRWLockShared(&g_storedUpdateLock);
    CopyUpdateString(currentVersion, sizeof(currentVersion), g_storedCurrentVersion);
    ReleaseSRWLockShared(&g_storedUpdateLock);

    ShowNoUpdateDialog(hwnd, currentVersion);
}
