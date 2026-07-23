/**
 * @file config_timer_api.h
 * @brief Timer/display conversion and tray animation color configuration.
 */

#ifndef CATIME_CONFIG_TIMER_API_H
#define CATIME_CONFIG_TIMER_API_H

#include <windows.h>
#include "config/config_types.h"
#include "timer/timer.h"

TimeFormatType TimeFormatType_FromStr(const char* value);
const char* TimeFormatType_ToStr(TimeFormatType value);
TimeoutActionType TimeoutActionType_FromStr(const char* value);
const char* TimeoutActionType_ToStr(TimeoutActionType value);

BOOL WriteConfigTimeFormat(TimeFormatType format);
BOOL WriteConfigShowMilliseconds(BOOL showMilliseconds);
UINT GetTimerInterval(void);
void ResetTimerWithInterval(HWND window);
BOOL WriteConfigStartupMode(const char* mode);
void WriteConfigWindowOpacity(int opacity);
void WriteConfigMoveSteps(int smallStep, int largeStep);
int ReadConfigOpacityStepNormal(void);
int ReadConfigOpacityStepFast(void);
void WriteConfigOpacitySteps(int normalStep, int fastStep);
int ReadConfigScaleStepNormal(void);
int ReadConfigScaleStepFast(void);
void WriteConfigScaleSteps(int normalStep, int fastStep);

void ReadPercentIconColorsConfig(void);
COLORREF GetPercentIconTextColor(void);
COLORREF GetPercentIconBgColor(void);

#endif /* CATIME_CONFIG_TIMER_API_H */
