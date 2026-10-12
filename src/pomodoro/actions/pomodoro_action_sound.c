#include "pomodoro_phase_action.h"
#include "audio_player.h"
#include "config.h"
#include "pomodoro_phase_hook_config.h"
#include <string.h>

static BOOL ExecuteSound(HWND hwnd, POMODORO_PHASE phase) {
    PomodoroPhaseHookConfig config;
    if (!LoadPomodoroPhaseHookConfig(phase, &config)) return FALSE;
    const char* soundFile = config.soundFile;
    if (_stricmp(soundFile, POMODORO_HOOK_DEFAULT) == 0) {
        soundFile = g_AppConfig.notification.sound.sound_file;
        if (soundFile[0] == '\0') soundFile = POMODORO_HOOK_DEFAULT_SOUND;
    }
    return PlayNotificationSoundFile(hwnd, soundFile);
}

const PomodoroPhaseActionDefinition POMODORO_PHASE_ACTION_SOUND = {
    "SOUND",
    L"Sound",
    ExecuteSound
};
