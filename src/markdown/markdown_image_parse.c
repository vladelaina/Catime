/**
 * @file markdown_image_parse.c
 * @brief Markdown image counting, syntax parsing, and size conversion.
 */

#include "markdown_image_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

int CountMarkdownImages(const wchar_t* input) {
    if (!input) {
        return 0;
    }

    int count = 0;
    const wchar_t* p = input;
    while (*p) {
        if (*p == L'!' && *(p + 1) == L'[') {
            const wchar_t* bracketEnd = wcschr(p + 2, L']');
            if (!bracketEnd) {
                break;
            }
            if (*(bracketEnd + 1) != L'(') {
                p = bracketEnd + 1;
                continue;
            }
            const wchar_t* parenEnd = wcschr(bracketEnd + 2, L')');
            if (!parenEnd) {
                break;
            }
            if (count < INT_MAX) {
                count++;
            }
            p = parenEnd + 1;
            continue;
        }
        p++;
    }
    return count;
}

static BOOL ParsePositiveIntLimited(const wchar_t* text, size_t len,
                                    int* value) {
    int result = 0;
    if (!text || !value || len == 0) {
        return FALSE;
    }
    for (size_t i = 0; i < len; i++) {
        if (text[i] < L'0' || text[i] > L'9') {
            return FALSE;
        }
        int digit = (int)(text[i] - L'0');
        if (result > (INT_MAX - digit) / 10) {
            return FALSE;
        }
        result = result * 10 + digit;
    }
    *value = result;
    return result > 0;
}

void ParseImageSize(const wchar_t* sizeStr, size_t len,
                    int* width, int* height) {
    *width = 0;
    *height = 0;
    if (len == 0) {
        return;
    }

    const wchar_t* xPos = NULL;
    for (size_t i = 0; i < len; i++) {
        if (sizeStr[i] == L'x' || sizeStr[i] == L'X') {
            xPos = &sizeStr[i];
            break;
        }
    }

    if (xPos) {
        size_t wLen = (size_t)(xPos - sizeStr);
        if (wLen > 0 && wLen < 16) {
            ParsePositiveIntLimited(sizeStr, wLen, width);
        }
        size_t hLen = len - wLen - 1;
        if (hLen > 0 && hLen < 16) {
            ParsePositiveIntLimited(xPos + 1, hLen, height);
        }
    } else if (len < 16) {
        ParsePositiveIntLimited(sizeStr, len, width);
    }
}

BOOL ScaleIntToInt(int value, float scale, int* outValue) {
    if (!outValue || value <= 0 || scale <= 0.0f) {
        return FALSE;
    }

    double scaled = (double)value * (double)scale;
    if (!(scaled > 0.0) || scaled > (double)INT_MAX) {
        return FALSE;
    }
    *outValue = scaled < 1.0 ? 1 : (int)scaled;
    return TRUE;
}

BOOL ExtractMarkdownImage(const wchar_t** src, MarkdownImage* images,
                          int* imageCount, int imageCapacity,
                          int currentPos) {
    if (!src || !*src || !images || !imageCount ||
        *imageCount >= imageCapacity) {
        return FALSE;
    }

    const wchar_t* p = *src;
    if (*p != L'!' || *(p + 1) != L'[') {
        return FALSE;
    }
    p += 2;

    const wchar_t* bracketEnd = wcschr(p, L']');
    if (!bracketEnd || *(bracketEnd + 1) != L'(') {
        return FALSE;
    }

    size_t sizeLen = (size_t)(bracketEnd - p);
    int specWidth = 0;
    int specHeight = 0;
    if (sizeLen > 0) {
        ParseImageSize(p, sizeLen, &specWidth, &specHeight);
    }

    const wchar_t* pathStart = bracketEnd + 2;
    const wchar_t* pathEnd = wcschr(pathStart, L')');
    if (!pathEnd || pathEnd == pathStart) {
        return FALSE;
    }
    size_t pathLen = (size_t)(pathEnd - pathStart);
    if (pathLen == 0 || pathLen > MARKDOWN_IMAGE_PATH_MAX_CHARS) {
        return FALSE;
    }

    wchar_t* imagePath = (wchar_t*)malloc((pathLen + 1) * sizeof(wchar_t));
    if (!imagePath) {
        return FALSE;
    }
    wcsncpy(imagePath, pathStart, pathLen);
    imagePath[pathLen] = L'\0';

    MarkdownImage* image = &images[*imageCount];
    memset(image, 0, sizeof(MarkdownImage));
    image->imagePath = imagePath;
    image->specifiedWidth = specWidth;
    image->specifiedHeight = specHeight;
    image->startPos = currentPos;
    image->endPos = currentPos;
    image->isNetworkImage = IsNetworkUrl(imagePath);

    (*imageCount)++;
    *src = pathEnd + 1;
    return TRUE;
}
