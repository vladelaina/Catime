/**
 * @file dialog_modern_internal.h
 * @brief Cross-module declarations for modern resource dialog hosting.
 */

#ifndef DIALOG_MODERN_INTERNAL_H
#define DIALOG_MODERN_INTERNAL_H

#include "dialog_modern_types.h"

COLORREF ModernBlendColor(COLORREF from, COLORREF to, int toPercent);
BOOL ModernUpdateTitleHover(ModernDialogState* state, POINT point);
void ModernRefreshTitleHoverFromCursor(ModernDialogState* state);
ModernDialogState* ModernGetState(HWND hwnd);
BOOL DialogModern_CopyPalette(HWND hwnd, DialogModernPalette* palette);
BOOL DialogModern_IsAttached(HWND hwndDlg);
void ModernDeleteFonts(ModernDialogState* state);
void ModernDeleteBrushes(ModernDialogState* state);
void ModernRebuildResources(ModernDialogState* state);
int ModernTo96(UINT dpi, int value);
BOOL ModernEnsureControlCapacity(ModernDialogState* state, size_t count);
ModernControlKind ModernClassifyControl(const ModernDialogState* state,
                                        HWND hwnd, int id);
BOOL ModernWindowHasClass(HWND hwnd, const wchar_t* expected);
BOOL ModernIsDateTimeControl(const ModernControl* control);
BOOL CALLBACK ModernAttachDateTimeChild(HWND child, LPARAM data);
void ModernHideDateTimeSpinner(ModernControl* control);
BOOL ModernIsPrimaryButton(int id);
BOOL CALLBACK ModernCaptureChild(HWND child, LPARAM lParam);
int ModernControlCompareX(const void* left, const void* right);
void ModernMeasureText(HWND hwnd, HFONT font, SIZE* size);
void ModernAnalyzeLayout(ModernDialogState* state);
void ModernSetControlFont(const ModernDialogState* state,
                                 const ModernControl* control);
BOOL ModernApplyFieldRegionRaw(ModernControl* control, BOOL redraw);
void ModernApplyFieldRegion(ModernControl* control);
BOOL ModernIsCompactEdit(const ModernControl* control);
BOOL ModernIsNativeEdit(const ModernControl* control);
void ModernSetImeCompositionActive(HWND hwndEdit, BOOL active);
void ModernApplyEditLayout(ModernControl* control);
wchar_t* ModernCreateSingleLineText(const wchar_t* source,
                                           size_t length,
                                           BOOL* changed);
BOOL ModernPasteCompactEdit(HWND hwnd);
void ModernStyleControl(ModernDialogState* state,
                               ModernControl* control);
void ModernSetDefaultButton(ModernDialogState* state);
void ModernUpdateBodyScrollMetrics(ModernDialogState* state);
BOOL ModernBodyRegionMatches(const ModernControl* control,
                                    ModernBodyRegionMode mode,
                                    int width, int height,
                                    int top, int bottom, UINT dpi);
void ModernRememberBodyRegion(ModernControl* control,
                                     ModernBodyRegionMode mode,
                                     int width, int height,
                                     int top, int bottom, UINT dpi);
void ModernHideBodyControl(ModernControl* control, BOOL redraw);
void ModernShowBodyControl(ModernControl* control, BOOL redraw);
void ModernApplyBodyControlRegion(
    ModernDialogState* state, ModernControl* control, int y96,
    BOOL suppressRedraw);
void ModernLayoutBodyControls(ModernDialogState* state,
                                     BOOL suppressRedraw);
void ModernLayoutControls(ModernDialogState* state);
void ModernAttachComboList(ModernControl* control);
void ModernSetBodyScrollOffset(ModernDialogState* state, int offset96);
BOOL ModernGetScrollbarRects(const ModernDialogState* state,
                                    RECT* track, RECT* thumb);
void ModernDrawBodyScrollbar(const ModernDialogState* state, HDC hdc);
void ModernEnsureControlVisible(ModernControl* control);
BOOL ModernAppendCloseButton(ModernDialogState* state);
void ModernCommitClientSize(ModernDialogState* state,
                                   int width, int height);
void ModernSyncClientSizeFromWindow(ModernDialogState* state);
void ModernCenterAndResize(ModernDialogState* state);
BOOL ModernFinalize(ModernDialogState* state);
void ModernArmFirstShowGuard(ModernDialogState* state, BOOL allowCloak);
void ModernReleaseFirstShowGuard(ModernDialogState* state);
ModernControl* ModernFindControl(ModernDialogState* state, HWND hwnd);
void ModernDrawButton(ModernDialogState* state,
                             const DRAWITEMSTRUCT* item);
void ModernDrawComboItemContent(ModernControl* control, HDC hdc,
                                       const RECT* itemRect, UINT itemId,
                                       UINT itemState);
void ModernDrawComboItem(ModernDialogState* state,
                                const DRAWITEMSTRUCT* item);
void ModernDrawBodyGroups(ModernDialogState* state, HDC hdc);
void ModernDrawDialog(ModernDialogState* state, HDC hdc);
void ModernPaintBuffered(ModernDialogState* state, HDC target);
void ModernDrawFieldOutlineToDc(ModernControl* control, HDC hdc);
void ModernDrawFieldOutline(ModernControl* control);
void ModernPaintChoiceControl(ModernControl* control, HDC suppliedDc);
void ModernPaintSlider(ModernControl* control, HDC suppliedDc);
void ModernPaintCombo(ModernControl* control, HDC suppliedDc);
void ModernPaintInstruction(ModernControl* control, HDC suppliedDc);
void ModernPaintFeedback(ModernControl* control, HDC suppliedDc);
BOOL ModernGetDateTimeLayout(const ModernControl* control,
                                    ModernDateTimeLayout* layout);
