/**
 * @file natural_sort.c
 * @brief Natural/numeric-aware string comparison implementation
 */

#include "utils/natural_sort.h"
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

