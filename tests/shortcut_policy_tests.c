#include "shortcut_policy.h"

#include <stdio.h>

static int failures = 0;

static void Expect(const char* name, bool actual, bool expected) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n",
                name, expected, actual);
        failures++;
    }
}

int main(void) {
    const char* packageName = "vladelaina.Catime";
    const char* executableName = "catime.exe";

    Expect("recognizes standard Store path",
           ShortcutPolicy_IsLegacyPackagedTarget(
               "C:\\Program Files\\WindowsApps\\"
               "vladelaina.Catime_1.5.0.0_x86__hnew8t3b8e0t6\\catime.exe",
               packageName, executableName), true);
    Expect("recognizes Store path on another drive",
           ShortcutPolicy_IsLegacyPackagedTarget(
               "D:\\WindowsApps\\"
               "vladelaina.Catime_2.0.0.0_x86__hnew8t3b8e0t6\\CATIME.EXE",
               packageName, executableName), true);
    Expect("supports normalized forward separators",
           ShortcutPolicy_IsLegacyPackagedTarget(
               "E:/WindowsApps/vladelaina.Catime_1.0.0.0_x86__pfn/catime.exe",
               packageName, executableName), true);
    Expect("rejects another Store package",
           ShortcutPolicy_IsLegacyPackagedTarget(
               "C:\\Program Files\\WindowsApps\\"
               "OtherCompany.Catime_1.0.0.0_x86__pfn\\catime.exe",
               packageName, executableName), false);
    Expect("rejects package prefix collision",
           ShortcutPolicy_IsLegacyPackagedTarget(
               "C:\\Program Files\\WindowsApps\\"
               "vladelaina.CatimePlus_1.0.0.0_x86__pfn\\catime.exe",
               packageName, executableName), false);
    Expect("rejects wrong executable",
           ShortcutPolicy_IsLegacyPackagedTarget(
               "C:\\Program Files\\WindowsApps\\"
               "vladelaina.Catime_1.0.0.0_x86__pfn\\helper.exe",
               packageName, executableName), false);
    Expect("rejects non-WindowsApps path",
           ShortcutPolicy_IsLegacyPackagedTarget(
               "C:\\Temp\\vladelaina.Catime_1.0.0.0_x86__pfn\\catime.exe",
               packageName, executableName), false);
    Expect("rejects unpackaged install",
           ShortcutPolicy_IsLegacyPackagedTarget(
               "C:\\ProgramData\\chocolatey\\lib\\catime\\catime.exe",
               packageName, executableName), false);
    Expect("rejects missing package version separator",
           ShortcutPolicy_IsLegacyPackagedTarget(
               "C:\\WindowsApps\\vladelaina.Catime\\catime.exe",
               packageName, executableName), false);
    Expect("rejects null input",
           ShortcutPolicy_IsLegacyPackagedTarget(
               NULL, packageName, executableName), false);

    if (failures == 0) puts("shortcut policy tests passed");
    return failures == 0 ? 0 : 1;
}
