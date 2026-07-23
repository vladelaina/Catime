#ifndef LANGUAGE_INTERNAL_H
#define LANGUAGE_INTERNAL_H

#include "language.h"

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

const wchar_t* Language_CopyReturnValue(const wchar_t* value);
BOOL Language_LoadResourceBuffer(UINT resourceId, char** buffer);
void Language_ParseBuffer(TranslationTable* table, char* buffer,
                          const TranslationTable* keyTable);

#endif
