#ifndef HOTKEY_INTERNAL_H
#define HOTKEY_INTERNAL_H

#include "hotkey.h"
#include "../resource/resource.h"

#include <commctrl.h>
#include <windows.h>

#ifndef HOTKEYF_SHIFT
#define HOTKEYF_SHIFT 0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT 0x04
#endif

#define HOTKEY_DIALOG_SUBCLASS_ID 0xD142

typedef struct {
    int editCtrlId;
    int labelCtrlId;
    const wchar_t* labelCN;
    const wchar_t* labelEN;
} HotkeyMetadata;

typedef struct {
    HWND hwndParent;
    HBRUSH backgroundBrush;
    HBRUSH buttonBrush;
    BOOL hotkeysSuspended;
    BOOL reregisterPosted;
} HotkeyDialogState;

extern WORD g_dialogHotkeys[HOTKEY_COUNT];
extern const HotkeyMetadata g_hotkeyMetadata[HOTKEY_COUNT];

BOOL Hotkey_IsEditControl(DWORD controlId);
BOOL Hotkey_IsModifierKey(BYTE virtualKey);
BOOL Hotkey_ValidateAndSanitize(WORD* hotkey);
BOOL Hotkey_IsExistingEvent(WORD keyCombination);
void Hotkey_ClearDuplicates(HWND dialog, int currentControlId,
                            WORD newHotkey);
void Hotkey_LoadConfiguration(void);
void Hotkey_SetControlValues(HWND dialog);
void Hotkey_GetControlValues(HWND dialog);
void Hotkey_ValidateAll(void);
BOOL Hotkey_SaveConfiguration(void);

BOOL Hotkey_IsValidParent(HWND hwnd);
HWND Hotkey_GetDialogParent(HWND dialog);
HotkeyDialogState* Hotkey_GetDialogState(HWND dialog);
void Hotkey_SetDialogState(HWND dialog, HotkeyDialogState* state);
HotkeyDialogState* Hotkey_CreateDialogState(HWND parent);
void Hotkey_DestroyDialogState(HWND dialog, HotkeyDialogState* state);
BOOL Hotkey_EnsureBrush(HBRUSH* brush, COLORREF color);
void Hotkey_PostReregister(HWND dialog);

void Hotkey_InitializeLabels(HWND dialog);
void Hotkey_SetupControlSubclasses(HWND dialog);
void Hotkey_RemoveControlSubclasses(HWND dialog);

LRESULT CALLBACK HotkeyDialogSubclassProc(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData);

#endif /* HOTKEY_INTERNAL_H */
