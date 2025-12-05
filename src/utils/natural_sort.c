/**
 * @file natural_sort.c
 * @brief Natural/numeric-aware string comparison implementation
 */

#include "utils/natural_sort.h"
#include <windows.h>
#include <wctype.h>
#include <ctype.h>
#include <string.h>

/**
 * @brief Natural string comparison with numeric ordering
 * @details Handles leading zeros and multi-digit numbers correctly
 */
int NaturalCompareW(const wchar_t* a, const wchar_t* b) {
    const wchar_t* pa = a;
    const wchar_t* pb = b;
    
    while (*pa && *pb) {
        if (iswdigit(*pa) && iswdigit(*pb)) {
            /* Skip leading zeros */
            const wchar_t* za = pa;
            while (*za == L'0') za++;
            const wchar_t* zb = pb;
            while (*zb == L'0') zb++;
            
            /* Compare zero count if both are all zeros */
            size_t leadA = (size_t)(za - pa);
            size_t leadB = (size_t)(zb - pb);
            if (leadA != leadB) {
                return (leadA > leadB) ? -1 : 1;
            }
            
            /* Find end of numeric sequences */
            const wchar_t* ea = za;
            while (iswdigit(*ea)) ea++;
            const wchar_t* eb = zb;
            while (iswdigit(*eb)) eb++;
            
            /* Compare by length first (longer = larger) */
            size_t lena = (size_t)(ea - za);
            size_t lenb = (size_t)(eb - zb);
            if (lena != lenb) {
                return (lena < lenb) ? -1 : 1;
            }
            
            /* Same length - compare lexicographically */
            int dcmp = wcsncmp(za, zb, lena);
            if (dcmp != 0) {
                return (dcmp < 0) ? -1 : 1;
            }
            
            /* Advance past numeric sequences */
            pa = ea;
            pb = eb;
            continue;
        }
        
        /* Case-insensitive character comparison */
        wchar_t ca = towlower(*pa);
        wchar_t cb = towlower(*pb);
        if (ca != cb) {
            return (ca < cb) ? -1 : 1;
        }
        
        pa++;
        pb++;
    }
    
    /* Handle strings of different lengths */
    if (*pa) return 1;
    if (*pb) return -1;
    return 0;
}

/**
 * @brief Natural comparison for narrow strings (ASCII/UTF-8)
 */
int NaturalCompareA(const char* a, const char* b) {
    const char* pa = a;
    const char* pb = b;
    
    while (*pa && *pb) {
        if (isdigit((unsigned char)*pa) && isdigit((unsigned char)*pb)) {
            const char* za = pa;
            while (*za == '0') za++;
            const char* zb = pb;
            while (*zb == '0') zb++;
            
            size_t leadA = (size_t)(za - pa);
            size_t leadB = (size_t)(zb - pb);
            if (leadA != leadB) {
                return (leadA > leadB) ? -1 : 1;
            }
            
            const char* ea = za;
            while (isdigit((unsigned char)*ea)) ea++;
            const char* eb = zb;
            while (isdigit((unsigned char)*eb)) eb++;
            
            size_t lena = (size_t)(ea - za);
            size_t lenb = (size_t)(eb - zb);
            if (lena != lenb) {
                return (lena < lenb) ? -1 : 1;
            }
            
            int dcmp = strncmp(za, zb, lena);
            if (dcmp != 0) {
                return (dcmp < 0) ? -1 : 1;
            }
            
            pa = ea;
            pb = eb;
            continue;
        }
        
        char ca = tolower((unsigned char)*pa);
        char cb = tolower((unsigned char)*pb);
        if (ca != cb) {
            return (ca < cb) ? -1 : 1;
        }
        
        pa++;
        pb++;
    }
    
    if (*pa) return 1;
    if (*pb) return -1;
    return 0;
}

/**
 * @brief Check if path has more directory components (wide char)
 */
static BOOL HasMoreDirsW(const wchar_t* path) {
    while (*path) {
        if (*path == L'\\' || *path == L'/') return TRUE;
        path++;
    }
    return FALSE;
}

/**
 * @brief Check if path has more directory components (narrow char)
 */
static BOOL HasMoreDirsA(const char* path) {
    while (*path) {
        if (*path == '\\' || *path == '/') return TRUE;
        path++;
    }
    return FALSE;
}

/**
 * @brief Get next path component (wide char)
 */
static const wchar_t* NextComponentW(const wchar_t* path) {
    while (*path && *path != L'\\' && *path != L'/') path++;
    if (*path) path++;
    return path;
}

