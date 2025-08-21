/**
 * @file font.c
 * @brief Font management and embedded font resource handling
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>
#include "../include/font.h"
#include "../resource/resource.h"

/** @brief Font name table structures for TTF font parsing */
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

/** @brief Current font file name */
char FONT_FILE_NAME[100] = "Hack Nerd Font.ttf";
/** @brief Internal font name for Windows GDI */
char FONT_INTERNAL_NAME[100];
/** @brief Preview font file name during font selection */
char PREVIEW_FONT_NAME[100] = "";
/** @brief Preview internal font name for Windows GDI */
char PREVIEW_INTERNAL_NAME[100] = "";
/** @brief Flag indicating if font preview mode is active */
BOOL IS_PREVIEWING = FALSE;

/** @brief Array of embedded font resources mapping control IDs to resource IDs and filenames */
FontResource fontResources[] = {
    {CLOCK_IDC_FONT_RECMONO, IDR_FONT_RECMONO, "RecMonoCasual Nerd Font Mono Essence.ttf"},
    {CLOCK_IDC_FONT_DEPARTURE, IDR_FONT_DEPARTURE, "DepartureMono Nerd Font Propo Essence.ttf"},
    {CLOCK_IDC_FONT_TERMINESS, IDR_FONT_TERMINESS, "Terminess Nerd Font Propo Essence.ttf"},
    {CLOCK_IDC_FONT_JACQUARD, IDR_FONT_JACQUARD, "Jacquard 12 Essence.ttf"},
    {CLOCK_IDC_FONT_JACQUARDA, IDR_FONT_JACQUARDA, "Jacquarda Bastarda 9 Essence.ttf"},
    {CLOCK_IDC_FONT_PIXELIFY, IDR_FONT_PIXELIFY, "Pixelify Sans Medium Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_BURNED, IDR_FONT_RUBIK_BURNED, "Rubik Burned Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_GLITCH, IDR_FONT_RUBIK_GLITCH, "Rubik Glitch Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_MARKER_HATCH, IDR_FONT_RUBIK_MARKER_HATCH, "Rubik Marker Hatch Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_PUDDLES, IDR_FONT_RUBIK_PUDDLES, "Rubik Puddles Essence.ttf"},
    {CLOCK_IDC_FONT_WALLPOET, IDR_FONT_WALLPOET, "Wallpoet Essence.ttf"},
    {CLOCK_IDC_FONT_PROFONT, IDR_FONT_PROFONT, "ProFont IIx Nerd Font Essence.ttf"},
    {CLOCK_IDC_FONT_DADDYTIME, IDR_FONT_DADDYTIME, "DaddyTimeMono Nerd Font Propo Essence.ttf"},
};

/** @brief Total number of embedded font resources */
const int FONT_RESOURCES_COUNT = sizeof(fontResources) / sizeof(FontResource);

/** @brief External reference to clock text color configuration */
extern char CLOCK_TEXT_COLOR[];

/** @brief External function to get configuration file path */
extern void GetConfigPath(char* path, size_t maxLen);
/** @brief External function to reload configuration */
extern void ReadConfig(void);
/** @brief Font enumeration callback function */
extern int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD FontType, LPARAM lParam);

/**
 * @brief Convert big-endian WORD to little-endian
 * @param value Big-endian WORD value
 * @return Little-endian WORD value
 */
static WORD SwapWORD(WORD value) {
    return ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
}

/**
 * @brief Convert big-endian DWORD to little-endian
 * @param value Big-endian DWORD value
 * @return Little-endian DWORD value
 */
static DWORD SwapDWORD(DWORD value) {
    return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) | 
           ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24);
}

/**
 * @brief Read font family name from TTF/OTF font file
 * @param fontFilePath Path to font file
 * @param fontName Buffer to store extracted font name
 * @param fontNameSize Size of fontName buffer
 * @return TRUE if font name extracted successfully, FALSE otherwise
 */
