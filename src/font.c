/**
 * @file font.c
 * @brief Font loading with auto-recovery for reorganized files
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>
#include "../include/font.h"
#include "../include/config.h"
#include "../resource/resource.h"

/** TTF/OTF binary structures (big-endian) */
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

char FONT_FILE_NAME[100] = "%LOCALAPPDATA%\\Catime\\resources\\fonts\\Wallpoet Essence.ttf";
char FONT_INTERNAL_NAME[100];
char PREVIEW_FONT_NAME[100] = "";
char PREVIEW_INTERNAL_NAME[100] = "";
BOOL IS_PREVIEWING = FALSE;

static wchar_t CURRENT_LOADED_FONT_PATH[MAX_PATH] = {0};
static BOOL FONT_RESOURCE_LOADED = FALSE;

FontResource fontResources[] = {
    {IDR_FONT_RECMONO, "RecMonoCasual Nerd Font Mono Essence.ttf"},
    {IDR_FONT_DEPARTURE, "DepartureMono Nerd Font Propo Essence.ttf"},
    {IDR_FONT_TERMINESS, "Terminess Nerd Font Propo Essence.ttf"},
    {IDR_FONT_JACQUARD, "Jacquard 12 Essence.ttf"},
    {IDR_FONT_JACQUARDA, "Jacquarda Bastarda 9 Essence.ttf"},
    {IDR_FONT_PIXELIFY, "Pixelify Sans Medium Essence.ttf"},
    {IDR_FONT_RUBIK_BURNED, "Rubik Burned Essence.ttf"},
    {IDR_FONT_RUBIK_GLITCH, "Rubik Glitch Essence.ttf"},
    {IDR_FONT_RUBIK_MARKER_HATCH, "Rubik Marker Hatch Essence.ttf"},
    {IDR_FONT_RUBIK_PUDDLES, "Rubik Puddles Essence.ttf"},
    {IDR_FONT_WALLPOET, "Wallpoet Essence.ttf"},
    {IDR_FONT_PROFONT, "ProFont IIx Nerd Font Essence.ttf"},
    {IDR_FONT_DADDYTIME, "DaddyTimeMono Nerd Font Propo Essence.ttf"},
};

const int FONT_RESOURCES_COUNT = sizeof(fontResources) / sizeof(FontResource);

extern char CLOCK_TEXT_COLOR[];
extern void GetConfigPath(char* path, size_t maxLen);
extern void ReadConfig(void);
extern void FlushConfigToDisk(void);
extern BOOL WriteIniString(const char* section, const char* key, const char* value, const char* filePath);
extern int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD FontType, LPARAM lParam);

static BOOL Utf8ToWide(const char* utf8Str, wchar_t* wideStr, size_t wideSize) {
    if (!utf8Str || !wideStr || wideSize == 0) return FALSE;
    int result = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wideStr, (int)wideSize);
    return result > 0;
}

static BOOL WideToUtf8(const wchar_t* wideStr, char* utf8Str, size_t utf8Size) {
    if (!wideStr || !utf8Str || utf8Size == 0) return FALSE;
    int result = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, utf8Str, (int)utf8Size, NULL, NULL);
    return result > 0;
}

/** @return Filename portion, handles both slash types */
static const char* GetFilenameFromPath(const char* path) {
    if (!path) return NULL;
    const char* lastSlash = strrchr(path, '\\');
    const char* lastForwardSlash = strrchr(path, '/');
    const char* filename = (lastSlash > lastForwardSlash) ? lastSlash : lastForwardSlash;
    return filename ? (filename + 1) : path;
}

/** @return TRUE if path fits buffer */
static BOOL BuildFontConfigPath(const char* relativePath, char* outBuffer, size_t bufferSize) {
    if (!relativePath || !outBuffer || bufferSize == 0) return FALSE;
    int result = snprintf(outBuffer, bufferSize, "%s%s", FONT_FOLDER_PREFIX, relativePath);
    return result > 0 && result < (int)bufferSize;
}

