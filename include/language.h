#pragma once

#include <wchar.h>

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
    APP_LANG_KOREAN
} AppLanguage;

extern AppLanguage CURRENT_LANGUAGE;

const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english);
