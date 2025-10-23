/**
 * @file language.c
 * @brief Multi-language support with data-driven architecture
 * 
 * Implements a centralized localization system using metadata tables to eliminate
 * code duplication and improve maintainability.
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include "../include/language.h"
#include "../resource/resource.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum number of translation entries supported */
#define MAX_TRANSLATIONS 200
/** @brief Maximum length of a localized string */
#define MAX_STRING_LENGTH 1024

/** @brief Resource IDs for embedded language files */
#define LANG_EN_INI       1001
#define LANG_ZH_CN_INI    1002
#define LANG_ZH_TW_INI    1003
#define LANG_ES_INI       1004
#define LANG_FR_INI       1005
#define LANG_DE_INI       1006
#define LANG_RU_INI       1007
#define LANG_PT_INI       1008
#define LANG_JA_INI       1009
#define LANG_KO_INI       1010

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Structure to store English-to-localized string mapping
 */
typedef struct {
    wchar_t english[MAX_STRING_LENGTH];      /**< Original English string key */
    wchar_t translation[MAX_STRING_LENGTH];  /**< Localized translation value */
} LocalizedString;

/**
 * @brief Centralized language metadata for data-driven configuration
 * 
 * Consolidates all language-specific information in one place, eliminating
 * the need for multiple switch statements across different functions.
 */
typedef struct {
    AppLanguage language;           /**< Language enumeration value */
    UINT resourceId;                /**< Resource ID for embedded INI file */
    WORD primaryLangId;             /**< Windows primary language identifier */
    WORD subLangId;                 /**< Windows sublanguage identifier (0 = any) */
    const wchar_t* localeCode;      /**< Standard locale code string */
    BOOL useDirectChinese;          /**< TRUE if Chinese text should be used directly */
    AppLanguage fallbackLanguage;   /**< Fallback language if resource not found */
} LanguageMetadata;

/* ============================================================================
 * Global State
 * ============================================================================ */

/** @brief Current application language setting */
AppLanguage CURRENT_LANGUAGE = APP_LANG_ENGLISH;

/** @brief Array of loaded translation entries */
static LocalizedString g_translations[MAX_TRANSLATIONS];
/** @brief Number of currently loaded translations */
static int g_translation_count = 0;
/** @brief Initialization flag for lazy loading */
static BOOL g_initialized = FALSE;

/* ============================================================================
 * Language Metadata Table
 * ============================================================================ */

/**
 * @brief Centralized language configuration table
 * 
 * All language-specific data is defined here. Adding a new language requires
 * only adding one entry to this table, rather than modifying multiple functions.
 */
static const LanguageMetadata g_languageMetadata[APP_LANG_COUNT] = {
    {APP_LANG_CHINESE_SIMP, LANG_ZH_CN_INI,  LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED,  L"zh_CN",   TRUE,  APP_LANG_ENGLISH},
    {APP_LANG_CHINESE_TRAD, LANG_ZH_TW_INI,  LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL, L"zh-Hant", TRUE,  APP_LANG_ENGLISH},
    {APP_LANG_ENGLISH,      LANG_EN_INI,     LANG_ENGLISH, 0,                           L"en",      FALSE, APP_LANG_ENGLISH},
    {APP_LANG_SPANISH,      LANG_ES_INI,     LANG_SPANISH, 0,                           L"es",      FALSE, APP_LANG_ENGLISH},
    {APP_LANG_FRENCH,       LANG_FR_INI,     LANG_FRENCH,  0,                           L"fr",      FALSE, APP_LANG_ENGLISH},
    {APP_LANG_GERMAN,       LANG_DE_INI,     LANG_GERMAN,  0,                           L"de",      FALSE, APP_LANG_ENGLISH},
    {APP_LANG_RUSSIAN,      LANG_RU_INI,     LANG_RUSSIAN, 0,                           L"ru",      FALSE, APP_LANG_ENGLISH},
    {APP_LANG_PORTUGUESE,   LANG_PT_INI,     LANG_PORTUGUESE, 0,                        L"pt",      FALSE, APP_LANG_ENGLISH},
    {APP_LANG_JAPANESE,     LANG_JA_INI,     LANG_JAPANESE, 0,                          L"ja",      FALSE, APP_LANG_ENGLISH},
    {APP_LANG_KOREAN,       LANG_KO_INI,     LANG_KOREAN,  0,                           L"ko",      FALSE, APP_LANG_ENGLISH},
};

