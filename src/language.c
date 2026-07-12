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
#ifdef CATIME_USE_WIN32_FLS
#include "utils/thread_local_buffer.h"
#endif
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
#include "utils/compressed_resource.h"
#endif
#include "../resource/resource.h"

#define MAX_TRANSLATIONS 600
#define MAX_STRING_LENGTH 1536
#define LOCALIZED_RETURN_SLOT_COUNT 32

typedef struct {
    wchar_t* english;
    wchar_t* translation;
    BOOL ownsEnglish;
    BOOL ownsTranslation;
} LocalizedString;

typedef struct {
    LocalizedString entries[MAX_TRANSLATIONS];
    int count;
    BOOL loaded;
} TranslationTable;

typedef struct {
    AppLanguage language;
    UINT resourceId;
    WORD primaryLangId;
    WORD subLangId;
    const wchar_t* localeCode;
    const char* configKey;
    BOOL useDirectChinese;
    AppLanguage fallbackLanguage;
} LanguageMetadata;

AppLanguage CURRENT_LANGUAGE = APP_LANG_ENGLISH;

static TranslationTable g_translationTables[APP_LANG_COUNT] = {0};
static AppLanguage g_activeTranslationLanguage = APP_LANG_ENGLISH;
static BOOL g_initialized = FALSE;
static INIT_ONCE g_languageLockOnce = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_languageCS;

#ifdef CATIME_USE_WIN32_FLS
typedef struct {
    wchar_t buffers[LOCALIZED_RETURN_SLOT_COUNT][MAX_STRING_LENGTH];
    unsigned int nextSlot;
} LocalizedReturnStorage;

static ThreadLocalBuffer g_localizedReturnStorage =
    THREAD_LOCAL_BUFFER_STATIC_INIT(sizeof(LocalizedReturnStorage));
#endif

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

static const wchar_t* CopyLocalizedReturnValue(const wchar_t* value) {
    if (!value) {
        return L"";
    }

#if defined(CATIME_USE_WIN32_FLS)
    LocalizedReturnStorage* storage =
        (LocalizedReturnStorage*)ThreadLocalBuffer_Get(&g_localizedReturnStorage);
    if (!storage) {
        return value;
    }
    wchar_t* slot = storage->buffers[
        storage->nextSlot++ % LOCALIZED_RETURN_SLOT_COUNT];
#elif defined(_MSC_VER)
    __declspec(thread) static wchar_t buffers[LOCALIZED_RETURN_SLOT_COUNT][MAX_STRING_LENGTH];
    __declspec(thread) static unsigned int nextSlot = 0;
#elif defined(__GNUC__)
    static __thread wchar_t buffers[LOCALIZED_RETURN_SLOT_COUNT][MAX_STRING_LENGTH];
    static __thread unsigned int nextSlot = 0;
#else
    static wchar_t buffers[LOCALIZED_RETURN_SLOT_COUNT][MAX_STRING_LENGTH];
    static unsigned int nextSlot = 0;
#endif

#if !defined(CATIME_USE_WIN32_FLS)
    wchar_t* slot = buffers[nextSlot++ % LOCALIZED_RETURN_SLOT_COUNT];
#endif
    wcsncpy_s(slot, MAX_STRING_LENGTH, value, _TRUNCATE);
    return slot;
}

static wchar_t* DuplicateWideSpan(const wchar_t* start, size_t length) {
    if (!start || length >= (size_t)-1 / sizeof(wchar_t)) {
        return NULL;
    }

    wchar_t* copy = (wchar_t*)malloc((length + 1) * sizeof(wchar_t));
    if (!copy) {
        return NULL;
    }

    memcpy(copy, start, length * sizeof(wchar_t));
    copy[length] = L'\0';
    return copy;
}

