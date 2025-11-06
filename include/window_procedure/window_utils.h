/**
 * @file window_utils.h
 * @brief Window procedure utility functions and helpers
 */

#ifndef WINDOW_UTILS_H
#define WINDOW_UTILS_H

#include <windows.h>
#include "utils/path_utils.h"

/* ============================================================================
 * String Conversion System
 * ============================================================================ */

/** UTF-8 → UTF-16 wrapper with validity flag */
typedef struct {
    wchar_t buf[MAX_PATH];
    BOOL valid;
} WideString;

/** UTF-16 → UTF-8 wrapper with validity flag */
typedef struct {
    char buf[MAX_PATH];
    BOOL valid;
} Utf8String;

/**
 * @brief Convert UTF-8 to UTF-16
 * @param utf8 Source string
 * @return Wrapped result - check `.valid` before use
 */
WideString ToWide(const char* utf8);

/**
 * @brief Convert UTF-16 to UTF-8
 * @param wide Source string
 * @return Wrapped result - check `.valid` before use
 */
Utf8String ToUtf8(const wchar_t* wide);

/* ============================================================================
 * Configuration Key Constants
 * ============================================================================ */

#define CFG_KEY_TEXT_COLOR           "CLOCK_TEXT_COLOR"
#define CFG_KEY_BASE_FONT_SIZE       "CLOCK_BASE_FONT_SIZE"
#define CFG_KEY_WINDOW_POS_X         "CLOCK_WINDOW_POS_X"
#define CFG_KEY_WINDOW_POS_Y         "CLOCK_WINDOW_POS_Y"
#define CFG_KEY_WINDOW_SCALE         "WINDOW_SCALE"
#define CFG_KEY_WINDOW_TOPMOST       "WINDOW_TOPMOST"
#define CFG_KEY_USE_24HOUR           "CLOCK_USE_24HOUR"
#define CFG_KEY_SHOW_SECONDS         "CLOCK_SHOW_SECONDS"
#define CFG_KEY_SHOW_MILLISECONDS    "CLOCK_SHOW_MILLISECONDS"
#define CFG_KEY_TIME_FORMAT          "CLOCK_TIME_FORMAT"
#define CFG_KEY_TIMEOUT_ACTION       "CLOCK_TIMEOUT_ACTION"
#define CFG_KEY_TIMEOUT_FILE         "CLOCK_TIMEOUT_FILE"
#define CFG_KEY_TIMEOUT_TEXT         "CLOCK_TIMEOUT_TEXT"
#define CFG_KEY_TIMEOUT_WEBSITE      "CLOCK_TIMEOUT_WEBSITE"
#define CFG_KEY_DEFAULT_START_TIME   "CLOCK_DEFAULT_START_TIME"
#define CFG_KEY_TIME_OPTIONS         "CLOCK_TIME_OPTIONS"
#define CFG_KEY_STARTUP_MODE         "STARTUP_MODE"
#define CFG_KEY_POMODORO_OPTIONS     "POMODORO_TIME_OPTIONS"
#define CFG_KEY_POMODORO_LOOP_COUNT  "POMODORO_LOOP_COUNT"
#define CFG_KEY_HOTKEY_CUSTOM_CD     "HOTKEY_CUSTOM_COUNTDOWN"

#define CFG_SECTION_DISPLAY     INI_SECTION_DISPLAY
#define CFG_SECTION_TIMER       INI_SECTION_TIMER
#define CFG_SECTION_POMODORO    INI_SECTION_POMODORO
#define CFG_SECTION_COLORS      INI_SECTION_COLORS

/* ============================================================================
 * String Constants
 * ============================================================================ */

extern const char* const STR_TRUE;
extern const char* const STR_FALSE;
extern const char* const STR_NONE;
extern const char* const STR_DEFAULT;
extern const char* const STR_MESSAGE;
extern const char* const STR_OK;

/* ============================================================================
 * Path Operations (from utils/path_utils.h)
 * ============================================================================ */

/* PathJoinW and GetRelativePathW are declared in utils/path_utils.h */

/* ============================================================================
 * Error Handling
 * ============================================================================ */

typedef enum {
    ERR_NONE = 0,
    ERR_FILE_NOT_FOUND,
    ERR_INVALID_INPUT,
    ERR_BUFFER_TOO_SMALL,
    ERR_OPERATION_FAILED
} ErrorCode;

/**
 * @brief Show localized error dialog
 */
void ShowError(HWND hwnd, ErrorCode errorCode);

/* ============================================================================
 * Configuration Access
 * ============================================================================ */

/**
 * @brief Get cached config path
 */
const char* GetCachedConfigPath(void);

/**
 * @brief Read config string
 */
void ReadConfigStr(const char* section, const char* key, 
                   const char* defaultVal, char* out, size_t size);

/**
 * @brief Read config integer
 */
int ReadConfigInt(const char* section, const char* key, int defaultVal);

/**
 * @brief Read config boolean
 */
BOOL ReadConfigBool(const char* section, const char* key, BOOL defaultVal);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if wide-char string is NULL, empty or whitespace-only
 */
BOOL isAllSpacesOnly(const wchar_t* str);

/**
 * @brief Clear input buffer
 */
void ClearInputBuffer(wchar_t* buffer, size_t size);

/**
 * @brief Update config with refresh
 */
void UpdateConfigWithRefresh(HWND hwnd, const char* key, const char* value);

/**
 * @brief Toggle boolean config value
 */
void ToggleConfigBool(HWND hwnd, const char* key, BOOL* currentValue, BOOL needsRedraw);

/**
 * @brief Write config and redraw
 */
void WriteConfigAndRedraw(HWND hwnd, const char* key, const char* value);

#endif /* WINDOW_UTILS_H */