static BOOL IsFontsFolderPath(const char* path) {
    if (!path) return FALSE;
    return _strnicmp(path, FONT_FOLDER_PREFIX, strlen(FONT_FOLDER_PREFIX)) == 0;
}

/** @return Relative portion after prefix, NULL if not a fonts folder path */
static const char* ExtractRelativePath(const char* fullConfigPath) {
    if (!IsFontsFolderPath(fullConfigPath)) return NULL;
    return fullConfigPath + strlen(FONT_FOLDER_PREFIX);
}

/**
 * @param ensureCreate TRUE to create directory if missing
 * @return TRUE on success
 */
static BOOL GetFontsFolderWide(wchar_t* outW, size_t size, BOOL ensureCreate) {
    if (!outW || size == 0) return FALSE;

    char configPathUtf8[MAX_PATH] = {0};
    GetConfigPath(configPathUtf8, MAX_PATH);

    wchar_t configPathW[MAX_PATH] = {0};
    if (!Utf8ToWide(configPathUtf8, configPathW, MAX_PATH)) return FALSE;

    wchar_t* lastSep = wcsrchr(configPathW, L'\\');
    if (!lastSep) return FALSE;

    size_t dirLen = (size_t)(lastSep - configPathW);
    if (dirLen + 1 >= size) return FALSE;
    wcsncpy(outW, configPathW, dirLen);
    outW[dirLen] = L'\0';

    if (wcslen(outW) + 1 + wcslen(L"resources\\fonts") + 1 >= size) return FALSE;
    wcscat(outW, L"\\resources\\fonts");

    if (ensureCreate) {
        SHCreateDirectoryExW(NULL, outW, NULL);
    }
    return TRUE;
}

/** @return TRUE on success */
static BOOL BuildFullFontPath(const char* relativePath, char* outAbsolutePathUtf8, size_t bufferSize) {
    if (!relativePath || !outAbsolutePathUtf8 || bufferSize == 0) return FALSE;

    wchar_t fontsFolderW[MAX_PATH] = {0};
    if (!GetFontsFolderWide(fontsFolderW, MAX_PATH, TRUE)) return FALSE;

    wchar_t relativeW[MAX_PATH] = {0};
    if (!Utf8ToWide(relativePath, relativeW, MAX_PATH)) return FALSE;

    wchar_t fullW[MAX_PATH] = {0};
    _snwprintf_s(fullW, MAX_PATH, _TRUNCATE, L"%s\\%s", fontsFolderW, relativeW);

    return WideToUtf8(fullW, outAbsolutePathUtf8, bufferSize);
}

/** @return TRUE if path is within fonts folder */
static BOOL CalculateRelativePath(const char* absolutePath, char* outRelativePath, size_t bufferSize) {
    if (!absolutePath || !outRelativePath || bufferSize == 0) return FALSE;

    wchar_t fontsFolderW[MAX_PATH] = {0};
    if (!GetFontsFolderWide(fontsFolderW, MAX_PATH, TRUE)) return FALSE;

    char fontsFolderUtf8[MAX_PATH];
    if (!WideToUtf8(fontsFolderW, fontsFolderUtf8, MAX_PATH)) return FALSE;

    size_t prefixLen = strlen(fontsFolderUtf8);
    if (prefixLen > 0 && fontsFolderUtf8[prefixLen - 1] != '\\') {
        if (prefixLen + 1 < MAX_PATH) {
            fontsFolderUtf8[prefixLen] = '\\';
            fontsFolderUtf8[prefixLen + 1] = '\0';
            prefixLen += 1;
        }
    }

    if (_strnicmp(absolutePath, fontsFolderUtf8, prefixLen) != 0) {
        return FALSE;
    }

    const char* relativePart = absolutePath + prefixLen;
    strncpy(outRelativePath, relativePart, bufferSize - 1);
    outRelativePath[bufferSize - 1] = '\0';
    return TRUE;
}

/**
 * Auto-recover font path after user reorganization
 * @param fontFileName Original filename
 * @param pathInfo Output: all path variants
 * @return TRUE if found in any subfolder
 * @note Searches recursively when direct path fails
 */
