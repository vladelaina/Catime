#include "font/font_ttf_parser.h"
#include "font/font_ttf_internal.h"
#include "utils/string_convert.h"

BOOL GetFontNameFromFile(const char* fontFilePath, char* fontName,
                         size_t fontNameSize) {
    if (!fontFilePath || !fontName || fontNameSize == 0) return FALSE;
    fontName[0] = '\0';

    wchar_t widePath[MAX_PATH];
    if (!Utf8ToWide(fontFilePath, widePath, MAX_PATH)) return FALSE;

    HANDLE file = CreateFileW(widePath, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;

    BOOL result = FontTtf_ExtractName(file, fontName, fontNameSize);
    CloseHandle(file);
    return result;
}