/**
 * @brief Get next path component (narrow char)
 */
static const char* NextComponentA(const char* path) {
    while (*path && *path != '\\' && *path != '/') path++;
    if (*path) path++;
    return path;
}

/**
 * @brief Compare two path components with natural ordering (wide char)
 */
static int CompareComponentW(const wchar_t* a, const wchar_t* b) {
    while (*a && *b && *a != L'\\' && *a != L'/' && *b != L'\\' && *b != L'/') {
        if (iswdigit(*a) && iswdigit(*b)) {
            const wchar_t* za = a; while (*za == L'0') za++;
            const wchar_t* zb = b; while (*zb == L'0') zb++;
            const wchar_t* ea = za; while (iswdigit(*ea)) ea++;
            const wchar_t* eb = zb; while (iswdigit(*eb)) eb++;
            
            size_t lena = (size_t)(ea - za);
            size_t lenb = (size_t)(eb - zb);
            if (lena != lenb) return (lena < lenb) ? -1 : 1;
            
            int dcmp = wcsncmp(za, zb, lena);
            if (dcmp != 0) return (dcmp < 0) ? -1 : 1;
            
            a = ea; b = eb;
            continue;
        }
        
        wchar_t ca = towlower(*a);
        wchar_t cb = towlower(*b);
        if (ca != cb) return (ca < cb) ? -1 : 1;
        a++; b++;
    }
    
    BOOL aEnd = (*a == L'\0' || *a == L'\\' || *a == L'/');
    BOOL bEnd = (*b == L'\0' || *b == L'\\' || *b == L'/');
    if (aEnd && !bEnd) return -1;
    if (!aEnd && bEnd) return 1;
    return 0;
}

/**
 * @brief Compare two path components with natural ordering (narrow char)
 */
static int CompareComponentA(const char* a, const char* b) {
    while (*a && *b && *a != '\\' && *a != '/' && *b != '\\' && *b != '/') {
        if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
            const char* za = a; while (*za == '0') za++;
            const char* zb = b; while (*zb == '0') zb++;
            const char* ea = za; while (isdigit((unsigned char)*ea)) ea++;
            const char* eb = zb; while (isdigit((unsigned char)*eb)) eb++;
            
            size_t lena = (size_t)(ea - za);
            size_t lenb = (size_t)(eb - zb);
            if (lena != lenb) return (lena < lenb) ? -1 : 1;
            
            int dcmp = strncmp(za, zb, lena);
            if (dcmp != 0) return (dcmp < 0) ? -1 : 1;
            
            a = ea; b = eb;
            continue;
        }
        
        char ca = tolower((unsigned char)*a);
        char cb = tolower((unsigned char)*b);
        if (ca != cb) return (ca < cb) ? -1 : 1;
        a++; b++;
    }
    
    BOOL aEnd = (*a == '\0' || *a == '\\' || *a == '/');
    BOOL bEnd = (*b == '\0' || *b == '\\' || *b == '/');
    if (aEnd && !bEnd) return -1;
    if (!aEnd && bEnd) return 1;
    return 0;
}

/**
 * @brief Natural path comparison with files before folders (wide char)
 * @details At each directory level, files are sorted before subdirectories
 */
int NaturalPathCompareW(const wchar_t* a, const wchar_t* b) {
    while (*a && *b) {
        BOOL aIsFile = !HasMoreDirsW(a);
        BOOL bIsFile = !HasMoreDirsW(b);
        
        if (aIsFile && !bIsFile) return -1;
        if (!aIsFile && bIsFile) return 1;
        
        int cmp = CompareComponentW(a, b);
        if (cmp != 0) return cmp;
        
        a = NextComponentW(a);
        b = NextComponentW(b);
    }
    
    if (*a) return 1;
    if (*b) return -1;
    return 0;
}

/**
 * @brief Natural path comparison with files before folders (narrow char)
 */
int NaturalPathCompareA(const char* a, const char* b) {
    while (*a && *b) {
        BOOL aIsFile = !HasMoreDirsA(a);
        BOOL bIsFile = !HasMoreDirsA(b);
        
        if (aIsFile && !bIsFile) return -1;
        if (!aIsFile && bIsFile) return 1;
        
        int cmp = CompareComponentA(a, b);
        if (cmp != 0) return cmp;
        
        a = NextComponentA(a);
        b = NextComponentA(b);
    }
    
    if (*a) return 1;
    if (*b) return -1;
    return 0;
}