static BOOL AutoFixFontPath(const char* fontFileName, FontPathInfo* pathInfo) {
    if (!fontFileName || !pathInfo) return FALSE;

    memset(pathInfo, 0, sizeof(FontPathInfo));
    strncpy(pathInfo->fileName, GetFilenameFromPath(fontFileName), sizeof(pathInfo->fileName) - 1);

    if (!FindFontInFontsFolder(pathInfo->fileName, pathInfo->absolutePath, sizeof(pathInfo->absolutePath))) {
        return FALSE;
    }

    if (!CalculateRelativePath(pathInfo->absolutePath, pathInfo->relativePath, sizeof(pathInfo->relativePath))) {
        return FALSE;
    }

    if (!BuildFontConfigPath(pathInfo->relativePath, pathInfo->configPath, sizeof(pathInfo->configPath))) {
        return FALSE;
    }

    pathInfo->isValid = TRUE;
    return TRUE;
}

/** @param shouldSave TRUE to flush immediately, FALSE to defer */
static void UpdateFontConfig(const FontPathInfo* pathInfo, BOOL shouldSave) {
    if (!pathInfo || !pathInfo->isValid) return;

    strncpy(FONT_FILE_NAME, pathInfo->configPath, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';

    if (shouldSave) {
        WriteConfigFont(pathInfo->relativePath, FALSE);
        FlushConfigToDisk();
    }
}

/**
 * Recursive case-insensitive font search
 * @return TRUE on first match
 * @note Assumes font filenames are unique within fonts folder
 */
static BOOL SearchFontRecursiveW(const wchar_t* folderPathW, const wchar_t* targetFileW, 
                                 wchar_t* resultPathW, size_t resultCapacity) {
    if (!folderPathW || !targetFileW || !resultPathW) return FALSE;

    wchar_t searchPathW[MAX_PATH] = {0};
    _snwprintf_s(searchPathW, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);

    WIN32_FIND_DATAW findDataW;
    HANDLE hFind = FindFirstFileW(searchPathW, &findDataW);
    if (hFind == INVALID_HANDLE_VALUE) return FALSE;

    BOOL found = FALSE;
    do {
        if (wcscmp(findDataW.cFileName, L".") == 0 || wcscmp(findDataW.cFileName, L"..") == 0) {
            continue;
        }

        wchar_t fullItemPathW[MAX_PATH] = {0};
        _snwprintf_s(fullItemPathW, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPathW, findDataW.cFileName);

        if (!(findDataW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (_wcsicmp(findDataW.cFileName, targetFileW) == 0) {
                wcsncpy(resultPathW, fullItemPathW, resultCapacity - 1);
                resultPathW[resultCapacity - 1] = L'\0';
                found = TRUE;
                break;
            }
        } else {
            if (SearchFontRecursiveW(fullItemPathW, targetFileW, resultPathW, resultCapacity)) {
                found = TRUE;
                break;
            }
        }
    } while (FindNextFileW(hFind, &findDataW));
    FindClose(hFind);
    return found;
}

BOOL FindFontInFontsFolder(const char* fontFileName, char* foundPath, size_t foundPathSize) {
    if (!fontFileName || !foundPath || foundPathSize == 0) return FALSE;

    wchar_t fontsFolderW[MAX_PATH] = {0};
    if (!GetFontsFolderWide(fontsFolderW, MAX_PATH, TRUE)) return FALSE;

    wchar_t targetFileW[MAX_PATH] = {0};
    if (!Utf8ToWide(fontFileName, targetFileW, MAX_PATH)) return FALSE;

    wchar_t resultPathW[MAX_PATH] = {0};
    if (!SearchFontRecursiveW(fontsFolderW, targetFileW, resultPathW, MAX_PATH)) return FALSE;

    return WideToUtf8(resultPathW, foundPath, foundPathSize);
}

/** TTF files use big-endian byte order */
static inline WORD SwapWORD(WORD value) {
    return ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
}

static inline DWORD SwapDWORD(DWORD value) {
    return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) | 
           ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24);
}

static BOOL FindTTFTable(HANDLE hFile, WORD numTables, DWORD targetTag, 
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
            return TRUE;
        }
    }
    return FALSE;
}

