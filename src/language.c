#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include "../include/language.h"
#include "../resource/resource.h"

AppLanguage CURRENT_LANGUAGE = APP_LANG_ENGLISH;

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

static LocalizedString g_translations[MAX_TRANSLATIONS];
static int g_translation_count = 0;

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

static int UTF8ToWideChar(const char* utf8, wchar_t* wstr, int wstr_size) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wstr_size) - 1;
}

static int ParseIniLine(const wchar_t* line) {
    if (line[0] == L'\0' || line[0] == L';' || line[0] == L'[') {
        return 0;
    }

    const wchar_t* key_start = wcschr(line, L'"');
    if (!key_start) return 0;
    key_start++;

    const wchar_t* key_end = wcschr(key_start, L'"');
    if (!key_end) return 0;

    const wchar_t* value_start = wcschr(key_end + 1, L'=');
    if (!value_start) return 0;
    
    value_start = wcschr(value_start, L'"');
    if (!value_start) return 0;
    value_start++;

    const wchar_t* value_end = wcsrchr(value_start, L'"');
    if (!value_end) return 0;

    size_t key_len = key_end - key_start;
    if (key_len >= MAX_STRING_LENGTH) key_len = MAX_STRING_LENGTH - 1;
    wcsncpy(g_translations[g_translation_count].english, key_start, key_len);
    g_translations[g_translation_count].english[key_len] = L'\0';

    size_t value_len = value_end - value_start;
    if (value_len >= MAX_STRING_LENGTH) value_len = MAX_STRING_LENGTH - 1;
    wcsncpy(g_translations[g_translation_count].translation, value_start, value_len);
    g_translations[g_translation_count].translation[value_len] = L'\0';

    g_translation_count++;
    return 1;
}

static int LoadLanguageResource(AppLanguage language) {
    UINT resourceID = GetLanguageResourceID(language);
    
    g_translation_count = 0;
    
    HRSRC hResInfo = FindResourceW(NULL, MAKEINTRESOURCE(resourceID), RT_RCDATA);
    if (!hResInfo) {
        if (language == APP_LANG_CHINESE_SIMP || language == APP_LANG_CHINESE_TRAD) {
            return 0;
        }
        
        if (language != APP_LANG_ENGLISH) {
            return LoadLanguageResource(APP_LANG_ENGLISH);
        }
        
        return 0;
    }
    
    DWORD dwSize = SizeofResource(NULL, hResInfo);
    if (dwSize == 0) {
        return 0;
    }
    
    HGLOBAL hResData = LoadResource(NULL, hResInfo);
    if (!hResData) {
        return 0;
    }
    
    const char* pData = (const char*)LockResource(hResData);
    if (!pData) {
        return 0;
    }
    
    char* buffer = (char*)malloc(dwSize + 1);
    if (!buffer) {
        return 0;
    }
    
    memcpy(buffer, pData, dwSize);
    buffer[dwSize] = '\0';
    
    char* line = strtok(buffer, "\r\n");
    wchar_t wide_buffer[MAX_STRING_LENGTH];
    
    while (line && g_translation_count < MAX_TRANSLATIONS) {
        if (line[0] == '\0' || (line[0] == (char)0xEF && line[1] == (char)0xBB && line[2] == (char)0xBF)) {
            line = strtok(NULL, "\r\n");
            continue;
        }
        
        if (UTF8ToWideChar(line, wide_buffer, MAX_STRING_LENGTH) > 0) {
            ParseIniLine(wide_buffer);
        }
        
        line = strtok(NULL, "\r\n");
    }
    
    free(buffer);
    return 1;
}

static const wchar_t* FindTranslation(const wchar_t* english) {
    for (int i = 0; i < g_translation_count; i++) {
        if (wcscmp(english, g_translations[i].english) == 0) {
            return g_translations[i].translation;
        }
    }
    return NULL;
}

static void DetectSystemLanguage() {
    LANGID langID = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(langID)) {
        case LANG_CHINESE:
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

const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english) {
    static BOOL initialized = FALSE;
    if (!initialized) {
        LoadLanguageResource(CURRENT_LANGUAGE);
        initialized = TRUE;
    }

    const wchar_t* translation = NULL;

    if (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP && chinese) {
        return chinese;
    }

    translation = FindTranslation(english);
    if (translation) {
        return translation;
    }

    if (CURRENT_LANGUAGE == APP_LANG_CHINESE_TRAD && chinese) {
        return chinese;
    }

    return english;
}

BOOL SetLanguage(AppLanguage language) {
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }
    
    CURRENT_LANGUAGE = language;
    g_translation_count = 0;
    return LoadLanguageResource(language);
}

AppLanguage GetCurrentLanguage() {
    return CURRENT_LANGUAGE;
}

BOOL GetCurrentLanguageName(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }
    
    AppLanguage language = GetCurrentLanguage();
    
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