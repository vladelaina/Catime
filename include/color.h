#ifndef COLOR_H
#define COLOR_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 颜色相关结构体定义
typedef struct {
    const char* hexColor;
} PredefinedColor;

typedef struct {
    const char* name;
    const char* hex;
} CSSColor;

// 全局变量声明
extern PredefinedColor* COLOR_OPTIONS;
extern size_t COLOR_OPTIONS_COUNT;
extern char CLOCK_TEXT_COLOR[10];
extern char PREVIEW_COLOR[10];
extern BOOL IS_COLOR_PREVIEWING;

// 颜色管理函数声明
void InitializeColorOptions(void);
void AddColorOption(const char* hexColor);
void ClearColorOptions(void);
BOOL IsColorExists(const char* hexColor);
void WriteConfigColor(const char* color_input);

// 颜色处理函数
int isValidColor(const char* input);
void normalizeColor(const char* input, char* output, size_t output_size);

// 颜色对话框相关函数
COLORREF ShowColorDialog(HWND hwnd);
INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);

#endif // COLOR_H