/* ============================================================================
 * String Parsing Helpers
 * ============================================================================ */

/**
 * @brief Extract quoted string from INI line
 * 
 * Finds the next pair of double quotes and extracts the content between them.
 * 
 * @param start Pointer to start searching from
 * @param output Buffer to store extracted string
 * @param maxLength Maximum length of output buffer
 * @return Pointer after the closing quote, or NULL if parsing failed
 */
static const wchar_t* ExtractQuotedString(const wchar_t* start, wchar_t* output, size_t maxLength) {
    const wchar_t* quote_start = wcschr(start, L'"');
    if (!quote_start) return NULL;
    quote_start++;
    
    const wchar_t* quote_end = wcschr(quote_start, L'"');
    if (!quote_end) return NULL;
    
    size_t length = quote_end - quote_start;
    if (length >= maxLength) length = maxLength - 1;
    
    wcsncpy(output, quote_start, length);
    output[length] = L'\0';
    
    return quote_end + 1;
}

/**
 * @brief Process escape sequences in a string
 * 
 * Converts escape sequences like \n, \t, \\ to their actual characters.
 * Modifies the string in-place.
 * 
 * @param str String to process (modified in-place)
 */
static void ProcessEscapeSequences(wchar_t* str) {
    wchar_t* src = str;
    wchar_t* dst = str;
    
    while (*src) {
        if (*src == L'\\' && *(src + 1)) {
            switch (*(src + 1)) {
                case L'n':
                    *dst++ = L'\n';
                    src += 2;
                    break;
                case L't':
                    *dst++ = L'\t';
                    src += 2;
                    break;
                case L'\\':
                    *dst++ = L'\\';
                    src += 2;
                    break;
                default:
                    *dst++ = *src++;
                    break;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = L'\0';
}

/**
 * @brief Check if line should be skipped during INI parsing
 * 
 * @param line Line to check
 * @return TRUE if line should be skipped (empty, comment, or section header)
 */
static BOOL ShouldSkipLine(const wchar_t* line) {
    return (line[0] == L'\0' || line[0] == L';' || line[0] == L'[');
}

/**
 * @brief Parse a single INI file line and extract key-value pair
 * 
 * Expects format: "English Key" = "Translated Value"
 * Handles escape sequences in the translated value.
 * 
 * @param line Wide character line to parse
 * @return TRUE if successfully parsed and stored, FALSE otherwise
 */
static BOOL ParseIniLine(const wchar_t* line) {
    if (ShouldSkipLine(line)) {
        return FALSE;
    }
    
    if (g_translation_count >= MAX_TRANSLATIONS) {
        return FALSE;
    }
    
    /** Extract English key (first quoted string) */
    const wchar_t* pos = ExtractQuotedString(line, 
                                            g_translations[g_translation_count].english,
                                            MAX_STRING_LENGTH);
    if (!pos) return FALSE;
    
    /** Find equals sign, then extract translation value (second quoted string) */
    pos = wcschr(pos, L'=');
    if (!pos) return FALSE;
    
    pos = ExtractQuotedString(pos, 
                             g_translations[g_translation_count].translation,
                             MAX_STRING_LENGTH);
    if (!pos) return FALSE;
    
    /** Process escape sequences in translation value */
    ProcessEscapeSequences(g_translations[g_translation_count].translation);
    
    g_translation_count++;
    return TRUE;
}

/* ============================================================================
 * Resource Loading Helpers
 * ============================================================================ */

/**
 * @brief Convert UTF-8 string to wide character string
 * 
 * @param utf8 Input UTF-8 string
 * @param wstr Output wide character buffer
 * @param wstr_size Size of output buffer in wide characters
 * @return Number of characters converted (excluding null terminator)
 */
static int UTF8ToWideChar(const char* utf8, wchar_t* wstr, int wstr_size) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wstr_size) - 1;
}

/**
 * @brief Skip UTF-8 BOM if present at start of string
 * 
 * @param str String to check
 * @return Pointer after BOM if present, otherwise original pointer
 */
static const char* SkipUTF8BOM(const char* str) {
    if (str[0] == (char)0xEF && str[1] == (char)0xBB && str[2] == (char)0xBF) {
        return str + 3;
    }
    return str;
}

/**
 * @brief Load resource data into a null-terminated buffer
 * 
 * @param resourceId Resource ID to load
 * @param outBuffer Pointer to receive allocated buffer (caller must free)
 * @return TRUE if successful, FALSE if resource not found or allocation failed
 */
static BOOL LoadResourceToBuffer(UINT resourceId, char** outBuffer) {
    /** Find resource in executable */
    HRSRC hResInfo = FindResourceW(NULL, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hResInfo) {
        return FALSE;
    }
    
    /** Get resource size */
    DWORD dwSize = SizeofResource(NULL, hResInfo);
    if (dwSize == 0) {
        return FALSE;
    }
    
    /** Load resource into memory */
    HGLOBAL hResData = LoadResource(NULL, hResInfo);
    if (!hResData) {
        return FALSE;
    }
    
    /** Lock resource data for access */
    const char* pData = (const char*)LockResource(hResData);
    if (!pData) {
        return FALSE;
    }
    
    /** Create null-terminated buffer */
    *outBuffer = (char*)malloc(dwSize + 1);
    if (!*outBuffer) {
        return FALSE;
    }
    
    memcpy(*outBuffer, pData, dwSize);
    (*outBuffer)[dwSize] = '\0';
    
    return TRUE;
}

/**
 * @brief Parse language INI buffer and populate translation table
 * 
 * @param buffer Null-terminated UTF-8 buffer to parse
 */
static void ParseLanguageBuffer(char* buffer) {
    wchar_t wide_buffer[MAX_STRING_LENGTH];
    
    /** Tokenize by line breaks */
    char* line = strtok(buffer, "\r\n");
    
    while (line && g_translation_count < MAX_TRANSLATIONS) {
        /** Skip empty lines and UTF-8 BOM */
        if (line[0] == '\0') {
            line = strtok(NULL, "\r\n");
            continue;
        }
        
        line = (char*)SkipUTF8BOM(line);
        
        /** Convert UTF-8 to wide char and parse */
        if (UTF8ToWideChar(line, wide_buffer, MAX_STRING_LENGTH) > 0) {
            ParseIniLine(wide_buffer);
        }
        
        line = strtok(NULL, "\r\n");
    }
}

/**
 * @brief Load language translations from embedded resource
 * 
 * Attempts to load the specified language resource. If not found, falls back
 * to the language's designated fallback (usually English).
 * 
 * @param language Language to load
 * @return TRUE if successful, FALSE if failed
 */
static BOOL LoadLanguageResource(AppLanguage language) {
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }
    
    /** Reset translation count for new language */
    g_translation_count = 0;
    
    const LanguageMetadata* metadata = &g_languageMetadata[language];
    char* buffer = NULL;
    
    /** Try to load the requested language resource */
    if (!LoadResourceToBuffer(metadata->resourceId, &buffer)) {
        /** If loading failed and we have a different fallback, try that */
        if (metadata->fallbackLanguage != language) {
            return LoadLanguageResource(metadata->fallbackLanguage);
        }
        return FALSE;
    }
    
    /** Parse the loaded buffer */
    ParseLanguageBuffer(buffer);
    
    free(buffer);
    return TRUE;
}

