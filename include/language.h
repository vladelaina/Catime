/**
 * @file language.h
 * @brief Multi-language localization system
 * 
 * Functions for managing application language settings and localized strings
 */

#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <wchar.h>
#include <windows.h>

/**
 * @brief Supported application languages
 */
typedef enum {
    APP_LANG_CHINESE_SIMP,  /**< Simplified Chinese */
    APP_LANG_CHINESE_TRAD,  /**< Traditional Chinese */
    APP_LANG_ENGLISH,       /**< English */
    APP_LANG_SPANISH,       /**< Spanish */
    APP_LANG_FRENCH,        /**< French */
    APP_LANG_GERMAN,        /**< German */
    APP_LANG_RUSSIAN,       /**< Russian */
    APP_LANG_PORTUGUESE,    /**< Portuguese */
    APP_LANG_JAPANESE,      /**< Japanese */
    APP_LANG_KOREAN,        /**< Korean */
    APP_LANG_COUNT          /**< Total language count */
} AppLanguage;

/** @brief Current active language */
extern AppLanguage CURRENT_LANGUAGE;

/**
 * @brief Get localized string based on current language
 * @param chinese Chinese text (fallback for Chinese languages)
 * @param english English text (fallback for other languages)
 * @return Appropriate localized string
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english);

/**
 * @brief Set application language
 * @param language Language to activate
 * @return TRUE on success, FALSE on failure
 */
BOOL SetLanguage(AppLanguage language);

/**
 * @brief Get current active language
 * @return Current language enumeration value
 */
AppLanguage GetCurrentLanguage(void);

/**
 * @brief Get current language name as string
 * @param buffer Buffer to store language name
 * @param bufferSize Size of buffer in characters
 * @return TRUE on success, FALSE on failure
 */
BOOL GetCurrentLanguageName(wchar_t* buffer, size_t bufferSize);

#endif