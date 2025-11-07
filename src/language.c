/**
 * @file language.c
 * @brief Multi-language support with metadata-driven translations
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include "language.h"
#include "../resource/resource.h"

#define MAX_TRANSLATIONS 200
#define MAX_STRING_LENGTH 1024

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

typedef struct {
    wchar_t english[MAX_STRING_LENGTH];
    wchar_t translation[MAX_STRING_LENGTH];
} LocalizedString;

typedef struct {
    AppLanguage language;
    UINT resourceId;
    WORD primaryLangId;
    WORD subLangId;
    const wchar_t* localeCode;
    BOOL useDirectChinese;
    AppLanguage fallbackLanguage;
} LanguageMetadata;

AppLanguage CURRENT_LANGUAGE = APP_LANG_ENGLISH;

static LocalizedString g_translations[MAX_TRANSLATIONS];
static int g_translation_count = 0;
static BOOL g_initialized = FALSE;

/** Adding a new language = one entry here */
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

/** @return Position after closing quote, or NULL */
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

/** In-place conversion of \n, \t, \\ escape sequences */
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

static BOOL ShouldSkipLine(const wchar_t* line) {
    return (line[0] == L'\0' || line[0] == L';' || line[0] == L'[');
}

/** Parse line format: "English Key" = "Translated Value" */
static BOOL ParseIniLine(const wchar_t* line) {
    if (ShouldSkipLine(line)) {
        return FALSE;
    }
    
    if (g_translation_count >= MAX_TRANSLATIONS) {
        return FALSE;
    }
    
    const wchar_t* pos = ExtractQuotedString(line, 
                                            g_translations[g_translation_count].english,
                                            MAX_STRING_LENGTH);
    if (!pos) return FALSE;
    
    pos = wcschr(pos, L'=');
    if (!pos) return FALSE;
    
    pos = ExtractQuotedString(pos, 
                             g_translations[g_translation_count].translation,
                             MAX_STRING_LENGTH);
    if (!pos) return FALSE;
    
    ProcessEscapeSequences(g_translations[g_translation_count].translation);
    
    g_translation_count++;
    return TRUE;
}

static int UTF8ToWideChar(const char* utf8, wchar_t* wstr, int wstr_size) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wstr_size) - 1;
}

static const char* SkipUTF8BOM(const char* str) {
    if (str[0] == (char)0xEF && str[1] == (char)0xBB && str[2] == (char)0xBF) {
        return str + 3;
    }
    return str;
}

/** @param outBuffer Caller must free */
static BOOL LoadResourceToBuffer(UINT resourceId, char** outBuffer) {
    HRSRC hResInfo = FindResourceW(NULL, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hResInfo) {
        return FALSE;
    }
    
    DWORD dwSize = SizeofResource(NULL, hResInfo);
    if (dwSize == 0) {
        return FALSE;
    }
    
    HGLOBAL hResData = LoadResource(NULL, hResInfo);
    if (!hResData) {
        return FALSE;
    }
    
    const char* pData = (const char*)LockResource(hResData);
    if (!pData) {
        return FALSE;
    }
    
    *outBuffer = (char*)malloc(dwSize + 1);
    if (!*outBuffer) {
        return FALSE;
    }
    
    memcpy(*outBuffer, pData, dwSize);
    (*outBuffer)[dwSize] = '\0';
    
    return TRUE;
}

static void ParseLanguageBuffer(char* buffer) {
    wchar_t wide_buffer[MAX_STRING_LENGTH];
    
    char* line = strtok(buffer, "\r\n");
    
    while (line && g_translation_count < MAX_TRANSLATIONS) {
        if (line[0] == '\0') {
            line = strtok(NULL, "\r\n");
            continue;
        }
        
        line = (char*)SkipUTF8BOM(line);
        
        if (UTF8ToWideChar(line, wide_buffer, MAX_STRING_LENGTH) > 0) {
            ParseIniLine(wide_buffer);
        }
        
        line = strtok(NULL, "\r\n");
    }
}

/** Falls back to metadata's fallbackLanguage on failure */
static BOOL LoadLanguageResource(AppLanguage language) {
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }
    
    g_translation_count = 0;
    
    const LanguageMetadata* metadata = &g_languageMetadata[language];
    char* buffer = NULL;
    
    if (!LoadResourceToBuffer(metadata->resourceId, &buffer)) {
        if (metadata->fallbackLanguage != language) {
            return LoadLanguageResource(metadata->fallbackLanguage);
        }
        return FALSE;
    }
    
    ParseLanguageBuffer(buffer);
    
    free(buffer);
    return TRUE;
}

/** Linear search (faster than hash table for ~200 entries) */
static const wchar_t* FindTranslation(const wchar_t* english) {
    for (int i = 0; i < g_translation_count; i++) {
        if (wcscmp(english, g_translations[i].english) == 0) {
            return g_translations[i].translation;
        }
    }
    return NULL;
}

static void DetectSystemLanguage(void) {
    LANGID langID = GetUserDefaultUILanguage();
    WORD primaryLang = PRIMARYLANGID(langID);
    WORD subLang = SUBLANGID(langID);
    
    for (int i = 0; i < APP_LANG_COUNT; i++) {
        const LanguageMetadata* meta = &g_languageMetadata[i];
        
        if (meta->primaryLangId == primaryLang) {
            if (meta->subLangId == 0 || meta->subLangId == subLang) {
                CURRENT_LANGUAGE = meta->language;
                return;
            }
        }
    }
    
    CURRENT_LANGUAGE = APP_LANG_ENGLISH;
}

/**
 * Three-tier fallback: Chinese direct → lookup → English
 * @return Never NULL
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english) {
    if (!g_initialized) {
        DetectSystemLanguage();
        LoadLanguageResource(CURRENT_LANGUAGE);
        g_initialized = TRUE;
    }
    
    if (chinese && g_languageMetadata[CURRENT_LANGUAGE].useDirectChinese) {
        return chinese;
    }
    
    const wchar_t* translation = FindTranslation(english);
    if (translation) {
        return translation;
    }
    
    return english;
}

BOOL SetLanguage(AppLanguage language) {
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }
    
    CURRENT_LANGUAGE = language;
    g_translation_count = 0;
    g_initialized = TRUE;
    
    return LoadLanguageResource(language);
}

AppLanguage GetCurrentLanguage(void) {
    return CURRENT_LANGUAGE;
}

BOOL GetCurrentLanguageName(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }
    
    AppLanguage language = GetCurrentLanguage();
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }
    
    wcscpy_s(buffer, bufferSize, g_languageMetadata[language].localeCode);
    
    return TRUE;
}
