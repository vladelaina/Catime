#ifndef FONT_H
#define FONT_H

#include <windows.h>
#include <stdbool.h>

int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD FontType, LPARAM lParam);

typedef struct {
    int menuId;
    int resourceId;
    const char* fontName;
} FontResource;

extern FontResource fontResources[];

extern const int FONT_RESOURCES_COUNT;

extern char FONT_FILE_NAME[100];

extern char FONT_INTERNAL_NAME[100];

extern char PREVIEW_FONT_NAME[100];

extern char PREVIEW_INTERNAL_NAME[100];

extern BOOL IS_PREVIEWING;

BOOL LoadFontFromResource(HINSTANCE hInstance, int resourceId);

BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName);

void WriteConfigFont(const char* font_file_name);

void ListAvailableFonts(void);

BOOL PreviewFont(HINSTANCE hInstance, const char* fontName);

void CancelFontPreview(void);

void ApplyFontPreview(void);

BOOL SwitchFont(HINSTANCE hInstance, const char* fontName);

#endif