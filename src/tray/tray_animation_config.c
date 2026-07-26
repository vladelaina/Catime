/**
 * @file tray_animation_config.c
 * @brief Animation config-path normalization and window validation.
 */

#include "tray_animation_core_internal.h"

BOOL IsValidTrayAnimationWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

HWND GetValidTrayAnimationWindow(void) {
    HWND hwnd = g_trayHwnd;
    return IsValidTrayAnimationWindow(hwnd) && IsTrayIconActive(hwnd) ? hwnd : NULL;
}

BOOL BuildAnimationConfigPath(const char* name, char* animPath, size_t animPathSize) {
    if (!name || !animPath || animPathSize == 0) return FALSE;

    int pathLen = 0;
    if (IsBuiltinAnimationName(name)) {
        pathLen = snprintf(animPath, animPathSize, "%s", name);
    } else {
        if (!IsSafeAnimationRelativePath(name)) return FALSE;

        pathLen = snprintf(animPath, animPathSize,
                           "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", name);
    }

    if (pathLen < 0 || (size_t)pathLen >= animPathSize) {
        animPath[0] = '\0';
        LOG_WARNING("Animation config path too long: %s", name);
        return FALSE;
    }

    return TRUE;
}

BOOL WriteAnimationConfigPathIfChanged(const char* configPath, const char* animPath) {
    if (!configPath || !animPath) return FALSE;

    char currentPath[MAX_PATH] = {0};
    BOOL hasCompleteCurrentPath = ReadIniStringExact(
        "Animation", "ANIMATION_PATH", "", currentPath, sizeof(currentPath), configPath);
    if (hasCompleteCurrentPath && strcmp(currentPath, animPath) == 0) {
        return TRUE;
    }
    if (!hasCompleteCurrentPath) {
        LOG_WARNING("Replacing ANIMATION_PATH because the existing config value is too long");
    }

    return WriteIniString("Animation", "ANIMATION_PATH", animPath, configPath);
}

BOOL WriteAnimationNameToConfigIfChanged(const char* name) {
    char configPath[MAX_PATH] = {0};
    char animPath[MAX_PATH] = {0};

    GetConfigPath(configPath, sizeof(configPath));
    if (!BuildAnimationConfigPath(name, animPath, sizeof(animPath))) {
        return FALSE;
    }

    return WriteAnimationConfigPathIfChanged(configPath, animPath);
}

void ReadAnimationNameFromConfig(char* name, size_t nameSize, const char* configPath) {
    if (!name || nameSize == 0) return;

    char nameBuf[MAX_PATH] = {0};
    if (!ReadIniStringExact("Animation", "ANIMATION_PATH", "__logo__",
                            nameBuf, sizeof(nameBuf), configPath)) {
        LOG_WARNING("Ignoring ANIMATION_PATH because the config value is too long");
        return;
    }
    NormalizeAnimConfigValue(nameBuf);

    if (nameBuf[0] == '\0') {
        return;
    }

    const char* prefix = ANIMATIONS_PATH_PREFIX;
    if (IsBuiltinAnimationName(nameBuf)) {
        CopyStringExactA(nameBuf, name, nameSize);
    } else if (_strnicmp(nameBuf, prefix, (int)strlen(prefix)) == 0) {
        const char* rel = nameBuf + strlen(prefix);
        if (IsSafeAnimationRelativePath(rel)) {
            CopyStringExactA(rel, name, nameSize);
        }
    } else if (IsSafeAnimationRelativePath(nameBuf)) {
        CopyStringExactA(nameBuf, name, nameSize);
    }
}
