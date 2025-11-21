/**
 * @file config_recovery.h
 * @brief Configuration validation and automatic recovery
 */

#ifndef CONFIG_RECOVERY_H
#define CONFIG_RECOVERY_H

#include <windows.h>
#include "config/config_loader.h"

BOOL ValidateAndRecoverConfig(ConfigSnapshot* snapshot);

BOOL ValidateFontConfig(ConfigSnapshot* snapshot);
BOOL ValidateColorConfig(ConfigSnapshot* snapshot);
BOOL ValidateTimerConfig(ConfigSnapshot* snapshot);
BOOL ValidateWindowConfig(ConfigSnapshot* snapshot);
BOOL ValidateNotificationConfig(ConfigSnapshot* snapshot);
BOOL ValidatePomodoroConfig(ConfigSnapshot* snapshot);
BOOL ValidateTimeoutAction(ConfigSnapshot* snapshot);

#endif /* CONFIG_RECOVERY_H */
