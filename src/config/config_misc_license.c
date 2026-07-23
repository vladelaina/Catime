#include "config_misc_internal.h"

void SetFontLicenseAccepted(BOOL accepted) {
    accepted = accepted ? TRUE : FALSE;
    if (!WriteConfigKeyValue(
            "FONT_LICENSE_ACCEPTED", accepted ? "TRUE" : "FALSE")) {
        return;
    }
    g_AppConfig.font_license.accepted = accepted;
}

void SetFontLicenseVersionAccepted(const char* version) {
    if (!version || !WriteConfigKeyValue(
            "FONT_LICENSE_VERSION_ACCEPTED", version)) {
        return;
    }
    strncpy(g_AppConfig.font_license.version_accepted,
            version, sizeof(g_AppConfig.font_license.version_accepted) - 1);
    g_AppConfig.font_license.version_accepted[
        sizeof(g_AppConfig.font_license.version_accepted) - 1] = '\0';
}

BOOL NeedsFontLicenseVersionAcceptance(void) {
    return !g_AppConfig.font_license.accepted ||
           g_AppConfig.font_license.version_accepted[0] == '\0' ||
           strcmp(FONT_LICENSE_VERSION,
                  g_AppConfig.font_license.version_accepted) != 0;
}

const char* GetCurrentFontLicenseVersion(void) {
    return FONT_LICENSE_VERSION;
}

BOOL WriteConfigLanguage(int language) {
    const char* languageName = GetLanguageConfigKey(language);
    return languageName && WriteConfigKeyValue("LANGUAGE", languageName);
}