BOOL GetFontNameFromFile(const char* fontFilePath, char* fontName, size_t fontNameSize) {
    if (!fontFilePath || !fontName || fontNameSize == 0) return FALSE;
    
    /** Convert to wide character path */
    wchar_t wFontPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, fontFilePath, -1, wFontPath, MAX_PATH);
    
    /** Open font file */
    HANDLE hFile = CreateFileW(wFontPath, GENERIC_READ, FILE_SHARE_READ, NULL, 
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    /** Read font directory header */
    FontDirectoryHeader fontHeader;
    DWORD bytesRead;
    if (!ReadFile(hFile, &fontHeader, sizeof(FontDirectoryHeader), &bytesRead, NULL) ||
        bytesRead != sizeof(FontDirectoryHeader)) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    /** Convert endianness */
    fontHeader.numTables = SwapWORD(fontHeader.numTables);
    
    /** Find 'name' table */
    DWORD nameTableOffset = 0;
    DWORD nameTableLength = 0;
    for (WORD i = 0; i < fontHeader.numTables; i++) {
        TableRecord tableRecord;
        if (!ReadFile(hFile, &tableRecord, sizeof(TableRecord), &bytesRead, NULL) ||
            bytesRead != sizeof(TableRecord)) {
            CloseHandle(hFile);
            return FALSE;
        }
        
        /** Check if this is the 'name' table (0x6E616D65 = 'name') */
        if (tableRecord.tag == 0x656D616E) {  // 'name' in little-endian
            nameTableOffset = SwapDWORD(tableRecord.offset);
            nameTableLength = SwapDWORD(tableRecord.length);
            break;
        }
    }
    
    if (nameTableOffset == 0) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    /** Seek to name table */
    if (SetFilePointer(hFile, nameTableOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    /** Read name table header */
    NameTableHeader nameHeader;
    if (!ReadFile(hFile, &nameHeader, sizeof(NameTableHeader), &bytesRead, NULL) ||
        bytesRead != sizeof(NameTableHeader)) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    /** Convert endianness */
    nameHeader.count = SwapWORD(nameHeader.count);
    nameHeader.stringOffset = SwapWORD(nameHeader.stringOffset);
    
    /** Search for font family name record (nameID = 1) */
    BOOL foundName = FALSE;
    WORD bestPlatform = 0;
    WORD bestEncoding = 0;
    WORD bestLanguage = 0;
    WORD nameLength = 0;
    WORD nameOffset = 0;
    
    for (WORD i = 0; i < nameHeader.count; i++) {
        NameRecord nameRecord;
        if (!ReadFile(hFile, &nameRecord, sizeof(NameRecord), &bytesRead, NULL) ||
            bytesRead != sizeof(NameRecord)) {
            CloseHandle(hFile);
            return FALSE;
        }
        
        /** Convert endianness */
        nameRecord.platformID = SwapWORD(nameRecord.platformID);
        nameRecord.encodingID = SwapWORD(nameRecord.encodingID);
        nameRecord.languageID = SwapWORD(nameRecord.languageID);
        nameRecord.nameID = SwapWORD(nameRecord.nameID);
        nameRecord.length = SwapWORD(nameRecord.length);
        nameRecord.offset = SwapWORD(nameRecord.offset);
        
        /** Look for font family name (nameID = 1) */
        if (nameRecord.nameID == 1) {
            /** Prefer Windows platform (3) with Unicode encoding (1) */
            if (nameRecord.platformID == 3 && nameRecord.encodingID == 1) {
                bestPlatform = nameRecord.platformID;
                bestEncoding = nameRecord.encodingID;
                bestLanguage = nameRecord.languageID;
                nameLength = nameRecord.length;
                nameOffset = nameRecord.offset;
                foundName = TRUE;
                break;  // This is the best option
            }
            /** Fallback to any Unicode platform */
            else if (!foundName && nameRecord.platformID == 0) {
                bestPlatform = nameRecord.platformID;
                bestEncoding = nameRecord.encodingID;
                bestLanguage = nameRecord.languageID;
                nameLength = nameRecord.length;
                nameOffset = nameRecord.offset;
                foundName = TRUE;
            }
            /** Last resort: any platform */
            else if (!foundName) {
                bestPlatform = nameRecord.platformID;
                bestEncoding = nameRecord.encodingID;
                bestLanguage = nameRecord.languageID;
                nameLength = nameRecord.length;
                nameOffset = nameRecord.offset;
                foundName = TRUE;
            }
        }
    }
    
    if (!foundName) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    /** Seek to string data */
    DWORD stringDataOffset = nameTableOffset + sizeof(NameTableHeader) + 
                           nameHeader.count * sizeof(NameRecord) + nameOffset;
    if (SetFilePointer(hFile, stringDataOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    /** Read string data */
    if (nameLength > 1024) nameLength = 1024;  // Safety limit
    char* stringBuffer = (char*)malloc(nameLength + 2);
    if (!stringBuffer) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    if (!ReadFile(hFile, stringBuffer, nameLength, &bytesRead, NULL) || bytesRead != nameLength) {
        free(stringBuffer);
        CloseHandle(hFile);
        return FALSE;
    }
    
    /** Convert string based on platform and encoding */
    if (bestPlatform == 3 && bestEncoding == 1) {
        /** Windows Unicode (UTF-16 BE) */
        WCHAR* unicodeStr = (WCHAR*)stringBuffer;
        int numChars = nameLength / 2;
        
        /** Convert from big-endian to little-endian */
        for (int i = 0; i < numChars; i++) {
            unicodeStr[i] = SwapWORD(unicodeStr[i]);
        }
        unicodeStr[numChars] = 0;
        
        /** Convert to UTF-8 */
        WideCharToMultiByte(CP_UTF8, 0, unicodeStr, -1, fontName, (int)fontNameSize, NULL, NULL);
    } else {
        /** ASCII/other encoding - copy directly */
        size_t copyLen = (nameLength < fontNameSize - 1) ? nameLength : fontNameSize - 1;
        memcpy(fontName, stringBuffer, copyLen);
        fontName[copyLen] = '\0';
    }
    
    free(stringBuffer);
    CloseHandle(hFile);
    return TRUE;
}

/**
 * @brief Load font from embedded resource into memory
 * @param hInstance Application instance handle
 * @param resourceId Resource ID of the font to load
 * @return TRUE if font loaded successfully, FALSE otherwise
 */
BOOL LoadFontFromResource(HINSTANCE hInstance, int resourceId) {
    /** Find font resource in executable */
    HRSRC hResource = FindResourceW(hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
    if (hResource == NULL) {
        return FALSE;
    }

    /** Load resource into memory */
    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (hMemory == NULL) {
        return FALSE;
    }

    /** Lock resource data for access */
    void* fontData = LockResource(hMemory);
    if (fontData == NULL) {
        return FALSE;
    }

    /** Add font to system font table from memory */
    DWORD fontLength = SizeofResource(hInstance, hResource);
    DWORD nFonts = 0;
    HANDLE handle = AddFontMemResourceEx(fontData, fontLength, NULL, &nFonts);
    
    if (handle == NULL) {
        return FALSE;
    }
    
    return TRUE;
}

/**
 * @brief Load font from file on disk
 * @param fontFilePath Full path to font file
 * @return TRUE if font loaded successfully, FALSE otherwise
 */
BOOL LoadFontFromFile(const char* fontFilePath) {
    if (!fontFilePath) return FALSE;
    
    /** Convert to wide character for Unicode support */
    wchar_t wFontPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, fontFilePath, -1, wFontPath, MAX_PATH);
    
    /** Check if file exists */
    if (GetFileAttributesW(wFontPath) == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    
    /** Add font from file to system font table */
    int result = AddFontResourceExW(wFontPath, FR_PRIVATE, NULL);
    return (result > 0);
}

/**
 * @brief Find font file in fonts folder and subfolders
 * @param fontFileName Font filename to search for
 * @param foundPath Buffer to store found font path
 * @param foundPathSize Size of foundPath buffer
 * @return TRUE if font file found, FALSE otherwise
 */
BOOL FindFontInFontsFolder(const char* fontFileName, char* foundPath, size_t foundPathSize) {
    if (!fontFileName || !foundPath || foundPathSize == 0) return FALSE;
    
    /** Helper function to recursively search for font file */
    BOOL SearchFontRecursive(const char* folderPath, const char* targetFile, char* resultPath, size_t resultSize) {
        char searchPath[MAX_PATH];
        snprintf(searchPath, MAX_PATH, "%s\\*", folderPath);
        
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath, &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                /** Skip . and .. entries */
                if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                    continue;
                }
                
                char fullItemPath[MAX_PATH];
                snprintf(fullItemPath, MAX_PATH, "%s\\%s", folderPath, findData.cFileName);
                
                /** Check if this is the target font file */
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    if (stricmp(findData.cFileName, targetFile) == 0) {
                        strncpy(resultPath, fullItemPath, resultSize - 1);
                        resultPath[resultSize - 1] = '\0';
                        FindClose(hFind);
                        return TRUE;
                    }
                }
                /** Recursively search subdirectories */
                else if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (SearchFontRecursive(fullItemPath, targetFile, resultPath, resultSize)) {
                        FindClose(hFind);
                        return TRUE;
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        return FALSE;
    }
    
    /** Get fonts folder path */
    char fontsFolderPath[MAX_PATH];
    char* appdata_path = getenv("LOCALAPPDATA");
    if (!appdata_path) return FALSE;
    
    snprintf(fontsFolderPath, MAX_PATH, "%s\\Catime\\resources\\fonts", appdata_path);
    
    /** Search for font file recursively */
    return SearchFontRecursive(fontsFolderPath, fontFileName, foundPath, foundPathSize);
}

/**
 * @brief Load font by name from embedded resources or fonts folder
 * @param hInstance Application instance handle
 * @param fontName Font filename to search for
 * @return TRUE if font found and loaded, FALSE otherwise
 */
BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName) {
    /** First try embedded resources */
    for (int i = 0; i < sizeof(fontResources) / sizeof(FontResource); i++) {
        if (strcmp(fontResources[i].fontName, fontName) == 0) {
            return LoadFontFromResource(hInstance, fontResources[i].resourceId);
        }
    }
    
    /** If not found in embedded resources, try fonts folder */
    char fontPath[MAX_PATH];
    if (FindFontInFontsFolder(fontName, fontPath, MAX_PATH)) {
        return LoadFontFromFile(fontPath);
    }
    
    return FALSE;
}

/**
 * @brief Load font and get real font name for fonts folder fonts
 * @param hInstance Application instance handle
 * @param fontFileName Font filename to search for
 * @param realFontName Buffer to store real font name
 * @param realFontNameSize Size of realFontName buffer
 * @return TRUE if font found, loaded, and real name extracted, FALSE otherwise
 */
BOOL LoadFontByNameAndGetRealName(HINSTANCE hInstance, const char* fontFileName, 
                                  char* realFontName, size_t realFontNameSize) {
    if (!fontFileName || !realFontName || realFontNameSize == 0) return FALSE;
    
    /** First try embedded resources */
    for (int i = 0; i < sizeof(fontResources) / sizeof(FontResource); i++) {
        if (strcmp(fontResources[i].fontName, fontFileName) == 0) {
            if (LoadFontFromResource(hInstance, fontResources[i].resourceId)) {
                /** For embedded fonts, use the filename without extension as font name */
                strncpy(realFontName, fontFileName, realFontNameSize - 1);
                realFontName[realFontNameSize - 1] = '\0';
                
                /** Remove .ttf extension if present */
                char* dot = strrchr(realFontName, '.');
                if (dot) *dot = '\0';
                
                return TRUE;
            }
            return FALSE;
        }
    }
    
    /** If not found in embedded resources, try fonts folder */
    char fontPath[MAX_PATH];
    if (FindFontInFontsFolder(fontFileName, fontPath, MAX_PATH)) {
        /** First extract the real font name from the file */
        if (!GetFontNameFromFile(fontPath, realFontName, realFontNameSize)) {
            /** Fallback to filename without extension */
            strncpy(realFontName, fontFileName, realFontNameSize - 1);
            realFontName[realFontNameSize - 1] = '\0';
            char* dot = strrchr(realFontName, '.');
            if (dot) *dot = '\0';
        }
        
        /** Load the font file */
        return LoadFontFromFile(fontPath);
    }
    
    return FALSE;
}

/**
 * @brief Write font configuration to config file
 * @param font_file_name Font filename to save in configuration
 */
void WriteConfigFont(const char* font_file_name) {
    if (!font_file_name) return;
    
    /** Check if this is a fonts folder font and add path prefix */
    char configFontName[MAX_PATH];
    BOOL isExternalFont = FALSE;
    
    /** Check if this font is in the fonts folder */
    char fontPath[MAX_PATH];
    if (FindFontInFontsFolder(font_file_name, fontPath, MAX_PATH)) {
        /** Check if it's not an embedded resource */
        BOOL isEmbedded = FALSE;
        for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
            if (strcmp(fontResources[i].fontName, font_file_name) == 0) {
                isEmbedded = TRUE;
                break;
            }
        }
        
        if (!isEmbedded) {
            /** Add path prefix for external fonts */
            snprintf(configFontName, MAX_PATH, "%%LOCALAPPDATA%%\\Catime\\resources\\fonts\\%s", font_file_name);
            isExternalFont = TRUE;
        } else {
            /** Use original name for embedded fonts */
            strncpy(configFontName, font_file_name, MAX_PATH - 1);
            configFontName[MAX_PATH - 1] = '\0';
        }
    } else {
        /** Use original name if not found in fonts folder */
        strncpy(configFontName, font_file_name, MAX_PATH - 1);
        configFontName[MAX_PATH - 1] = '\0';
    }
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert path to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    /** Read existing config file */
    FILE *file = _wfopen(wconfig_path, L"r");
    if (!file) {
        fprintf(stderr, "Failed to open config file for reading: %s\n", config_path);
        return;
    }

    /** Get file size and allocate buffer */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *config_content = (char *)malloc(file_size + 1);
    if (!config_content) {
        fprintf(stderr, "Memory allocation failed!\n");
        fclose(file);
        return;
    }
    fread(config_content, sizeof(char), file_size, file);
    config_content[file_size] = '\0';
    fclose(file);

    /** Allocate buffer for modified config */
    char *new_config = (char *)malloc(file_size + 100);
    if (!new_config) {
        fprintf(stderr, "Memory allocation failed!\n");
        free(config_content);
        return;
    }
    new_config[0] = '\0';

    /** Process each line and update FONT_FILE_NAME */
    char *line = strtok(config_content, "\n");
    while (line) {
        if (strncmp(line, "FONT_FILE_NAME=", 15) == 0) {
            /** Replace font file name line */
            strcat(new_config, "FONT_FILE_NAME=");
            strcat(new_config, configFontName);
            strcat(new_config, "\n");
        } else {
            /** Keep existing line */
            strcat(new_config, line);
            strcat(new_config, "\n");
        }
        line = strtok(NULL, "\n");
    }

    free(config_content);

    /** Write updated config back to file */
    file = _wfopen(wconfig_path, L"w");
    if (!file) {
        fprintf(stderr, "Failed to open config file for writing: %s\n", config_path);
        free(new_config);
        return;
    }
    fwrite(new_config, sizeof(char), strlen(new_config), file);
    fclose(file);

    free(new_config);

    /** Reload configuration to apply changes */
    ReadConfig();
}

/**
 * @brief Enumerate all available system fonts
 */
void ListAvailableFonts(void) {
    /** Get device context for font enumeration */
    HDC hdc = GetDC(NULL);
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfCharSet = DEFAULT_CHARSET;

    /** Create temporary font for enumeration context */
    HFONT hFont = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              lf.lfCharSet, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, NULL);
    SelectObject(hdc, hFont);

    /** Enumerate all font families */
    EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontFamExProc, 0, 0);

    /** Clean up resources */
    DeleteObject(hFont);
    ReleaseDC(NULL, hdc);
}

