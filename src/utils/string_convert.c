/**
 * @file string_convert.c
 * @brief UTF-8 ↔ UTF-16 conversion implementation
 */

#include "utils/string_convert.h"
#include <stdlib.h>

/* ============================================================================
 * Size calculation helpers
 * ============================================================================ */

size_t Utf8ToWideSize(const char* utf8) {
    if (!utf8) return 0;
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    return size > 0 ? (size_t)size : 0;
}

size_t WideToUtf8Size(const wchar_t* wide) {
    if (!wide) return 0;
    int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    return size > 0 ? (size_t)size : 0;
}

/* ============================================================================
 * Fixed-buffer conversions
 * ============================================================================ */

BOOL Utf8ToWide(const char* utf8, wchar_t* wide, size_t wideSize) {
    if (!utf8 || !wide || wideSize == 0) return FALSE;
    
    int result = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, (int)wideSize);
    return result > 0;
}

BOOL WideToUtf8(const wchar_t* wide, char* utf8, size_t utf8Size) {
    if (!wide || !utf8 || utf8Size == 0) return FALSE;
    
    int result = WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, (int)utf8Size, NULL, NULL);
    return result > 0;
}

/* ============================================================================
 * Dynamic allocation conversions
 * ============================================================================ */

wchar_t* Utf8ToWideAlloc(const char* utf8) {
    if (!utf8) return NULL;
    
    size_t size = Utf8ToWideSize(utf8);
    if (size == 0) return NULL;
    
    wchar_t* wide = (wchar_t*)malloc(size * sizeof(wchar_t));
    if (!wide) return NULL;
    
    if (!Utf8ToWide(utf8, wide, size)) {
        free(wide);
        return NULL;
    }
    
    return wide;
}

char* WideToUtf8Alloc(const wchar_t* wide) {
    if (!wide) return NULL;
    
    size_t size = WideToUtf8Size(wide);
    if (size == 0) return NULL;
    
    char* utf8 = (char*)malloc(size);
    if (!utf8) return NULL;
    
    if (!WideToUtf8(wide, utf8, size)) {
        free(utf8);
        return NULL;
    }
    
    return utf8;
}