/** @param isUnicode TRUE for UTF-16BE, FALSE for ASCII */
static void ParseFontName(const char* stringData, size_t dataLength, BOOL isUnicode,
                          char* outName, size_t outNameSize) {
    if (!stringData || !outName || outNameSize == 0) return;

    if (isUnicode) {
        WCHAR* unicodeStr = (WCHAR*)stringData;
        int numChars = (int)(dataLength / 2);

        for (int i = 0; i < numChars; i++) {
            unicodeStr[i] = SwapWORD(unicodeStr[i]);
        }
        unicodeStr[numChars] = 0;

        WideCharToMultiByte(CP_UTF8, 0, unicodeStr, -1, outName, (int)outNameSize, NULL, NULL);
    } else {
        size_t copyLen = (dataLength < outNameSize - 1) ? dataLength : outNameSize - 1;
        memcpy(outName, stringData, copyLen);
        outName[copyLen] = '\0';
    }
}

/**
 * Extract font family name from TTF "name" table
 * @return TRUE on success
 * @note Prefers Windows Unicode (platform 3, encoding 1)
 */
static BOOL ExtractFontNameFromHandle(HANDLE hFile, char* fontName, size_t fontNameSize) {
    if (hFile == INVALID_HANDLE_VALUE || !fontName || fontNameSize == 0) return FALSE;

    FontDirectoryHeader fontHeader;
    DWORD bytesRead;
    if (!ReadFile(hFile, &fontHeader, sizeof(FontDirectoryHeader), &bytesRead, NULL) ||
        bytesRead != sizeof(FontDirectoryHeader)) {
        return FALSE;
    }

    fontHeader.numTables = SwapWORD(fontHeader.numTables);

    DWORD nameTableOffset = 0, nameTableLength = 0;
    if (!FindTTFTable(hFile, fontHeader.numTables, TTF_NAME_TABLE_TAG, 
                     &nameTableOffset, &nameTableLength)) {
        return FALSE;
    }

    if (SetFilePointer(hFile, nameTableOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        return FALSE;
    }

    NameTableHeader nameHeader;
    if (!ReadFile(hFile, &nameHeader, sizeof(NameTableHeader), &bytesRead, NULL) ||
        bytesRead != sizeof(NameTableHeader)) {
        return FALSE;
    }

    nameHeader.count = SwapWORD(nameHeader.count);
    nameHeader.stringOffset = SwapWORD(nameHeader.stringOffset);

    BOOL foundName = FALSE;
    WORD nameLength = 0, nameOffset = 0;
    BOOL isUnicode = FALSE;

    /** Name ID 1 = font family name */
    for (WORD i = 0; i < nameHeader.count; i++) {
        NameRecord nameRecord;
        if (!ReadFile(hFile, &nameRecord, sizeof(NameRecord), &bytesRead, NULL) ||
            bytesRead != sizeof(NameRecord)) {
            return FALSE;
        }

        nameRecord.platformID = SwapWORD(nameRecord.platformID);
        nameRecord.encodingID = SwapWORD(nameRecord.encodingID);
        nameRecord.nameID = SwapWORD(nameRecord.nameID);
        nameRecord.length = SwapWORD(nameRecord.length);
        nameRecord.offset = SwapWORD(nameRecord.offset);

        if (nameRecord.nameID == TTF_NAME_ID_FAMILY) {
            if (nameRecord.platformID == 3 && nameRecord.encodingID == 1) {
                nameLength = nameRecord.length;
                nameOffset = nameRecord.offset;
                isUnicode = TRUE;
                foundName = TRUE;
                break;
            } else if (!foundName) {
                nameLength = nameRecord.length;
                nameOffset = nameRecord.offset;
                isUnicode = (nameRecord.platformID == 0);
                foundName = TRUE;
            }
        }
    }

    if (!foundName) return FALSE;

    DWORD stringDataOffset = nameTableOffset + sizeof(NameTableHeader) + 
                            nameHeader.count * sizeof(NameRecord) + nameOffset;
    if (SetFilePointer(hFile, stringDataOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        return FALSE;
    }

    if (nameLength > TTF_STRING_SAFETY_LIMIT) nameLength = TTF_STRING_SAFETY_LIMIT;
    char* stringBuffer = (char*)malloc(nameLength + 2);
    if (!stringBuffer) return FALSE;

    BOOL success = FALSE;
    if (ReadFile(hFile, stringBuffer, nameLength, &bytesRead, NULL) && bytesRead == nameLength) {
        ParseFontName(stringBuffer, nameLength, isUnicode, fontName, fontNameSize);
        success = TRUE;
    }

    free(stringBuffer);
    return success;
}

BOOL GetFontNameFromFile(const char* fontFilePath, char* fontName, size_t fontNameSize) {
    if (!fontFilePath || !fontName || fontNameSize == 0) return FALSE;

    wchar_t wFontPath[MAX_PATH];
    if (!Utf8ToWide(fontFilePath, wFontPath, MAX_PATH)) return FALSE;

    HANDLE hFile = CreateFileW(wFontPath, GENERIC_READ, FILE_SHARE_READ, NULL, 
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    BOOL result = ExtractFontNameFromHandle(hFile, fontName, fontNameSize);
    CloseHandle(hFile);
    return result;
}

/** Cleanup before exit or font switch */
BOOL UnloadCurrentFontResource(void) {
    if (!FONT_RESOURCE_LOADED || CURRENT_LOADED_FONT_PATH[0] == 0) {
        return TRUE;
    }

    BOOL result = RemoveFontResourceExW(CURRENT_LOADED_FONT_PATH, FR_PRIVATE, NULL);
    CURRENT_LOADED_FONT_PATH[0] = 0;
    FONT_RESOURCE_LOADED = FALSE;
    return result;
}

/**
 * Load font into GDI
 * @return TRUE on success
 * @note FR_PRIVATE prevents system font list pollution
 * @note Skips reload if already loaded
 */
BOOL LoadFontFromFile(const char* fontFilePath) {
    if (!fontFilePath) return FALSE;

    wchar_t wFontPath[MAX_PATH];
    if (!Utf8ToWide(fontFilePath, wFontPath, MAX_PATH)) return FALSE;

    if (GetFileAttributesW(wFontPath) == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }

    if (FONT_RESOURCE_LOADED && wcscmp(CURRENT_LOADED_FONT_PATH, wFontPath) == 0) {
        return TRUE;
    }

    int addResult = AddFontResourceExW(wFontPath, FR_PRIVATE, NULL);
    if (addResult <= 0) {
        return FALSE;
    }

    if (FONT_RESOURCE_LOADED && CURRENT_LOADED_FONT_PATH[0] != 0 && 
        wcscmp(CURRENT_LOADED_FONT_PATH, wFontPath) != 0) {
        RemoveFontResourceExW(CURRENT_LOADED_FONT_PATH, FR_PRIVATE, NULL);
    }

    wcscpy(CURRENT_LOADED_FONT_PATH, wFontPath);
    FONT_RESOURCE_LOADED = TRUE;
    return TRUE;
}

/**
 * Load font with auto-recovery
 * @param shouldUpdateConfig TRUE to persist auto-fixed path
 * @return TRUE if loaded (direct or recovered)
 */
static BOOL LoadFontInternal(const char* fontFileName, BOOL shouldUpdateConfig) {
    if (!fontFileName) return FALSE;

    char fontPath[MAX_PATH];
    if (!BuildFullFontPath(fontFileName, fontPath, MAX_PATH)) return FALSE;

    if (LoadFontFromFile(fontPath)) {
        return TRUE;
    }

    FontPathInfo pathInfo;
    if (!AutoFixFontPath(fontFileName, &pathInfo)) {
        return FALSE;
    }

    if (shouldUpdateConfig && IsFontsFolderPath(FONT_FILE_NAME)) {
        const char* currentRelative = ExtractRelativePath(FONT_FILE_NAME);
        if (currentRelative && strcmp(currentRelative, fontFileName) == 0) {
            UpdateFontConfig(&pathInfo, TRUE);
        }
    }

    return LoadFontFromFile(pathInfo.absolutePath);
}

BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName) {
    return LoadFontInternal(fontName, TRUE);
}

/**
 * Load font and extract internal name
 * @param realFontName Output: TTF internal family name
 * @return TRUE on success
 * @note Fallback: filename without extension
 */
BOOL LoadFontByNameAndGetRealName(HINSTANCE hInstance, const char* fontFileName, 
                                  char* realFontName, size_t realFontNameSize) {
    if (!fontFileName || !realFontName || realFontNameSize == 0) return FALSE;

    char fontPath[MAX_PATH];
    if (!BuildFullFontPath(fontFileName, fontPath, MAX_PATH)) return FALSE;

    BOOL fontExists = (GetFileAttributesW((wchar_t*)NULL) != INVALID_FILE_ATTRIBUTES);
    wchar_t wFontPath[MAX_PATH];
    if (Utf8ToWide(fontPath, wFontPath, MAX_PATH)) {
        fontExists = (GetFileAttributesW(wFontPath) != INVALID_FILE_ATTRIBUTES);
    }

    if (!fontExists) {
        FontPathInfo pathInfo;
        if (AutoFixFontPath(fontFileName, &pathInfo)) {
            strcpy(fontPath, pathInfo.absolutePath);

            if (IsFontsFolderPath(FONT_FILE_NAME)) {
                const char* currentRelative = ExtractRelativePath(FONT_FILE_NAME);
                if (currentRelative && strcmp(currentRelative, fontFileName) == 0) {
                    UpdateFontConfig(&pathInfo, TRUE);
                }
            }
        } else {
            return FALSE;
        }
    }

    if (!GetFontNameFromFile(fontPath, realFontName, realFontNameSize)) {
        const char* filename = GetFilenameFromPath(fontFileName);
        strncpy(realFontName, filename, realFontNameSize - 1);
        realFontName[realFontNameSize - 1] = '\0';
        char* dot = strrchr(realFontName, '.');
        if (dot) *dot = '\0';
    }

    return LoadFontFromFile(fontPath);
}

/** @param shouldReload TRUE to re-read entire config */
void WriteConfigFont(const char* fontFileName, BOOL shouldReload) {
    if (!fontFileName) return;

    char actualFontPath[MAX_PATH];
    char configFontName[MAX_PATH];

    if (FindFontInFontsFolder(fontFileName, actualFontPath, MAX_PATH)) {
        char relativePath[MAX_PATH];
        if (CalculateRelativePath(actualFontPath, relativePath, MAX_PATH)) {
            BuildFontConfigPath(relativePath, configFontName, MAX_PATH);
        } else {
            BuildFontConfigPath(fontFileName, configFontName, MAX_PATH);
        }
    } else {
        BuildFontConfigPath(fontFileName, configFontName, MAX_PATH);
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", configFontName, config_path);

    if (shouldReload) {
        ReadConfig();
    }
}

/** Debug helper (legacy) */
void ListAvailableFonts(void) {
    HDC hdc = GetDC(NULL);
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfCharSet = DEFAULT_CHARSET;

    HFONT hFont = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              lf.lfCharSet, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, NULL);
    SelectObject(hdc, hFont);

    EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontFamExProc, 0, 0);

    DeleteObject(hFont);
    ReleaseDC(NULL, hdc);
}

int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe, NEWTEXTMETRICEX *lpntme,
                               DWORD FontType, LPARAM lParam) {
    return 1;
}

