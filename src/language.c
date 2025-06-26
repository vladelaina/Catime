/**
 * @file language.c
 * @brief Multilingual support module implementation
 * 
 * This file implements the multilingual support functionality for the application, 
 * including language detection and localized string handling.
 * Translation content is embedded as resources in the executable file.
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include "../include/language.h"
#include "../resource/resource.h"

/// Global language variable, stores the current application language setting
AppLanguage CURRENT_LANGUAGE = APP_LANG_ENGLISH;  // Default to English

/// Global hash table for storing translations of the current language
#define MAX_TRANSLATIONS 200
#define MAX_STRING_LENGTH 1024

// Language resource IDs (defined in languages.rc)
#define LANG_EN_INI       1001  // Corresponds to languages/en.ini
#define LANG_ZH_CN_INI    1002  // Corresponds to languages/zh_CN.ini
#define LANG_ZH_TW_INI    1003  // Corresponds to languages/zh-Hant.ini
#define LANG_ES_INI       1004  // Corresponds to languages/es.ini
#define LANG_FR_INI       1005  // Corresponds to languages/fr.ini
#define LANG_DE_INI       1006  // Corresponds to languages/de.ini
#define LANG_RU_INI       1007  // Corresponds to languages/ru.ini
#define LANG_PT_INI       1008  // Corresponds to languages/pt.ini
#define LANG_JA_INI       1009  // Corresponds to languages/ja.ini
#define LANG_KO_INI       1010  // Corresponds to languages/ko.ini

/**
 * @brief Define language string key-value pair structure
 */
typedef struct {
    wchar_t english[MAX_STRING_LENGTH];  // English key
    wchar_t translation[MAX_STRING_LENGTH];  // Translated value
} LocalizedString;

static LocalizedString g_translations[MAX_TRANSLATIONS];
static int g_translation_count = 0;

/**
 * @brief Get the resource ID corresponding to a language
 * 
 * @param language Language enumeration value
 * @return UINT Corresponding resource ID
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
 * @brief Convert UTF-8 string to wide character (UTF-16) string
 * 
 * @param utf8 UTF-8 string
 * @param wstr Output wide character string buffer
 * @param wstr_size Buffer size (in characters)
 * @return int Number of characters after conversion, returns -1 if failed
 */
static int UTF8ToWideChar(const char* utf8, wchar_t* wstr, int wstr_size) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wstr_size) - 1;
}

/**
 * @brief Parse a line in an ini file
 * 
 * @param line A line from the ini file
 * @return int Whether parsing was successful (1 for success, 0 for failure)
 */
static int ParseIniLine(const wchar_t* line) {
    // Skip empty lines and comment lines
    if (line[0] == L'\0' || line[0] == L';' || line[0] == L'[') {
        return 0;
    }

    // Find content between the first and last quotes as the key
    const wchar_t* key_start = wcschr(line, L'"');
    if (!key_start) return 0;
    key_start++; // Skip the first quote

    const wchar_t* key_end = wcschr(key_start, L'"');
    if (!key_end) return 0;

    // Find content between the first and last quotes after the equal sign as the value
    const wchar_t* value_start = wcschr(key_end + 1, L'=');
    if (!value_start) return 0;
    
    value_start = wcschr(value_start, L'"');
    if (!value_start) return 0;
    value_start++; // Skip the first quote

    const wchar_t* value_end = wcsrchr(value_start, L'"');
    if (!value_end) return 0;

    // Copy key
    size_t key_len = key_end - key_start;
    if (key_len >= MAX_STRING_LENGTH) key_len = MAX_STRING_LENGTH - 1;
    wcsncpy(g_translations[g_translation_count].english, key_start, key_len);
    g_translations[g_translation_count].english[key_len] = L'\0';

    // Copy value
    size_t value_len = value_end - value_start;
    if (value_len >= MAX_STRING_LENGTH) value_len = MAX_STRING_LENGTH - 1;
    wcsncpy(g_translations[g_translation_count].translation, value_start, value_len);
    g_translations[g_translation_count].translation[value_len] = L'\0';

    g_translation_count++;
    return 1;
}

/**
 * @brief Load translations for a specified language from resources
 * 
 * @param language Language enumeration value
 * @return int Whether loading was successful
 */
