/**
 * @file font_ttf_parser.c
 * @brief TTF/OTF binary parsing implementation
 */

#include "font/font_ttf_parser.h"
#include "utils/string_convert.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * TTF Binary Structures (Big-Endian)
 * ============================================================================ */

#pragma pack(push, 1)

typedef struct {
    WORD platformID;
    WORD encodingID;
    WORD languageID;
    WORD nameID;
    WORD length;
    WORD offset;
} NameRecord;

typedef struct {
    WORD format;
    WORD count;
    WORD stringOffset;
} NameTableHeader;

typedef struct {
    DWORD tag;
    DWORD checksum;
    DWORD offset;
    DWORD length;
} TableRecord;

typedef struct {
    DWORD sfntVersion;
    WORD numTables;
    WORD searchRange;
    WORD entrySelector;
    WORD rangeShift;
} FontDirectoryHeader;

#pragma pack(pop)

/* ============================================================================
 * Byte Order Conversion (Big-Endian ↔ Little-Endian)
 * ============================================================================ */

static inline WORD SwapWORD(WORD value) {
    return ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
}

static inline DWORD SwapDWORD(DWORD value) {
    return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) |
           ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24);
}

static BOOL RangeInFile(DWORD offset, DWORD length, DWORD fileSize) {
    return offset <= fileSize && length <= fileSize - offset;
}

static BOOL RangeInTable(DWORD relativeOffset, DWORD length, DWORD tableLength) {
    return relativeOffset <= tableLength && length <= tableLength - relativeOffset;
}

static BOOL SeekFileOffset(HANDLE hFile, DWORD offset) {
    LARGE_INTEGER distance;
    distance.QuadPart = offset;
    return SetFilePointerEx(hFile, distance, NULL, FILE_BEGIN);
}

/* ============================================================================
 * TTF Table Lookup
 * ============================================================================ */

/**
 * @brief Find specific table in TTF directory
 * @param hFile Open file handle
 * @param numTables Number of tables in directory
 * @param targetTag Table tag to find
 * @param outOffset Output: table offset
 * @param outLength Output: table length
 * @return TRUE if table found
 */
static BOOL FindTTFTable(HANDLE hFile, WORD numTables, DWORD targetTag, DWORD fileSize,
                         DWORD* outOffset, DWORD* outLength) {
    if (hFile == INVALID_HANDLE_VALUE || !outOffset || !outLength) return FALSE;

    for (WORD i = 0; i < numTables; i++) {
        TableRecord tableRecord;
        DWORD bytesRead;
        
        if (!ReadFile(hFile, &tableRecord, sizeof(TableRecord), &bytesRead, NULL) ||
            bytesRead != sizeof(TableRecord)) {
            return FALSE;
        }
        
        if (tableRecord.tag == targetTag) {
            *outOffset = SwapDWORD(tableRecord.offset);
            *outLength = SwapDWORD(tableRecord.length);
            return RangeInFile(*outOffset, *outLength, fileSize);
        }
    }

    return FALSE;
}

/* ============================================================================
 * String Parsing
 * ============================================================================ */

/**
 * @brief Parse font name from TTF name table entry
 * @param stringData Raw string data
 * @param dataLength Data length
 * @param isUnicode TRUE for UTF-16BE, FALSE for ASCII
 * @param outName Output buffer
 * @param outNameSize Buffer size
 */
static void ParseFontName(const char* stringData, size_t dataLength, BOOL isUnicode,
                          char* outName, size_t outNameSize) {
    if (!stringData || !outName || outNameSize == 0) return;
    
    if (isUnicode) {
        /* UTF-16 Big-Endian → UTF-16 Little-Endian → UTF-8 */
        WCHAR* unicodeStr = (WCHAR*)stringData;
        int numChars = (int)(dataLength / 2);
        
        /* Swap byte order */
        for (int i = 0; i < numChars; i++) {
            unicodeStr[i] = SwapWORD(unicodeStr[i]);
        }
        unicodeStr[numChars] = 0;
        
        /* Convert to UTF-8 */
        WideToUtf8(unicodeStr, outName, outNameSize);
    } else {
        /* ASCII → UTF-8 (direct copy) */
        size_t copyLen = (dataLength < outNameSize - 1) ? dataLength : outNameSize - 1;
        memcpy(outName, stringData, copyLen);
        outName[copyLen] = '\0';
    }
}

/* ============================================================================
 * Name Table Parsing
 * ============================================================================ */

/**
 * @brief Extract font family name from open TTF file
 * @param hFile Open file handle (positioned at start)
 * @param fontName Output buffer
 * @param fontNameSize Buffer size
 * @return TRUE on success
 */
