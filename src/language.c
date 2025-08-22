/**
 * @file language.c
 * @brief Multi-language support and localization system
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include "../include/language.h"
#include "../resource/resource.h"

/** @brief Current application language setting */
AppLanguage CURRENT_LANGUAGE = APP_LANG_ENGLISH;

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

/** @brief Structure to store English-to-localized string mapping */
typedef struct {
    wchar_t english[MAX_STRING_LENGTH];      /**< Original English string */
    wchar_t translation[MAX_STRING_LENGTH];  /**< Localized translation */
} LocalizedString;

/** @brief Array of loaded translation entries */
static LocalizedString g_translations[MAX_TRANSLATIONS];
/** @brief Number of currently loaded translations */
static int g_translation_count = 0;

/**
 * @brief Get resource ID for language-specific INI file
 * @param language Language enumeration value
 * @return Resource ID of the corresponding language file
 */
static UINT GetLanguageResourceID(AppLanguage language) {
    switch (language) {
        case APP_LANG_CHINESE_SIMP:
            return LANG_ZH_CN_INI;
        case APP_LANG_CHINESE_TRAD:
            return LANG_ZH_TW_INI;
        case APP_LANG_SPANISH:
            return LANG_ES_INI;
        case APP_LANG_FRENCH:
            return LANG_FR_INI;
        case APP_LANG_GERMAN:
            return LANG_DE_INI;
        case APP_LANG_RUSSIAN:
            return LANG_RU_INI;
        case APP_LANG_PORTUGUESE:
            return LANG_PT_INI;
        case APP_LANG_JAPANESE:
            return LANG_JA_INI;
        case APP_LANG_KOREAN:
            return LANG_KO_INI;
        case APP_LANG_ENGLISH:
        default:
            return LANG_EN_INI;
    }
}

/**
 * @brief Convert UTF-8 string to wide character string
 * @param utf8 Input UTF-8 string
 * @param wstr Output wide character buffer
 * @param wstr_size Size of output buffer
 * @return Number of characters converted (excluding null terminator)
 */
static int UTF8ToWideChar(const char* utf8, wchar_t* wstr, int wstr_size) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wstr_size) - 1;
}

/**
 * @brief Parse a single INI file line and extract key-value pair
 * @param line Wide character line to parse
 * @return 1 if successfully parsed, 0 if skipped or failed
 */
static int ParseIniLine(const wchar_t* line) {
    /** Skip empty lines, comments, and section headers */
    if (line[0] == L'\0' || line[0] == L';' || line[0] == L'[') {
        return 0;
    }

    /** Find English key enclosed in quotes */
    const wchar_t* key_start = wcschr(line, L'"');
    if (!key_start) return 0;
    key_start++;

    const wchar_t* key_end = wcschr(key_start, L'"');
    if (!key_end) return 0;

    /** Find equals sign and translation value */
    const wchar_t* value_start = wcschr(key_end + 1, L'=');
    if (!value_start) return 0;
    
    value_start = wcschr(value_start, L'"');
    if (!value_start) return 0;
    value_start++;

    const wchar_t* value_end = wcsrchr(value_start, L'"');
    if (!value_end) return 0;

    /** Extract and store English key */
    size_t key_len = key_end - key_start;
    if (key_len >= MAX_STRING_LENGTH) key_len = MAX_STRING_LENGTH - 1;
    wcsncpy(g_translations[g_translation_count].english, key_start, key_len);
    g_translations[g_translation_count].english[key_len] = L'\0';

    /** Extract and store translation value */
    size_t value_len = value_end - value_start;
    if (value_len >= MAX_STRING_LENGTH) value_len = MAX_STRING_LENGTH - 1;
    wcsncpy(g_translations[g_translation_count].translation, value_start, value_len);
    g_translations[g_translation_count].translation[value_len] = L'\0';

    /** Process escape sequences in translation value */
    wchar_t* src = g_translations[g_translation_count].translation;
    wchar_t* dst = g_translations[g_translation_count].translation;
    
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

    g_translation_count++;
    return 1;
}

/**
 * @brief Load language translations from embedded resource
 * @param language Language to load
 * @return 1 if successful, 0 if failed
 */
static int LoadLanguageResource(AppLanguage language) {
    UINT resourceID = GetLanguageResourceID(language);
    
    /** Reset translation count for new language */
    g_translation_count = 0;
    
    /** Find language resource in executable */
    HRSRC hResInfo = FindResourceW(NULL, MAKEINTRESOURCE(resourceID), RT_RCDATA);
    if (!hResInfo) {
        /** Chinese languages don't fall back to English */
        if (language == APP_LANG_CHINESE_SIMP || language == APP_LANG_CHINESE_TRAD) {
            return 0;
        }
        
        /** Other languages fall back to English if not found */
        if (language != APP_LANG_ENGLISH) {
            return LoadLanguageResource(APP_LANG_ENGLISH);
        }
        
        return 0;
    }
    
    /** Get resource size */
    DWORD dwSize = SizeofResource(NULL, hResInfo);
    if (dwSize == 0) {
        return 0;
    }
    
    /** Load resource into memory */
    HGLOBAL hResData = LoadResource(NULL, hResInfo);
    if (!hResData) {
        return 0;
    }
    
    /** Lock resource data for access */
    const char* pData = (const char*)LockResource(hResData);
    if (!pData) {
        return 0;
    }
    
    /** Create null-terminated buffer for parsing */
    char* buffer = (char*)malloc(dwSize + 1);
    if (!buffer) {
        return 0;
    }
    
    memcpy(buffer, pData, dwSize);
    buffer[dwSize] = '\0';
    
    /** Parse line by line */
    char* line = strtok(buffer, "\r\n");
    wchar_t wide_buffer[MAX_STRING_LENGTH];
    
    while (line && g_translation_count < MAX_TRANSLATIONS) {
        /** Skip empty lines and UTF-8 BOM */
        if (line[0] == '\0' || (line[0] == (char)0xEF && line[1] == (char)0xBB && line[2] == (char)0xBF)) {
            line = strtok(NULL, "\r\n");
            continue;
        }
        
        /** Convert UTF-8 to wide char and parse */
        if (UTF8ToWideChar(line, wide_buffer, MAX_STRING_LENGTH) > 0) {
            ParseIniLine(wide_buffer);
        }
        
        line = strtok(NULL, "\r\n");
    }
    
    free(buffer);
    return 1;
}