/**
 * @brief Font enumeration callback procedure
 * @param lpelfe Extended logical font information
 * @param lpntme Font metrics information
 * @param FontType Font type flags
 * @param lParam User-defined parameter
 * @return 1 to continue enumeration, 0 to stop
 */
int CALLBACK EnumFontFamExProc(
    ENUMLOGFONTEXW *lpelfe,
    NEWTEXTMETRICEX *lpntme,
    DWORD FontType,
    LPARAM lParam
) {
    /** Continue enumeration (placeholder implementation) */
    return 1;
}

/**
 * @brief Start font preview mode with specified font
 * @param hInstance Application instance handle
 * @param fontName Font filename to preview
 * @return TRUE if preview started successfully, FALSE otherwise
 */
BOOL PreviewFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;
    
    /** Copy font name for preview */
    strncpy(PREVIEW_FONT_NAME, fontName, sizeof(PREVIEW_FONT_NAME) - 1);
    PREVIEW_FONT_NAME[sizeof(PREVIEW_FONT_NAME) - 1] = '\0';
    
    /** Load font and get real font name */
    if (!LoadFontByNameAndGetRealName(hInstance, fontName, PREVIEW_INTERNAL_NAME, sizeof(PREVIEW_INTERNAL_NAME))) {
        return FALSE;
    }
    
    /** Enable preview mode */
    IS_PREVIEWING = TRUE;
    return TRUE;
}

