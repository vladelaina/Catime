/**
 * @file language_def.h
 * @brief Centralized definition of all supported languages using X-Macros.
 * 
 * To add a new language:
 * 1. Add a line here defining the language properties.
 * 2. Add the corresponding IDM_LANG_XXX and LANG_XXX_INI in resource.h
 * 3. Add the .ini file reference in resource/languages.rc
 * 
 * Columns:
 * - Enum: Internal enumeration name (APP_LANG_XXX)
 * - Locale: String code for locale (e.g., "en", "zh_CN")
 * - NativeName: Display name in the language itself (wide string)
 * - EnglishDesc: English name for description (string)
 * - ConfigKey: String key used in config.ini (no spaces)
 * - ResId: Resource ID for the .ini file (defined in resource.h)
 * - MenuId: Menu command ID (defined in resource.h)
 * - WinPrimaryLang: Windows Primary Language ID (LANG_XXX)
 * - WinSubLang: Windows Sub Language ID (SUBLANG_XXX or 0)
 * - DirectZh: Boolean, TRUE if this language uses Chinese text structure/fallback
 */

/* 
 * The order here determines the order in the menu.
 */
#define LANGUAGE_LIST \
    X(APP_LANG_CHINESE_SIMP, "zh_CN",   L"简体中文",  "Chinese Simplified",  "Chinese_Simplified",  LANG_ZH_CN_INI, CLOCK_IDM_LANG_CHINESE,      LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED,  TRUE) \
    X(APP_LANG_CHINESE_TRAD, "zh-Hant", L"繁體中文",  "Chinese Traditional", "Chinese_Traditional", LANG_ZH_TW_INI, CLOCK_IDM_LANG_CHINESE_TRAD, LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL, TRUE) \
    X(APP_LANG_ENGLISH,      "en",      L"English",   "English",             "English",             LANG_EN_INI,    CLOCK_IDM_LANG_ENGLISH,      LANG_ENGLISH, SUBLANG_NEUTRAL,              FALSE) \
    X(APP_LANG_FRENCH,       "fr",      L"Français",  "French",              "French",              LANG_FR_INI,    CLOCK_IDM_LANG_FRENCH,       LANG_FRENCH,  SUBLANG_NEUTRAL,              FALSE) \
    X(APP_LANG_GERMAN,       "de",      L"Deutsch",   "German",              "German",              LANG_DE_INI,    CLOCK_IDM_LANG_GERMAN,       LANG_GERMAN,  SUBLANG_NEUTRAL,              FALSE) \
    X(APP_LANG_ITALIAN,      "it",      L"Italiano",  "Italian",             "Italian",             LANG_IT_INI,    CLOCK_IDM_LANG_ITALIAN,      LANG_ITALIAN, SUBLANG_NEUTRAL,              FALSE) \
    X(APP_LANG_JAPANESE,     "ja",      L"日本語",     "Japanese",            "Japanese",            LANG_JA_INI,    CLOCK_IDM_LANG_JAPANESE,     LANG_JAPANESE, SUBLANG_NEUTRAL,              FALSE) \
    X(APP_LANG_KOREAN,       "ko",      L"한국어",     "Korean",              "Korean",              LANG_KO_INI,    CLOCK_IDM_LANG_KOREAN,       LANG_KOREAN,  SUBLANG_NEUTRAL,              FALSE) \
    X(APP_LANG_PORTUGUESE,   "pt",      L"Português", "Portuguese",          "Portuguese",          LANG_PT_INI,    CLOCK_IDM_LANG_PORTUGUESE,   LANG_PORTUGUESE, SUBLANG_NEUTRAL,           FALSE) \
    X(APP_LANG_RUSSIAN,      "ru",      L"Русский",   "Russian",             "Russian",             LANG_RU_INI,    CLOCK_IDM_LANG_RUSSIAN,      LANG_RUSSIAN, SUBLANG_NEUTRAL,              FALSE) \
    X(APP_LANG_SPANISH,      "es",      L"Español",   "Spanish",             "Spanish",             LANG_ES_INI,    CLOCK_IDM_LANG_SPANISH,      LANG_SPANISH, SUBLANG_NEUTRAL,              FALSE)