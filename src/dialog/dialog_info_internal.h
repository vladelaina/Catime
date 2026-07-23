/**
 * @file dialog_info_internal.h
 * @brief Shared implementation details for informational dialogs.
 */

#ifndef CATIME_DIALOG_INFO_INTERNAL_H
#define CATIME_DIALOG_INFO_INTERNAL_H

#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "dialog/dialog_info.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_form_layout.h"
#include "dialog/dialog_markdown.h"
#include "dialog/dialog_modern.h"
#include "dialog/dialog_language.h"
#include "language.h"
#include "config.h"
#include "utils/win32_dynamic_loader.h"
#include "../../resource/resource.h"

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define FONT_LICENSE_PARENT_PROP L"Catime.FontLicense.Parent"
#define ABOUT_LINK_ORIG_PROC_PROP L"Catime.AboutLink.OrigProc"
#define ABOUT_LINK_HOVER_PROP L"Catime.AboutLink.Hover"
#define ABOUT_MODERN_ICON_SIZE_96 88
#define ABOUT_CONTENT_WIDTH_96 448

typedef struct {
    UINT controlId;
    const wchar_t* textCN;
    const wchar_t* textEN;
    const wchar_t* url;
} AboutLinkInfo;

extern AboutLinkInfo g_aboutLinkInfos[];
extern const size_t g_aboutLinkInfoCount;

BOOL DialogInfo_IsAboutLinkControlId(UINT controlId);
BOOL DialogInfo_IsAboutFooterLinkControlId(UINT controlId);
void DialogInfo_ConfigureAboutLinkControls(HWND hwndDlg);
void DialogInfo_PaintAboutLinkBackground(HWND hwndDlg,
                                         const DRAWITEMSTRUCT* drawItem);
void DialogInfo_LayoutAboutDialogControls(HWND hwndDlg);
void DialogInfo_ReloadAboutDialogIcon(HWND hwndDlg);

#endif /* CATIME_DIALOG_INFO_INTERNAL_H */