/* ============================================================================
 * Translation Lookup
 * ============================================================================ */

/**
 * @brief Find translation for an English string
 * 
 * Performs linear search through translation table. For the typical scale
 * (200 entries), this is more efficient than hash table overhead.
 * 
 * @param english English string to translate
 * @return Pointer to translation if found, NULL otherwise
 */
static const wchar_t* FindTranslation(const wchar_t* english) {
    for (int i = 0; i < g_translation_count; i++) {
        if (wcscmp(english, g_translations[i].english) == 0) {
            return g_translations[i].translation;
        }
    }
    return NULL;
}

/* ============================================================================
 * Language Detection
 * ============================================================================ */

/**
 * @brief Detect system language and set as current language
 * 
 * Uses Windows API to query user's default UI language and maps it to
 * our supported languages using the metadata table.
 */
static void DetectSystemLanguage(void) {
    LANGID langID = GetUserDefaultUILanguage();
    WORD primaryLang = PRIMARYLANGID(langID);
    WORD subLang = SUBLANGID(langID);
    
    /** Search metadata table for matching language */
    for (int i = 0; i < APP_LANG_COUNT; i++) {
        const LanguageMetadata* meta = &g_languageMetadata[i];
        
        /** Check primary language match */
        if (meta->primaryLangId == primaryLang) {
            /** If sublanguage is specified, check it too */
            if (meta->subLangId == 0 || meta->subLangId == subLang) {
                CURRENT_LANGUAGE = meta->language;
                return;
            }
        }
    }
    
    /** Default to English if no match found */
    CURRENT_LANGUAGE = APP_LANG_ENGLISH;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Get localized string for current language
 * 
 * Implements a three-tier fallback strategy:
 * 1. For Chinese languages: use Chinese text directly
 * 2. For other languages: look up translation in loaded resources
 * 3. Final fallback: use English text
 * 
 * @param chinese Chinese text (used directly for Chinese languages)
 * @param english English text (used as lookup key and final fallback)
 * @return Pointer to appropriate localized string (never NULL)
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english) {
    /** Initialize language system on first call */
    if (!g_initialized) {
        DetectSystemLanguage();
        LoadLanguageResource(CURRENT_LANGUAGE);
        g_initialized = TRUE;
    }
    
    /** Use Chinese text directly for Chinese languages if available */
    if (chinese && g_languageMetadata[CURRENT_LANGUAGE].useDirectChinese) {
        return chinese;
    }
    
    /** Look up translation for non-Chinese languages */
    const wchar_t* translation = FindTranslation(english);
    if (translation) {
        return translation;
    }
    
    /** Fall back to English */
    return english;
}

/**
 * @brief Set application language and reload translations
 * 
 * @param language Language to activate
 * @return TRUE if successful, FALSE if invalid language
 */
BOOL SetLanguage(AppLanguage language) {
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }
    
    CURRENT_LANGUAGE = language;
    g_translation_count = 0;
    g_initialized = TRUE;
    
    return LoadLanguageResource(language);
}

/**
 * @brief Get current application language
 * 
 * @return Current language enumeration value
 */
AppLanguage GetCurrentLanguage(void) {
    return CURRENT_LANGUAGE;
}

/**
 * @brief Get current language name as locale code string
 * 
 * Uses metadata table to retrieve the standard locale code for the current language.
 * 
 * @param buffer Output buffer for language code
 * @param bufferSize Size of output buffer in wide characters
 * @return TRUE if successful, FALSE if invalid parameters
 */
BOOL GetCurrentLanguageName(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }
    
    AppLanguage language = GetCurrentLanguage();
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }
    
    /** Retrieve locale code from metadata table */
    wcscpy_s(buffer, bufferSize, g_languageMetadata[language].localeCode);
    
    return TRUE;
}
