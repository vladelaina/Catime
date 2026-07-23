    return TRUE;
}

static INT_PTR CALLBACK SimpleFontPickerProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lp;
            if (mis->CtlID == IDC_FONT_LIST_SIMPLE) {
                HDC hdc = GetDC(hdlg);
                if (!hdc) {
                    /* Fallback to default height if GetDC fails */
                    mis->itemHeight = 20;
                    return TRUE;
                }
                TEXTMETRIC tm = {0};
                if (!GetTextMetrics(hdc, &tm)) {
                    ReleaseDC(hdlg, hdc);
                    mis->itemHeight = 20;
                    return TRUE;
                }
                ReleaseDC(hdlg, hdc);
                mis->itemHeight = tm.tmHeight + 4;  /* Add padding */
                return TRUE;
            }
            return FALSE;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
            if (dis->CtlID != IDC_FONT_LIST_SIMPLE) return FALSE;

            if (dis->itemID == (UINT)-1) return TRUE;

            /* Get item text */
            wchar_t text[LF_FACESIZE];
            LRESULT textLen = SendMessageW(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);
            if (textLen == LB_ERR) {
                /* Failed to get text, skip drawing */
                return TRUE;
            }

            DialogModernPalette palette;
            DialogModern_CopyPalette(hdlg, &palette);
            BOOL selected = (dis->itemState & ODS_SELECTED) != 0;
            COLORREF bgColor = selected ? palette.accent : palette.field;
            COLORREF txtColor = selected ?
                (palette.highContrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) :
                                        RGB(0xFF, 0xFF, 0xFF)) :
                palette.text;
            COLORREF oldTextColor = GetTextColor(dis->hDC);
            int oldBkMode = SetBkMode(dis->hDC, TRANSPARENT);
            HBRUSH background = CreateSolidBrush(bgColor);
            if (background) {
                FillRect(dis->hDC, &dis->rcItem, background);
                DeleteObject(background);
            }
            if ((dis->itemState & ODS_FOCUS) && !selected) {
                RECT focusMark = dis->rcItem;
                focusMark.right = focusMark.left +
                                  DialogModern_Scale(
                                      DialogModern_GetDpi(hdlg), 2);
                HBRUSH accent = CreateSolidBrush(palette.accent);
                if (accent) {
                    FillRect(dis->hDC, &focusMark, accent);
                    DeleteObject(accent);
                }
            }

            /* Draw checkmark if this is the current font */
            int textLeft = dis->rcItem.left + 4;
            if ((int)dis->itemID == g_currentFontIndex) {
                SetTextColor(dis->hDC, txtColor);
                TextOutW(dis->hDC, dis->rcItem.left + 2, dis->rcItem.top + 2, L"✓", 1);
                textLeft += 16;  /* Make room for checkmark */
            }

            /* Draw text */
            SetTextColor(dis->hDC, txtColor);
            RECT textRect = dis->rcItem;
            textRect.left = textLeft;
            DrawTextW(dis->hDC, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            if (oldTextColor != CLR_INVALID) {
                SetTextColor(dis->hDC, oldTextColor);
            }
            SetBkMode(dis->hDC, oldBkMode);

            return TRUE;
        }
        case WM_INITDIALOG: {
            Dialog_InitializeInstance(DIALOG_INSTANCE_FONT_PICKER, hdlg);

            /* Start timer to maintain TOPMOST state across virtual desktop switches */
            if (!SetTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID, 500, NULL)) {
                LOG_WARNING("FontPicker: Failed to start topmost timer (error=%lu)",
                            GetLastError());
            }

            /* Initialize checkmark index at dialog start */
            g_currentFontIndex = -1;
            g_previewFontIndex = -1;

            SetWindowTextW(hdlg, GetLocalizedString(NULL, L"Select Font"));
            SetDlgItemTextW(hdlg, IDOK, GetLocalizedString(NULL, L"OK"));
            SetDlgItemTextW(hdlg, IDCANCEL, GetLocalizedString(NULL, L"Cancel"));
            SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                           GetLocalizedString(NULL, L"Font families (variants filtered):"));

            MoveDialogToPrimaryScreen(hdlg);
            SaveOriginalFont();

            HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
            if (!hwndList) {
                LOG_ERROR("FontPicker: Failed to get list control");
                return TRUE;
            }

            if (!CleanupCompletedFontEnumeration()) {
                StopFontEnumerationWithTimeout(FONT_ENUM_STOP_WAIT_MS);
            }
            if (!CleanupCompletedFontEnumeration()) {
                g_fontEnumRestartAfterCleanup = TRUE;
                SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                               GetLocalizedString(NULL, L"Font families (variants filtered):"));
                StartFontEnumPollTimer(hdlg);
                return TRUE;
            }

            g_fontEnumRestartAfterCleanup = FALSE;
            ResetFontMap();
            g_fontListReady = FALSE;
            EnableWindow(hwndList, FALSE);
            EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
            SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                           GetLocalizedString(NULL, L"Font families (variants filtered):"));

            StartFontEnumerationAsync(hdlg);

            return TRUE;
        }

        case WM_APP_FONT_ENUM_COMPLETE:
            if ((LONG)wp != InterlockedCompareExchange(&g_fontEnumGeneration, 0, 0) ||
                Dialog_GetInstance(DIALOG_INSTANCE_FONT_PICKER) != hdlg) {
                return TRUE;
            }
            KillTimer(hdlg, FONT_ENUM_POLL_TIMER_ID);
            if (g_fontListReady) {
                return TRUE;
            }
            if (!CleanupCompletedFontEnumeration()) {
                g_fontEnumRestartAfterCleanup = FALSE;
                StartFontEnumPollTimer(hdlg);
                return TRUE;
            }
            g_fontListReady = TRUE;
            PopulateFontList(hdlg);
            return TRUE;

        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                SendMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
                return TRUE;
            }
            break;

        case WM_TIMER:
            if (wp == FONT_PICKER_TOPMOST_TIMER_ID) {
                /* Re-apply TOPMOST to maintain visibility across virtual desktops */
                Dialog_ApplyTopmost(hdlg);
                return TRUE;
            }
            if (wp == FONT_ENUM_POLL_TIMER_ID) {
                if (CleanupCompletedFontEnumeration()) {
                    KillTimer(hdlg, FONT_ENUM_POLL_TIMER_ID);

                    if (!g_fontEnumRestartAfterCleanup) {
                        if (!g_fontListReady) {
                            g_fontListReady = TRUE;
                            PopulateFontList(hdlg);
                        }
                        return TRUE;
                    }

                    g_fontEnumRestartAfterCleanup = FALSE;
                    ResetFontMap();
                    g_fontListReady = FALSE;

                    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
                    if (hwndList) {
                        EnableWindow(hwndList, FALSE);
                    }
                    EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);

                    StartFontEnumerationAsync(hdlg);
                }
                return TRUE;
            }
            if (wp == FONT_ENUM_START_RETRY_TIMER_ID) {
                if (!g_fontEnumThread) {
                    StartFontEnumerationAsync(hdlg);
                }
                return TRUE;
            }
            break;

        case WM_COMMAND: {
            if (LOWORD(wp) == IDOK) {
                HWND hwndMain = FindCurrentProcessMainWindow();
                if (!CommitCurrentFontSelection(hwndMain)) {
                    RestoreOriginalFont();
                    return TRUE;
                }
                g_fontState.closeHandled = TRUE;
                KillTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID);
                DestroyWindow(hdlg);
                return TRUE;
            } else if (LOWORD(wp) == IDCANCEL) {
                RestoreOriginalFont();
                g_fontState.closeHandled = TRUE;
                KillTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID);
                DestroyWindow(hdlg);
                return TRUE;
            } else if (LOWORD(wp) == IDC_FONT_LIST_SIMPLE) {
                if (HIWORD(wp) == LBN_SELCHANGE) {
                    if (!g_fontListReady) {
                        return TRUE;
                    }
                    /* Real-time preview when selection changes */
                    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
                    int sel = (int)SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
                    if (sel != LB_ERR) {
                        LRESULT itemData = SendMessageW(hwndList, LB_GETITEMDATA, (WPARAM)sel, 0);
                        if (sel == g_previewFontIndex) {
                            return TRUE;
                        }
                        if (itemData >= 0 && itemData < g_fontMapCount) {
                            int fontIndex = (int)itemData;
                            if (PreviewFontInMainWindow(
                                    g_fontMap[fontIndex].fontName,
                                    g_fontMap[fontIndex].fontPath,
                                    hdlg, hwndList)) {
                                g_previewFontIndex = sel;
                            }
                        } else {
                            wchar_t fontName[LF_FACESIZE];
                            ZeroMemory(fontName, sizeof(fontName));
                            if (SendMessageW(hwndList, LB_GETTEXT, (WPARAM)sel,
                                             (LPARAM)fontName) != LB_ERR) {
                                if (PreviewFontInMainWindow(fontName, NULL,
                                                            hdlg, hwndList)) {
                                    g_previewFontIndex = sel;
                                }
                            }
                        }

                        /* Verify selection is still set after applying */
                        int newSel = (int)SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
                        if (newSel != sel) {
                            LOG_ERROR("FontPicker: ✗✗✗ SELECTION LOST! Was %d, now %d. Re-selecting.", sel, newSel);
                            SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)sel, 0);
                            SetFocus(hwndList);
                        }
                    } else {
                        LOG_WARNING("FontPicker: LBN_SELCHANGE but GETCURSEL returned LB_ERR!");
                    }
                    return TRUE;
                } else if (HIWORD(wp) == LBN_DBLCLK) {
                    /* Double-click acts as OK */
                    SendMessageW(hdlg, WM_COMMAND, IDOK, 0);
                    return TRUE;
                }
            }
            break;
        }

        case WM_CLOSE:
            KillTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID);
            SendMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
            return TRUE;