static BOOL ExtractFontNameFromHandle(HANDLE hFile, char* fontName, size_t fontNameSize) {
    if (hFile == INVALID_HANDLE_VALUE || !fontName || fontNameSize == 0) return FALSE;

    LARGE_INTEGER fileSizeLarge;
    if (!GetFileSizeEx(hFile, &fileSizeLarge) ||
        fileSizeLarge.QuadPart < 0 ||
        fileSizeLarge.QuadPart > MAXDWORD) {
        return FALSE;
    }
    DWORD fileSize = (DWORD)fileSizeLarge.QuadPart;
    if (!RangeInFile(0, sizeof(FontDirectoryHeader), fileSize)) {
        return FALSE;
    }

    /* Read font directory header */
    FontDirectoryHeader fontHeader;
    DWORD bytesRead;
    
    if (!ReadFile(hFile, &fontHeader, sizeof(FontDirectoryHeader), &bytesRead, NULL) ||
        bytesRead != sizeof(FontDirectoryHeader)) {
        return FALSE;
    }

    fontHeader.numTables = SwapWORD(fontHeader.numTables);

    if ((DWORD)fontHeader.numTables > (MAXDWORD - sizeof(FontDirectoryHeader)) / sizeof(TableRecord)) {
        return FALSE;
    }
    DWORD tableDirectorySize = sizeof(FontDirectoryHeader) +
                               (DWORD)fontHeader.numTables * sizeof(TableRecord);
    if (!RangeInFile(0, tableDirectorySize, fileSize)) {
        return FALSE;
    }

    /* Locate 'name' table */
    DWORD nameTableOffset = 0, nameTableLength = 0;
    if (!FindTTFTable(hFile, fontHeader.numTables, TTF_NAME_TABLE_TAG, fileSize,
                     &nameTableOffset, &nameTableLength)) {
        return FALSE;
    }
    if (nameTableLength < sizeof(NameTableHeader) ||
        !RangeInFile(nameTableOffset, nameTableLength, fileSize)) {
        return FALSE;
    }

    /* Seek to name table */
    if (!SeekFileOffset(hFile, nameTableOffset)) {
        return FALSE;
    }
    
    /* Read name table header */
    NameTableHeader nameHeader;
    if (!ReadFile(hFile, &nameHeader, sizeof(NameTableHeader), &bytesRead, NULL) ||
        bytesRead != sizeof(NameTableHeader)) {
        return FALSE;
    }

    nameHeader.count = SwapWORD(nameHeader.count);
    nameHeader.stringOffset = SwapWORD(nameHeader.stringOffset);

    if ((DWORD)nameHeader.count > (MAXDWORD - sizeof(NameTableHeader)) / sizeof(NameRecord)) {
        return FALSE;
    }
    DWORD recordsSize = (DWORD)nameHeader.count * sizeof(NameRecord);
    if (!RangeInTable(sizeof(NameTableHeader), recordsSize, nameTableLength) ||
        nameHeader.stringOffset < sizeof(NameTableHeader) + recordsSize ||
        nameHeader.stringOffset > nameTableLength) {
        return FALSE;
    }

    /* Search for font family name (ID=1) */
    BOOL foundName = FALSE;
    WORD nameLength = 0, nameOffset = 0;
    BOOL isUnicode = FALSE;
    
    for (WORD i = 0; i < nameHeader.count; i++) {
        NameRecord nameRecord;
        
        if (!ReadFile(hFile, &nameRecord, sizeof(NameRecord), &bytesRead, NULL) ||
            bytesRead != sizeof(NameRecord)) {
            return FALSE;
        }
        
        /* Convert from big-endian */
        nameRecord.platformID = SwapWORD(nameRecord.platformID);
        nameRecord.encodingID = SwapWORD(nameRecord.encodingID);
        nameRecord.nameID = SwapWORD(nameRecord.nameID);
        nameRecord.length = SwapWORD(nameRecord.length);
        nameRecord.offset = SwapWORD(nameRecord.offset);

        /* Look for family name (ID=1) */
        if (nameRecord.nameID == TTF_NAME_ID_FAMILY) {
            DWORD stringDataLength = nameTableLength - nameHeader.stringOffset;
            if (!RangeInTable(nameRecord.offset, nameRecord.length, stringDataLength)) {
                continue;
            }

            /* Prefer Windows Unicode (platform 3, encoding 1) */
            if (nameRecord.platformID == 3 && nameRecord.encodingID == 1) {
                nameLength = nameRecord.length;
                nameOffset = nameRecord.offset;
                isUnicode = TRUE;
                foundName = TRUE;
                break;
            } else if (!foundName) {
                /* Fallback to first family name found */
                nameLength = nameRecord.length;
                nameOffset = nameRecord.offset;
                isUnicode = (nameRecord.platformID == 0);
                foundName = TRUE;
            }
        }
    }
    
    if (!foundName) return FALSE;

    /* Seek to string data */
    DWORD stringRelativeOffset = (DWORD)nameHeader.stringOffset + nameOffset;
    if (!RangeInTable(stringRelativeOffset, nameLength, nameTableLength) ||
        !RangeInFile(nameTableOffset + stringRelativeOffset, nameLength, fileSize)) {
        return FALSE;
    }
    DWORD stringDataOffset = nameTableOffset + stringRelativeOffset;

    if (!SeekFileOffset(hFile, stringDataOffset)) {
        return FALSE;
    }
    
    /* Read string data */
    if (nameLength > TTF_STRING_SAFETY_LIMIT) {
        nameLength = TTF_STRING_SAFETY_LIMIT;
    }
    
    char* stringBuffer = (char*)malloc(nameLength + 2);
    if (!stringBuffer) return FALSE;
    
    BOOL success = FALSE;
    if (ReadFile(hFile, stringBuffer, nameLength, &bytesRead, NULL) && 
        bytesRead == nameLength) {
        ParseFontName(stringBuffer, nameLength, isUnicode, fontName, fontNameSize);
        success = TRUE;
    }
    
    free(stringBuffer);
    return success;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

BOOL GetFontNameFromFile(const char* fontFilePath, char* fontName, size_t fontNameSize) {
    if (!fontFilePath || !fontName || fontNameSize == 0) return FALSE;
    
    /* Convert path to wide */
    wchar_t wFontPath[MAX_PATH];
    if (!Utf8ToWide(fontFilePath, wFontPath, MAX_PATH)) return FALSE;
    
    /* Open font file */
    HANDLE hFile = CreateFileW(wFontPath, GENERIC_READ, FILE_SHARE_READ, NULL, 
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    /* Parse and extract name */
    BOOL result = ExtractFontNameFromHandle(hFile, fontName, fontNameSize);
    
    CloseHandle(hFile);
    return result;
}

