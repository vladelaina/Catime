/**
 * @file language.h
 * @brief Multi-language localization system with data-driven architecture
 * 
 * Provides a centralized, metadata-driven approach to application localization.
 * Supports 10 languages with efficient translation lookup and flexible fallback mechanisms.
 */

#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <wchar.h>
#include <windows.h>

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Supported application languages
 */
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
    APP_LANG_ITALIAN,
    APP_LANG_COUNT
} AppLanguage;

/* ============================================================================
 * Global State
 * ============================================================================ */

/** @brief Current active language */
extern AppLanguage CURRENT_LANGUAGE;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Get localized string for current language
 * 
 * Uses Chinese text directly for Chinese languages, looks up translations
 * for other languages, and falls back to English if no translation exists.
 * Automatically initializes language system on first call.
 * 
 * @param chinese Chinese text (used directly for Chinese languages)
 * @param english English text (used as lookup key and final fallback)
 * @return Pointer to appropriate localized string (never NULL)
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english);

/**
 * @brief Set application language and reload translations
 * 
 * Validates language parameter, updates global state, and loads corresponding
 * translation resources. Automatically falls back to English for missing resources.
 * 
 * @param language Language enumeration value to activate
 * @return TRUE if language was set successfully, FALSE for invalid language
 */
BOOL SetLanguage(AppLanguage language);

/**
 * @brief Get current active language
 * @return Current language enumeration value
 */
AppLanguage GetCurrentLanguage(void);

/**
 * @brief Get current language as locale code string
 * 
 * Returns standard locale codes (e.g., "zh_CN", "en", "fr") for the current language.
 * 
 * @param buffer Output buffer for language code string
 * @param bufferSize Size of output buffer in wide characters
 * @return TRUE if successful, FALSE if buffer is NULL or too small
 */
BOOL GetCurrentLanguageName(wchar_t* buffer, size_t bufferSize);

#endif