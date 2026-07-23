/**
 * @file language.c
 * @brief Multi-language support with metadata-driven translations
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "language.h"
#include "language_internal.h"
#ifdef CATIME_USE_WIN32_FLS
#include "utils/thread_local_buffer.h"
#endif
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
#include "utils/compressed_resource.h"
#endif
#include "../resource/resource.h"

AppLanguage CURRENT_LANGUAGE = APP_LANG_ENGLISH;

static TranslationTable g_translationTables[APP_LANG_COUNT] = {0};
static AppLanguage g_activeTranslationLanguage = APP_LANG_ENGLISH;
static BOOL g_initialized = FALSE;
static INIT_ONCE g_languageLockOnce = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_languageCS;

/** Adding a new language = one entry here (Auto-generated via X-Macro) */
static const LanguageMetadata g_languageMetadata[APP_LANG_COUNT] = {
#define X(Enum, Code, Native, Eng, ConfigKey, ResId, MenuId, PId, SId, DirectZh) \
    {Enum, ResId, PId, SId, L##Code, ConfigKey, DirectZh, APP_LANG_ENGLISH},
#include "language_def.h"
    LANGUAGE_LIST
#undef X
};

static BOOL CALLBACK InitLanguageLock(PINIT_ONCE initOnce,
                                      PVOID parameter,
                                      PVOID* context) {
    (void)initOnce;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&g_languageCS);
    return TRUE;
}

static BOOL BeginLanguageStateUse(void) {
    if (!InitOnceExecuteOnce(&g_languageLockOnce, InitLanguageLock, NULL, NULL)) {
        return FALSE;
    }
    EnterCriticalSection(&g_languageCS);
    return TRUE;
}

static void EndLanguageStateUse(void) {
    LeaveCriticalSection(&g_languageCS);
}

static void ClearTranslationTable(TranslationTable* table) {
    if (!table) return;

    for (int i = 0; i < table->count; i++) {
        if (table->entries[i].ownsEnglish) {
            free(table->entries[i].english);
        }
        if (table->entries[i].ownsTranslation) {
            free(table->entries[i].translation);
        }
    }
    ZeroMemory(table, sizeof(*table));
}

static BOOL LoadLanguageResource(AppLanguage language) {
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }

    TranslationTable* table = &g_translationTables[language];
    if (table->loaded) {
        g_activeTranslationLanguage = language;
        return TRUE;
    }

    const LanguageMetadata* metadata = &g_languageMetadata[language];
    char* buffer = NULL;

    if (language != APP_LANG_ENGLISH &&
        !g_translationTables[APP_LANG_ENGLISH].loaded) {
        AppLanguage previousActive = g_activeTranslationLanguage;
        if (!LoadLanguageResource(APP_LANG_ENGLISH)) {
            return FALSE;
        }
        g_activeTranslationLanguage = previousActive;
    }

    if (!Language_LoadResourceBuffer(metadata->resourceId, &buffer)) {
        if (metadata->fallbackLanguage != language) {
            return LoadLanguageResource(metadata->fallbackLanguage);
        }
        return FALSE;
    }

    Language_ParseBuffer(table, buffer,
                        language == APP_LANG_ENGLISH
                            ? NULL
                            : &g_translationTables[APP_LANG_ENGLISH]);
    free(buffer);

    if (table->count <= 0) {
        ClearTranslationTable(table);
        if (metadata->fallbackLanguage != language) {
            return LoadLanguageResource(metadata->fallbackLanguage);
        }
        return FALSE;
    }

    table->loaded = TRUE;
    g_activeTranslationLanguage = language;
    return TRUE;
}

/** Linear search (faster than hash table for ~200 entries) */
static const wchar_t* FindTranslation(const wchar_t* english) {
    if (!english ||
        g_activeTranslationLanguage < 0 ||
        g_activeTranslationLanguage >= APP_LANG_COUNT) {
        return NULL;
    }

    const TranslationTable* table =
        &g_translationTables[g_activeTranslationLanguage];
    for (int i = 0; i < table->count; i++) {
        if (table->entries[i].english &&
            wcscmp(english, table->entries[i].english) == 0) {
            return table->entries[i].translation;
        }
    }
    return NULL;
}

AppLanguage GetSystemDefaultLanguage(void) {
    LANGID langID = GetUserDefaultUILanguage();
    WORD primaryLang = PRIMARYLANGID(langID);
    WORD subLang = SUBLANGID(langID);

    if (primaryLang == LANG_CHINESE) {
        switch (subLang) {
            case SUBLANG_CHINESE_TRADITIONAL:
            case SUBLANG_CHINESE_HONGKONG:
            case SUBLANG_CHINESE_MACAU:
                return APP_LANG_CHINESE_TRAD;
            default:
                return APP_LANG_CHINESE_SIMP;
        }
    }

    for (int i = 0; i < APP_LANG_COUNT; i++) {
        const LanguageMetadata* meta = &g_languageMetadata[i];

        if (meta->primaryLangId == primaryLang) {
            if (meta->subLangId == SUBLANG_NEUTRAL || meta->subLangId == subLang) {
                return meta->language;
            }
        }
    }

    return APP_LANG_ENGLISH;
}

static void DetectSystemLanguage(void) {
    CURRENT_LANGUAGE = GetSystemDefaultLanguage();
}

/**
 * Three-tier fallback: Chinese direct → lookup → English
 * @return Never NULL
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english) {
    const wchar_t* fallback = english ? english : L"";

    if (!BeginLanguageStateUse()) {
        return fallback;
    }

    if (!g_initialized) {
        DetectSystemLanguage();
        LoadLanguageResource(CURRENT_LANGUAGE);
        g_initialized = TRUE;
    }

    AppLanguage language = CURRENT_LANGUAGE;
    if (language < 0 || language >= APP_LANG_COUNT) {
        language = APP_LANG_ENGLISH;
    }

    if (chinese && g_languageMetadata[language].useDirectChinese) {
        EndLanguageStateUse();
        return chinese;
    }

    const wchar_t* translation = FindTranslation(english);
    if (translation) {
        const wchar_t* copy = Language_CopyReturnValue(translation);
        EndLanguageStateUse();
        return copy;
    }

    EndLanguageStateUse();
    return fallback;
}

BOOL SetLanguage(AppLanguage language) {
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }

    if (!BeginLanguageStateUse()) {
        return FALSE;
    }

    CURRENT_LANGUAGE = language;
    g_initialized = TRUE;

    BOOL loaded = LoadLanguageResource(language);
    EndLanguageStateUse();
    return loaded;
}

void CleanupLanguage(void) {
    if (!BeginLanguageStateUse()) {
        return;
    }

    for (int i = 0; i < APP_LANG_COUNT; i++) {
        ClearTranslationTable(&g_translationTables[i]);
    }
    g_activeTranslationLanguage = APP_LANG_ENGLISH;
    g_initialized = FALSE;

    EndLanguageStateUse();
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

const char* GetLanguageConfigKey(AppLanguage language) {
    if (language < 0 || language >= APP_LANG_COUNT) {
        return "English"; /* Default fallback */
    }
    return g_languageMetadata[language].configKey;
}

AppLanguage GetLanguageFromConfigKey(const char* key) {
    if (!key) return APP_LANG_ENGLISH;

    for (int i = 0; i < APP_LANG_COUNT; i++) {
        if (strcmp(key, g_languageMetadata[i].configKey) == 0) {
            return g_languageMetadata[i].language;
        }
    }
    return APP_LANG_ENGLISH;
}
