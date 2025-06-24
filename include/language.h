/**
 * @file language.h
 * @brief Multilingual support module header file
 * 
 * This file defines language enumerations supported by the application and localized string retrieval interfaces.
 */

#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <wchar.h>
#include <windows.h>

/**
 * @enum AppLanguage
 * @brief Enumeration of languages supported by the application
 * 
 * Defines all language options supported by the application for internationalization functionality.
 */
typedef enum {
    APP_LANG_CHINESE_SIMP,   ///< Simplified Chinese
    APP_LANG_CHINESE_TRAD,   ///< Traditional Chinese
    APP_LANG_ENGLISH,        ///< English
    APP_LANG_SPANISH,        ///< Spanish
    APP_LANG_FRENCH,         ///< French
    APP_LANG_GERMAN,         ///< German
    APP_LANG_RUSSIAN,        ///< Russian
    APP_LANG_PORTUGUESE,     ///< Portuguese
    APP_LANG_JAPANESE,       ///< Japanese
    APP_LANG_KOREAN,         ///< Korean
    APP_LANG_COUNT           ///< Total number of languages, used for range checking
} AppLanguage;

/// Current language used by the application, defaults to automatic detection based on system language
extern AppLanguage CURRENT_LANGUAGE;

/**
 * @brief Get localized string
 * @param chinese Simplified Chinese version of the string
 * @param english English version of the string
 * @return Pointer to the string in the current language setting
 * 
 * Example usage:
 * @code
 * const wchar_t* text = GetLocalizedString(L"你好", L"Hello");
 * @endcode
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english);

/**
 * @brief Set application language
 * @param language The language to set
 * @return Whether the setting was successful
 * 
 * Manually set the application language, automatically reloads the corresponding language translation file.
 */
BOOL SetLanguage(AppLanguage language);

/**
 * @brief Get current application language
 * @return Currently set language
 */
AppLanguage GetCurrentLanguage(void);

/**
 * @brief Get the name of the current language
 * @param buffer Buffer to store the language name
 * @param bufferSize Buffer size (in characters)
 * @return Whether the language name was successfully retrieved
 */
BOOL GetCurrentLanguageName(wchar_t* buffer, size_t bufferSize);

#endif /* LANGUAGE_H */