/**
 * @brief Cancel font preview and return to current font
 */
void CancelFontPreview(void) {
    IS_PREVIEWING = FALSE;
    PREVIEW_FONT_NAME[0] = '\0';
    PREVIEW_INTERNAL_NAME[0] = '\0';
}

/**
 * @brief Apply previewed font as the current font
 */
void ApplyFontPreview(void) {
    if (!IS_PREVIEWING || strlen(PREVIEW_FONT_NAME) == 0) return;
    
    /** Copy preview font to current font variables */
    strncpy(FONT_FILE_NAME, PREVIEW_FONT_NAME, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    strncpy(FONT_INTERNAL_NAME, PREVIEW_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    
    /** Save to configuration and exit preview mode */
    WriteConfigFont(FONT_FILE_NAME);
    CancelFontPreview();
}

/**
 * @brief Extract embedded font resource to file
 * @param hInstance Application instance handle
 * @param resourceId Resource ID of the font to extract
 * @param outputPath Full path where to save the font file
 * @return TRUE if font extracted successfully, FALSE otherwise
 */
BOOL ExtractFontResourceToFile(HINSTANCE hInstance, int resourceId, const char* outputPath) {
    if (!outputPath) return FALSE;
    
    /** Find font resource in executable */
    HRSRC hResource = FindResourceW(hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
    if (hResource == NULL) {
        return FALSE;
    }

    /** Load resource into memory */
    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (hMemory == NULL) {
        return FALSE;
    }

    /** Lock resource data for access */
    void* fontData = LockResource(hMemory);
    if (fontData == NULL) {
        return FALSE;
    }

    /** Get resource size */
    DWORD fontLength = SizeofResource(hInstance, hResource);
    if (fontLength == 0) {
        return FALSE;
    }
    
    /** Convert output path to wide character */
    wchar_t wOutputPath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, outputPath, -1, wOutputPath, MAX_PATH);
    
    /** Create output file */
    HANDLE hFile = CreateFileW(wOutputPath, GENERIC_WRITE, 0, NULL, 
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    /** Write font data to file */
    DWORD bytesWritten;
    BOOL result = WriteFile(hFile, fontData, fontLength, &bytesWritten, NULL);
    CloseHandle(hFile);
    
    return (result && bytesWritten == fontLength);
}

/**
 * @brief Extract all embedded fonts to the fonts directory
 * @param hInstance Application instance handle
 * @return TRUE if all fonts extracted successfully, FALSE otherwise
 */
BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance) {
    /** Get fonts folder path */
    char fontsFolderPath[MAX_PATH];
    char* appdata_path = getenv("LOCALAPPDATA");
    if (!appdata_path) return FALSE;
    
    snprintf(fontsFolderPath, MAX_PATH, "%s\\Catime\\resources\\fonts", appdata_path);
    
    /** Create directory structure recursively */
    char catimePath[MAX_PATH];
    char resourcesPath[MAX_PATH];
    snprintf(catimePath, MAX_PATH, "%s\\Catime", appdata_path);
    snprintf(resourcesPath, MAX_PATH, "%s\\Catime\\resources", appdata_path);
    
    /** Create Catime directory */
    wchar_t wCatimePath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, catimePath, -1, wCatimePath, MAX_PATH);
    if (GetFileAttributesW(wCatimePath) == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryW(wCatimePath, NULL);
    }
    
    /** Create resources directory */
    wchar_t wResourcesPath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, resourcesPath, -1, wResourcesPath, MAX_PATH);
    if (GetFileAttributesW(wResourcesPath) == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryW(wResourcesPath, NULL);
    }
    
    /** Create fonts directory */
    wchar_t wFontsFolderPath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, fontsFolderPath, -1, wFontsFolderPath, MAX_PATH);
    if (GetFileAttributesW(wFontsFolderPath) == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryW(wFontsFolderPath, NULL);
    }
    
    /** Extract each embedded font resource */
    int successCount = 0;
    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
        char outputPath[MAX_PATH];
        snprintf(outputPath, MAX_PATH, "%s\\%s", fontsFolderPath, fontResources[i].fontName);
        
        /** Check if font file already exists */
        wchar_t wOutputPath[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, outputPath, -1, wOutputPath, MAX_PATH);
        
        if (GetFileAttributesW(wOutputPath) == INVALID_FILE_ATTRIBUTES) {
            /** File doesn't exist, extract it */
            if (ExtractFontResourceToFile(hInstance, fontResources[i].resourceId, outputPath)) {
                successCount++;
            }
        } else {
            /** File already exists, count as success */
            successCount++;
        }
    }
    
    return (successCount == FONT_RESOURCES_COUNT);
}

/**
 * @brief Switch to a different font permanently
 * @param hInstance Application instance handle
 * @param fontName Font filename to switch to
 * @return TRUE if font switched successfully, FALSE otherwise
 */
BOOL SwitchFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;
    
    /** Update current font name */
    strncpy(FONT_FILE_NAME, fontName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    /** Load font and get real font name */
    if (!LoadFontByNameAndGetRealName(hInstance, fontName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
        return FALSE;
    }
    
    /** Save new font to configuration */
    WriteConfigFont(FONT_FILE_NAME);
    return TRUE;
}