static int LoadLanguageResource(AppLanguage language) {
    UINT resourceID = GetLanguageResourceID(language);
    
    // Reset translation count
    g_translation_count = 0;
    
    // Find resource
    HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(resourceID), RT_RCDATA);
    if (!hResInfo) {
        // If not found, check if it's Chinese and return
        if (language == APP_LANG_CHINESE_SIMP || language == APP_LANG_CHINESE_TRAD) {
            return 0;
        }
        
        // If not Chinese, load English as fallback
        if (language != APP_LANG_ENGLISH) {
            return LoadLanguageResource(APP_LANG_ENGLISH);
        }
        
        return 0;
    }
    
    // Get resource size
    DWORD dwSize = SizeofResource(NULL, hResInfo);
    if (dwSize == 0) {
        return 0;
    }
    
    // Load resource
    HGLOBAL hResData = LoadResource(NULL, hResInfo);
    if (!hResData) {
        return 0;
    }
    
    // Lock resource to get pointer
    const char* pData = (const char*)LockResource(hResData);
    if (!pData) {
        return 0;
    }
    
    // Create memory buffer copy
    char* buffer = (char*)malloc(dwSize + 1);
    if (!buffer) {
        return 0;
    }
    
    // Copy resource data to buffer
    memcpy(buffer, pData, dwSize);
    buffer[dwSize] = '\0';
    
    // Split by lines and parse
    char* line = strtok(buffer, "\r\n");
    wchar_t wide_buffer[MAX_STRING_LENGTH];
    
    while (line && g_translation_count < MAX_TRANSLATIONS) {
        // Skip empty lines and BOM markers
        if (line[0] == '\0' || (line[0] == (char)0xEF && line[1] == (char)0xBB && line[2] == (char)0xBF)) {
            line = strtok(NULL, "\r\n");
            continue;
        }
        
        // Convert to wide characters
        if (UTF8ToWideChar(line, wide_buffer, MAX_STRING_LENGTH) > 0) {
            ParseIniLine(wide_buffer);
        }
        
        line = strtok(NULL, "\r\n");
    }
    
    free(buffer);
    return 1;
}

/**
 * @brief Find corresponding translation in the global translation table
 * 
 * @param english English original text
 * @return const wchar_t* Found translation, returns NULL if not found
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
 * @brief Initialize the application language environment
 * 
 * Automatically detect and set the current language of the application based on system language.
 * Supports detection of Simplified Chinese, Traditional Chinese, and other preset languages.
 */
static void DetectSystemLanguage() {
    LANGID langID = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(langID)) {
        case LANG_CHINESE:
            // Distinguish between Simplified and Traditional Chinese
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
            CURRENT_LANGUAGE = APP_LANG_ENGLISH;  // Default fallback to English
    }
}

/**
 * @brief Get localized string
 * @param chinese Simplified Chinese version of the string
 * @param english English version of the string
 * @return const wchar_t* Pointer to the string corresponding to the current language
 * 
 * Returns the string in the corresponding language based on the current language setting.
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english) {
    // Initialize translation resources on first call, but don't automatically detect system language
    static BOOL initialized = FALSE;
    if (!initialized) {
        // No longer call DetectSystemLanguage() to automatically detect system language
        // Instead, use the currently set CURRENT_LANGUAGE value (possibly from a configuration file)
        LoadLanguageResource(CURRENT_LANGUAGE);
        initialized = TRUE;
    }

    const wchar_t* translation = NULL;

    // If Simplified Chinese and Chinese string is provided, return directly
    if (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP && chinese) {
        return chinese;
    }

    // Find translation
    translation = FindTranslation(english);
    if (translation) {
        return translation;
    }

    // For Traditional Chinese but no translation found, return Simplified Chinese as a fallback
    if (CURRENT_LANGUAGE == APP_LANG_CHINESE_TRAD && chinese) {
        return chinese;
    }

    // Default to English
    return english;
}

/**
 * @brief Set application language
 * 
 * @param language The language to set
 * @return BOOL Whether the setting was successful
 */
BOOL SetLanguage(AppLanguage language) {
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }
    
    CURRENT_LANGUAGE = language;
    g_translation_count = 0;  // Clear existing translations
    return LoadLanguageResource(language);
}

/**
 * @brief Get current application language
 * 
 * @return AppLanguage Current language
 */
AppLanguage GetCurrentLanguage() {
    return CURRENT_LANGUAGE;
}

/**
 * @brief Get the name of the current language
 * @param buffer Buffer to store the language name
 * @param bufferSize Buffer size (in characters)
 * @return Whether the language name was successfully retrieved
 */
BOOL GetCurrentLanguageName(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }
    
    // Get current language
    AppLanguage language = GetCurrentLanguage();
    
    // Return corresponding name based on language enumeration
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
