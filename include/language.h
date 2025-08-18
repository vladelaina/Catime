#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <wchar.h>
#include <windows.h>

typedef enum {
    APP_LANG_CHINESE_SIMP,
    APP_LANG_CHINESE_TRAD,
    APP_LANG_ENGLISH,
    APP_LANG_SPANISH,
    APP_LANG_FRENCH,
    APP_LANG_GERMAN,
    APP_LANG_RUSSIAN,
    APP_LANG_PORTUGUESE,
    APP_LANG_JAPANESE,
    APP_LANG_KOREAN,
    APP_LANG_COUNT
} AppLanguage;

extern AppLanguage CURRENT_LANGUAGE;

const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english);

BOOL SetLanguage(AppLanguage language);

AppLanguage GetCurrentLanguage(void);

BOOL GetCurrentLanguageName(wchar_t* buffer, size_t bufferSize);

#endif