/** @return Position after closing quote, or NULL */
static const wchar_t* ExtractQuotedString(const wchar_t* start, wchar_t** output) {
    if (!output) return NULL;
    *output = NULL;

    const wchar_t* quote_start = wcschr(start, L'"');
    if (!quote_start) return NULL;
    quote_start++;
    
    const wchar_t* quote_end = wcschr(quote_start, L'"');
    if (!quote_end) return NULL;
    
    size_t length = quote_end - quote_start;
    if (length >= MAX_STRING_LENGTH) length = MAX_STRING_LENGTH - 1;

    *output = DuplicateWideSpan(quote_start, length);
    if (!*output) return NULL;
    
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

static BOOL AddTranslationEntry(TranslationTable* table,
                                wchar_t* english,
                                wchar_t* translation,
                                BOOL ownsEnglish,
                                BOOL ownsTranslation) {
    if (!table || !english || !translation || table->count >= MAX_TRANSLATIONS) {
        if (ownsEnglish) {
            free(english);
        }
        if (ownsTranslation) {
            free(translation);
        }
        return FALSE;
    }

    table->entries[table->count].english = english;
    table->entries[table->count].translation = translation;
    table->entries[table->count].ownsEnglish = ownsEnglish;
    table->entries[table->count].ownsTranslation = ownsTranslation;
    table->count++;
    return TRUE;
}

/** Parse line format: "English Key" = "Translated Value" */
static BOOL ParseIniLine(TranslationTable* table, const wchar_t* line) {
    if (!table) {
        return FALSE;
    }
    if (ShouldSkipLine(line)) {
        return FALSE;
    }
    
    if (table->count >= MAX_TRANSLATIONS) {
        return FALSE;
    }

    wchar_t* english = NULL;
    wchar_t* translation = NULL;

    const wchar_t* pos = ExtractQuotedString(line, &english);
    if (!pos) return FALSE;
    
    pos = wcschr(pos, L'=');
    if (!pos) {
        free(english);
        return FALSE;
    }

    pos = ExtractQuotedString(pos, &translation);
    if (!pos) {
        free(english);
        return FALSE;
    }

    ProcessEscapeSequences(translation);

    return AddTranslationEntry(table, english, translation, TRUE, TRUE);
}

/** Parse compact line format: "Translated Value"; key comes from en.ini order */
static BOOL ParseCompactValueLine(TranslationTable* table,
                                  const TranslationTable* keyTable,
                                  int* keyIndex,
                                  const wchar_t* line) {
    if (!table || !keyTable || !keyIndex) {
        return FALSE;
    }
    if (ShouldSkipLine(line)) {
        return FALSE;
    }
    if (table->count >= MAX_TRANSLATIONS) {
        return FALSE;
    }

    while (*keyIndex < keyTable->count &&
           !keyTable->entries[*keyIndex].english) {
        (*keyIndex)++;
    }
    if (*keyIndex >= keyTable->count) {
        return FALSE;
    }

    wchar_t* translation = NULL;
    const wchar_t* pos = ExtractQuotedString(line, &translation);
    if (!pos) {
        return FALSE;
    }

    ProcessEscapeSequences(translation);

    wchar_t* english = keyTable->entries[*keyIndex].english;
    (*keyIndex)++;

    return AddTranslationEntry(table, english, translation, FALSE, TRUE);
}

static int UTF8ToWideChar(const char* utf8, wchar_t* wstr, int wstr_size) {
    if (!wstr || wstr_size <= 0) {
        return -1;
    }
    wstr[0] = L'\0';
    if (!utf8) {
        return -1;
    }

    int converted = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wstr_size);
    if (converted <= 0) {
        wstr[0] = L'\0';
        return -1;
    }
    return converted - 1;
}

static const char* SkipUTF8BOM(const char* str) {
    return (strncmp(str, "\xEF\xBB\xBF", 3) == 0) ? str + 3 : str;
}

/** @param outBuffer Caller must free */
static BOOL LoadResourceToBuffer(UINT resourceId, char** outBuffer) {
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
    if (!outBuffer) {
        return FALSE;
    }
    *outBuffer = NULL;

    CompressedResourceGroup* group = NULL;
    if (!CompressedResource_LoadGroup(NULL,
                                      COMPRESSED_RESOURCE_GROUP_LANGUAGES,
                                      &group)) {
        return FALSE;
    }

    BOOL result = CompressedResource_CopyTextMember(group, resourceId,
                                                     outBuffer, NULL);
    CompressedResource_FreeGroup(group);
    return result;
#else
    if (!outBuffer) {
        return FALSE;
    }
    *outBuffer = NULL;

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
    
    if (dwSize == MAXDWORD) {
        return FALSE;
    }

    *outBuffer = (char*)malloc((size_t)dwSize + 1);
    if (!*outBuffer) {
        return FALSE;
    }
    
    memcpy(*outBuffer, pData, dwSize);
    (*outBuffer)[dwSize] = '\0';
    
    return TRUE;
#endif
}

static void ParseLanguageBuffer(TranslationTable* table,
                                char* buffer,
                                const TranslationTable* keyTable) {
    if (!table) return;

    wchar_t wide_buffer[MAX_STRING_LENGTH];
    int compactKeyIndex = 0;
    
    const char* line = strtok(buffer, "\r\n");
    
    while (line && table->count < MAX_TRANSLATIONS) {
        if (line[0] == '\0') {
            line = strtok(NULL, "\r\n");
            continue;
        }
        
        line = SkipUTF8BOM(line);
        
        if (UTF8ToWideChar(line, wide_buffer, MAX_STRING_LENGTH) > 0) {
            if (!ParseIniLine(table, wide_buffer) && keyTable) {
                ParseCompactValueLine(table, keyTable, &compactKeyIndex,
                                      wide_buffer);
            }
        }
        
        line = strtok(NULL, "\r\n");
    }
}

/** Falls back to metadata's fallbackLanguage on failure */
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
    
    if (!LoadResourceToBuffer(metadata->resourceId, &buffer)) {
        if (metadata->fallbackLanguage != language) {
            return LoadLanguageResource(metadata->fallbackLanguage);
        }
        return FALSE;
    }

    ParseLanguageBuffer(table, buffer,
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
        const wchar_t* copy = CopyLocalizedReturnValue(translation);
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