BOOL PreviewFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;

    strncpy(PREVIEW_FONT_NAME, fontName, sizeof(PREVIEW_FONT_NAME) - 1);
    PREVIEW_FONT_NAME[sizeof(PREVIEW_FONT_NAME) - 1] = '\0';

    if (!LoadFontByNameAndGetRealName(hInstance, fontName, PREVIEW_INTERNAL_NAME, 
                                      sizeof(PREVIEW_INTERNAL_NAME))) {
        return FALSE;
    }

    IS_PREVIEWING = TRUE;
    return TRUE;
}

void CancelFontPreview(void) {
    IS_PREVIEWING = FALSE;
    PREVIEW_FONT_NAME[0] = '\0';
    PREVIEW_INTERNAL_NAME[0] = '\0';

    if (IsFontsFolderPath(FONT_FILE_NAME)) {
        const char* relativePath = ExtractRelativePath(FONT_FILE_NAME);
        if (relativePath) {
            LoadFontByNameAndGetRealName(GetModuleHandle(NULL), relativePath, 
                                        FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        }
    } else if (FONT_FILE_NAME[0] != '\0') {
        LoadFontByNameAndGetRealName(GetModuleHandle(NULL), FONT_FILE_NAME, 
                                    FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
    }
}

void ApplyFontPreview(void) {
    if (!IS_PREVIEWING || strlen(PREVIEW_FONT_NAME) == 0) return;

    strncpy(FONT_FILE_NAME, PREVIEW_FONT_NAME, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';

    strncpy(FONT_INTERNAL_NAME, PREVIEW_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';

    WriteConfigFont(FONT_FILE_NAME, FALSE);
    CancelFontPreview();
}

BOOL SwitchFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;

    strncpy(FONT_FILE_NAME, fontName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';

    if (!LoadFontByNameAndGetRealName(hInstance, fontName, FONT_INTERNAL_NAME, 
                                      sizeof(FONT_INTERNAL_NAME))) {
        return FALSE;
    }

    WriteConfigFont(FONT_FILE_NAME, FALSE);
    return TRUE;
}

BOOL ExtractFontResourceToFile(HINSTANCE hInstance, int resourceId, const char* outputPath) {
    if (!outputPath) return FALSE;

    HRSRC hResource = FindResourceW(hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
    if (hResource == NULL) return FALSE;

    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (hMemory == NULL) return FALSE;

    void* fontData = LockResource(hMemory);
    if (fontData == NULL) return FALSE;

    DWORD fontLength = SizeofResource(hInstance, hResource);
    if (fontLength == 0) return FALSE;

    wchar_t wOutputPath[MAX_PATH];
    if (!Utf8ToWide(outputPath, wOutputPath, MAX_PATH)) return FALSE;

    HANDLE hFile = CreateFileW(wOutputPath, GENERIC_WRITE, 0, NULL, 
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    DWORD bytesWritten;
    BOOL result = WriteFile(hFile, fontData, fontLength, &bytesWritten, NULL);
    CloseHandle(hFile);

    return (result && bytesWritten == fontLength);
}

BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance) {
    wchar_t wFontsFolderPath[MAX_PATH] = {0};
    if (!GetFontsFolderWide(wFontsFolderPath, MAX_PATH, TRUE)) return FALSE;

    char fontsFolderPath[MAX_PATH];
    if (!WideToUtf8(wFontsFolderPath, fontsFolderPath, MAX_PATH)) return FALSE;

    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
        char outputPath[MAX_PATH];
        snprintf(outputPath, MAX_PATH, "%s\\%s", fontsFolderPath, fontResources[i].fontName);
        ExtractFontResourceToFile(hInstance, fontResources[i].resourceId, outputPath);
    }

    return TRUE;
}

BOOL CheckAndFixFontPath(void) {
    if (!IsFontsFolderPath(FONT_FILE_NAME)) {
        return FALSE;
    }

    const char* relativePath = ExtractRelativePath(FONT_FILE_NAME);
    if (!relativePath) return FALSE;

    char fontPath[MAX_PATH];
    if (!BuildFullFontPath(relativePath, fontPath, MAX_PATH)) return FALSE;

    wchar_t wFontPath[MAX_PATH];
    if (!Utf8ToWide(fontPath, wFontPath, MAX_PATH)) return FALSE;

    if (GetFileAttributesW(wFontPath) != INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }

    FontPathInfo pathInfo;
    if (!AutoFixFontPath(relativePath, &pathInfo)) {
        return FALSE;
    }

    UpdateFontConfig(&pathInfo, TRUE);
    GetFontNameFromFile(pathInfo.absolutePath, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));

    return TRUE;
}
