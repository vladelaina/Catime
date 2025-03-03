/**
 * @file config.h
 * @brief Configuration management module header file.
 *
 * This file defines the interfaces for application configuration,
 * including reading, writing, and managing settings related to window,
 * fonts, colors, and other customizable options.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>
#include <time.h>
#include "../resource/resource.h"
#include "language.h"
#include "font.h"
#include "color.h"
#include "tray.h"
#include "tray_menu.h"
#include "timer.h"
#include "window.h"
#include "startup.h"

typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
} RecentFile;

extern RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];
extern int CLOCK_RECENT_FILES_COUNT;
extern int CLOCK_DEFAULT_START_TIME;
extern time_t last_config_time;

/// @name Configuration-related Function Declarations
/// @{

/**
 * @brief Retrieves the configuration file path.
 * @param path Buffer to store the configuration file path.
 * @param size Size of the buffer.
 */
void GetConfigPath(char* path, size_t size);

/**
 * @brief Reads the configuration from the file.
 */
void ReadConfig();

/**
 * @brief Writes the timeout action to the configuration file.
 * @param action The timeout action to write.
 */
void WriteConfigTimeoutAction(const char* action);

/**
 * @brief Writes the edit mode to the configuration file.
 * @param mode The edit mode to write.
 */
void WriteConfigEditMode(const char* mode);

/**
 * @brief Writes the time options to the configuration file.
 * @param options The time options to write.
 */
void WriteConfigTimeOptions(const char* options);

/**
 * @brief Loads recent files from the configuration.
 */
void LoadRecentFiles(void);

/**
 * @brief Saves a recent file to the configuration.
 * @param filePath The path of the file to save.
 */
void SaveRecentFile(const char* filePath);

/**
 * @brief Converts a UTF-8 string to ANSI.
 * @param utf8Str The UTF-8 string to convert.
 * @return The converted ANSI string.
 */
char* UTF8ToANSI(const char* utf8Str);

/**
 * @brief Creates the default configuration file.
 * @param config_path The path to the configuration file.
 */
void CreateDefaultConfig(const char* config_path);

/// @}

#endif // CONFIG_H