/**
 * @brief Find translation for an English string
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

/**
 * @brief Detect system language and set as current language
 */
static void DetectSystemLanguage() {
    LANGID langID = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(langID)) {
        case LANG_CHINESE:
            /** Distinguish between Simplified and Traditional Chinese */
            if (SUBLANGID(langID) == SUBLANG_CHINESE_SIMPLIFIED) {
                CURRENT_LANGUAGE = APP_LANG_CHINESE_SIMP;
            } else {
                CURRENT_LANGUAGE = APP_LANG_CHINESE_TRAD;
            }
            break;
        case LANG_SPANISH:
            CURRENT_LANGUAGE = APP_LANG_SPANISH;
            break;
        case LANG_FRENCH:
            CURRENT_LANGUAGE = APP_LANG_FRENCH;
            break;
        case LANG_GERMAN:
            CURRENT_LANGUAGE = APP_LANG_GERMAN;
            break;
        case LANG_RUSSIAN:
            CURRENT_LANGUAGE = APP_LANG_RUSSIAN;
            break;
        case LANG_PORTUGUESE:
            CURRENT_LANGUAGE = APP_LANG_PORTUGUESE;
            break;
        case LANG_JAPANESE:
            CURRENT_LANGUAGE = APP_LANG_JAPANESE;
            break;
        case LANG_KOREAN:
            CURRENT_LANGUAGE = APP_LANG_KOREAN;
            break;
        default:
            CURRENT_LANGUAGE = APP_LANG_ENGLISH;
    }
}

/**
 * @brief Get localized string for current language
 * @param chinese Chinese text (used for Chinese languages)
 * @param english English text (used as key and fallback)
 * @return Pointer to appropriate localized string
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english) {
    /** Initialize language resources on first call */
    static BOOL initialized = FALSE;
    if (!initialized) {
        LoadLanguageResource(CURRENT_LANGUAGE);
        initialized = TRUE;
    }

    const wchar_t* translation = NULL;

    /** Return Chinese text directly for Simplified Chinese */
    if (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP && chinese) {
        return chinese;
    }

    /** Look up translation for other languages */
    translation = FindTranslation(english);
    if (translation) {
        return translation;
    }

    /** Return Chinese text for Traditional Chinese if no translation found */
    if (CURRENT_LANGUAGE == APP_LANG_CHINESE_TRAD && chinese) {
        return chinese;
    }

    /** Fall back to English */
    return english;
}

/**
 * @brief Set application language and reload translations
 * @param language Language to set
 * @return TRUE if successful, FALSE if invalid language
 */
BOOL SetLanguage(AppLanguage language) {
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }
    
    CURRENT_LANGUAGE = language;
    g_translation_count = 0;
    return LoadLanguageResource(language);
}

/**
 * @brief Get current application language
 * @return Current language enumeration value
 */
AppLanguage GetCurrentLanguage() {
    return CURRENT_LANGUAGE;
}

/**
 * @brief Get current language name as locale code
 * @param buffer Output buffer for language code
 * @param bufferSize Size of output buffer
 * @return TRUE if successful, FALSE if invalid parameters
 */
BOOL GetCurrentLanguageName(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }
    
    AppLanguage language = GetCurrentLanguage();
    
    /** Convert language enum to standard locale codes */
    switch (language) {
        case APP_LANG_CHINESE_SIMP:
            wcscpy_s(buffer, bufferSize, L"zh_CN");
            break;
        case APP_LANG_CHINESE_TRAD:
            wcscpy_s(buffer, bufferSize, L"zh-Hant");
            break;
        case APP_LANG_SPANISH:
            wcscpy_s(buffer, bufferSize, L"es");
            break;
        case APP_LANG_FRENCH:
            wcscpy_s(buffer, bufferSize, L"fr");
            break;
        case APP_LANG_GERMAN:
            wcscpy_s(buffer, bufferSize, L"de");
            break;
        case APP_LANG_RUSSIAN:
            wcscpy_s(buffer, bufferSize, L"ru");
            break;
        case APP_LANG_PORTUGUESE:
            wcscpy_s(buffer, bufferSize, L"pt");
            break;
        case APP_LANG_JAPANESE:
            wcscpy_s(buffer, bufferSize, L"ja");
            break;
        case APP_LANG_KOREAN:
            wcscpy_s(buffer, bufferSize, L"ko");
            break;
        case APP_LANG_ENGLISH:
        default:
            wcscpy_s(buffer, bufferSize, L"en");
            break;
    }
    
    return TRUE;
}