int ModernDateTimePartMaximum(int part);
BOOL ModernReadDateTime(const ModernControl* control, SYSTEMTIME* value);
void ModernResetDateTimeInput(ModernControl* control);
BOOL ModernWriteDateTimePart(ModernControl* control, int part, int value);
BOOL ModernAdjustDateTimePart(ModernControl* control, int part,
                                     int delta);
int ModernDateTimeHitTest(const ModernDateTimeLayout* layout, POINT point);
void ModernSelectDateTimePart(ModernControl* control, int part);
BOOL ModernInputDateTimeDigit(ModernControl* control, int digit);
void ModernStopDateTimeRepeat(ModernControl* control);
void ModernStartDateTimeRepeat(ModernControl* control);
void ModernPaintDateTime(ModernControl* control, HDC suppliedDc);
void ModernTrackMouse(HWND hwnd);
void ModernTrackNonClientMouse(HWND hwnd);
BOOL ModernControlOwnsVerticalScroll(const ModernControl* control);
LRESULT CALLBACK ModernDateTimeChildSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData);
void ModernApplyComboListRegion(HWND hwnd, ModernControl* control);
void ModernInvalidateComboListItem(HWND hwnd, int itemIndex);
int ModernGetComboListVisibleItems(
    HWND hwnd, const ModernControl* control);
BOOL ModernPointInComboScrollbar(
    const ModernControl* control, const RECT* rect, POINT point);
BOOL ModernGetComboListScrollbarRects(
    HWND hwnd, ModernControl* control, RECT* track, RECT* thumb);
void ModernDrawComboListScrollbar(
    HWND hwnd, ModernControl* control, HDC hdc);
void ModernDrawComboListFrame(
    HWND hwnd, ModernControl* control, HDC hdc);
void ModernDrawComboList(HWND hwnd, ModernControl* control, HDC hdc);
void ModernPaintComboList(HWND hwnd, ModernControl* control,
                                 HDC suppliedDc);
void ModernRefreshComboListPointerState(
    HWND hwnd, ModernControl* control);
LRESULT CALLBACK ModernComboListSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData);
int ModernSliderPositionFromPoint(ModernControl* control, int x, int y);
BOOL ModernSetSliderPosition(ModernControl* control, int position,
                                    UINT notification);
BOOL ModernSetSliderFromPoint(ModernControl* control, int x, int y,
                                     UINT notification);
ModernControl* ModernFindWheelControl(ModernDialogState* state,
                                             POINT screenPoint);
BOOL ModernWindowOwnsFocus(HWND owner, HWND focused);
void ModernClearFocusedChild(ModernDialogState* state);
BOOL ModernPointIsPassiveContent(ModernDialogState* state,
                                        POINT point);
BOOL ModernCursorIsOverPassiveContent(ModernDialogState* state);
BOOL ModernHandleInteractiveWheel(ModernDialogState* state,
                                         WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ModernControlSubclassProc(HWND hwnd, UINT msg,
                                                  WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR subclassId,
                                                  DWORD_PTR refData);
void ModernHandleDpiChanged(ModernDialogState* state, WPARAM wParam,
                                   LPARAM lParam);
void ModernFreeState(ModernDialogState* state);
LRESULT CALLBACK ModernDialogSubclassProc(HWND hwnd, UINT msg,
                                                 WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR subclassId,
                                                 DWORD_PTR refData);
BOOL ModernHandleBodyWheel(ModernDialogState* state, WPARAM wParam);
BOOL ModernHandleBodyScrollTimer(ModernDialogState* state, WPARAM timerId);
void ModernBeginBodyScrollDrag(ModernDialogState* state, int pointerY);
void ModernQueueBodyScrollDrag(ModernDialogState* state, int offset96);
void ModernEndBodyScrollDrag(ModernDialogState* state);
void ModernDiscardBodyScrollDrag(ModernDialogState* state);
void ModernRefreshControlHover(ModernControl* control);
void ModernEndSliderDrag(ModernControl* control, BOOL commitPosition,
                                int x, int y);
void ModernRefreshBodyScrollbarHover(ModernDialogState* state);
LRESULT ModernHandleShowWindow(HWND hwnd, WPARAM wParam, LPARAM lParam,
                               ModernDialogState* state);
BOOL DialogModern_Attach(HWND hwndDlg, int dialogType);
void DialogModern_Refresh(HWND hwndDlg);
LRESULT ModernHandleControlPointerMessage(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    ModernControl* control, ModernDialogState* state, BOOL* handled);
LRESULT ModernHandleControlPaintMessage(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    ModernControl* control, ModernDialogState* state, BOOL* handled);
LRESULT ModernHandleControlKeyboardMessage(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    ModernControl* control, ModernDialogState* state, BOOL* handled);
LRESULT ModernHandleDialogColorMessage(
    UINT msg, WPARAM wParam, LPARAM lParam,
    ModernDialogState* state, BOOL* handled);

#endif /* DIALOG_MODERN_INTERNAL_H */
