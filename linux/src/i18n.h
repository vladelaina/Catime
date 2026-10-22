/**
 * @file i18n.h
 * @brief Runtime language loader for the Catime Linux port.
 *
 * Parses resource/languages/<locale>.ini files (format: "EnglishKey"="value").
 * All strings are kept as UTF-8 (GTK/Cairo/Pango expect UTF-8). Lookup falls
 * back to the key itself (the English text) when a translation is missing,
 * matching the original app's English-fallback behavior.
 */
#ifndef CATIME_LINUX_I18N_H
#define CATIME_LINUX_I18N_H

#include <stddef.h>

typedef struct {
    const char *config_key;   /* e.g. "English", "Chinese_Simplified" */
    const char *locale;       /* e.g. "en", "zh_CN" */
    const char *file;         /* e.g. "en.ini", "zh_CN.ini" */
    const char *native_name;  /* UTF-8 display name, e.g. "简体中文" */
} CatimeLanguageInfo;

/** Number of supported languages. */
int i18n_language_count(void);

/** Language table entry for menu building (by index). */
const CatimeLanguageInfo *i18n_language_info(int index);

/** Initialize: detect system locale and load the matching language. */
void i18n_init(void);

/** Load a language by its config key (e.g. "English"). Returns 0 on success. */
int i18n_set_language(const char *config_key);

/** Config key of the currently active language. */
const char *i18n_current_config_key(void);

/** Native display name of the currently active language (UTF-8). */
const char *i18n_current_native_name(void);

/**
 * Look up a translation by its English key.
 * @return the translation, or @p key (the English text) if not found.
 *         Never returns NULL.
 */
const char *i18n_get(const char *key);

/** Convenience macro: tr("Set Countdown") */
#define tr(key) i18n_get(key)

#endif /* CATIME_LINUX_I18N_H */
