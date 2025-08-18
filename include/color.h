#ifndef COLOR_H
#define COLOR_H

#include <windows.h>

typedef struct {
    const char* hexColor;
} PredefinedColor;

typedef struct {
    const char* name;
    const char* hex;
} CSSColor;

extern PredefinedColor* COLOR_OPTIONS;
extern size_t COLOR_OPTIONS_COUNT;
extern char PREVIEW_COLOR[10];
extern BOOL IS_COLOR_PREVIEWING;
extern char CLOCK_TEXT_COLOR[10];

void InitializeDefaultLanguage(void);

void AddColorOption(const char* hexColor);

void ClearColorOptions(void);

void WriteConfigColor(const char* color_input);

void normalizeColor(const char* input, char* output, size_t output_size);

BOOL isValidColor(const char* input);

LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

BOOL IsColorExists(const char* hexColor);

COLORREF ShowColorDialog(HWND hwnd);

UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

#endif