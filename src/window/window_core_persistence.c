#include "window_core_internal.h"

static UINT g_windowSettingsDirtyFlags = 0;

void MarkWindowSettingsDirty(UINT flags) {
    g_windowSettingsDirtyFlags |= flags;
}

void ClearWindowSettingsDirty(UINT flags) {
    g_windowSettingsDirtyFlags &= ~flags;
}

BOOL SaveWindowSettings(HWND hwnd) {
    if (!hwnd) return FALSE;
    UINT dirtyFlags = g_windowSettingsDirtyFlags;
    if (!dirtyFlags) return TRUE;
    if ((IsSystemPositionChangeGuardActive() ||
         g_pendingSystemPositionRestore) && !CLOCK_EDIT_MODE) return FALSE;

    BOOL savePosition =
        (dirtyFlags & WINDOW_SETTINGS_DIRTY_POSITION) != 0;
    if ((!CLOCK_WINDOW_POSITION_MANUAL ||
         g_positionTemporarilyRelocatedForDisplay) && !CLOCK_EDIT_MODE) {
        savePosition = FALSE;
    }

    RECT rect = {0};
    char monitorId[256] = {0};
    int monitorOffsetX = 0;
    int monitorOffsetY = 0;
    BOOL taskbarAvailable = FALSE;
    BOOL taskbarAnchored = FALSE;
    int taskbarAxisRatio = 0;
    int taskbarCrossOffset = 0;
    BOOL placementAvailable = FALSE;
    if (savePosition) {
        if (!GetWindowRect(hwnd, &rect)) {
            LOG_WARNING("Failed to get window rect for saving");
            return FALSE;
        }
        CLOCK_WINDOW_POS_X = rect.left;
        CLOCK_WINDOW_POS_Y = rect.top;
        placementAvailable = WindowCore_GetMonitorPlacementData(
            &rect, monitorId, sizeof(monitorId),
            &monitorOffsetX, &monitorOffsetY,
            &taskbarAvailable, &taskbarAnchored,
            &taskbarAxisRatio, &taskbarCrossOffset);
    }

    char posX[16], posY[16], scale[64], pluginScale[64];
    char offsetX[16], offsetY[16], axisRatio[16], crossOffset[16];
    snprintf(posX, sizeof(posX), "%d", CLOCK_WINDOW_POS_X);
    snprintf(posY, sizeof(posY), "%d", CLOCK_WINDOW_POS_Y);
    snprintf(scale, sizeof(scale), "%.9g", CLOCK_WINDOW_SCALE);
    snprintf(pluginScale, sizeof(pluginScale), "%.9g",
             PLUGIN_FONT_SCALE_FACTOR);
    snprintf(offsetX, sizeof(offsetX), "%d", monitorOffsetX);
    snprintf(offsetY, sizeof(offsetY), "%d", monitorOffsetY);
    snprintf(axisRatio, sizeof(axisRatio), "%d", taskbarAxisRatio);
    snprintf(crossOffset, sizeof(crossOffset), "%d", taskbarCrossOffset);

    IniKeyValue updates[11];
    size_t count = 0;
    if (savePosition) {
        updates[count++] = (IniKeyValue){
            INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", posX};
        updates[count++] = (IniKeyValue){
            INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", posY};
        updates[count++] = (IniKeyValue){
            INI_SECTION_DISPLAY, WINDOW_POSITION_MANUAL_KEY, "TRUE"};
        if (placementAvailable) {
            updates[count++] = (IniKeyValue){
                INI_SECTION_DISPLAY, WINDOW_MONITOR_ID_KEY, monitorId};
            updates[count++] = (IniKeyValue){
                INI_SECTION_DISPLAY, WINDOW_MONITOR_OFFSET_X_KEY, offsetX};
            updates[count++] = (IniKeyValue){
                INI_SECTION_DISPLAY, WINDOW_MONITOR_OFFSET_Y_KEY, offsetY};
        }
        if (taskbarAvailable) {
            updates[count++] = (IniKeyValue){
                INI_SECTION_DISPLAY, WINDOW_TASKBAR_ANCHORED_KEY,
                taskbarAnchored ? "TRUE" : "FALSE"};
            updates[count++] = (IniKeyValue){
                INI_SECTION_DISPLAY, WINDOW_TASKBAR_AXIS_RATIO_KEY, axisRatio};
            updates[count++] = (IniKeyValue){
                INI_SECTION_DISPLAY, WINDOW_TASKBAR_CROSS_OFFSET_KEY,
                crossOffset};
        }
    }
    if (dirtyFlags & WINDOW_SETTINGS_DIRTY_SCALE) {
        updates[count++] = (IniKeyValue){
            INI_SECTION_DISPLAY, "WINDOW_SCALE", scale};
    }
    if (dirtyFlags & WINDOW_SETTINGS_DIRTY_PLUGIN_SCALE) {
        updates[count++] = (IniKeyValue){
            INI_SECTION_DISPLAY, "PLUGIN_SCALE", pluginScale};
    }

    BOOL saved = TRUE;
    if (count > 0) {
        char configPath[MAX_PATH];
        GetConfigPath(configPath, MAX_PATH);
        saved = WriteIniMultipleAtomic(configPath, updates, count);
    }
    if (!saved) {
        LOG_WARNING("Failed to save changed window settings");
        return FALSE;
    }
    g_windowSettingsDirtyFlags &= ~dirtyFlags;
    if (savePosition) CLOCK_WINDOW_POSITION_MANUAL = TRUE;
    return TRUE;
}

BOOL OpenFileDialog(HWND hwnd, wchar_t* filePath, DWORD maxPath) {
    OPENFILENAMEW dialog = {0};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd;
    dialog.lpstrFilter = L"All Files\0*.*\0";
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = maxPath;
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    dialog.lpstrDefExt = L"";
    BOOL result = GetOpenFileNameW(&dialog);
    if (result) {
        LOG_INFO("File selected: %S", filePath);
    } else {
        LOG_INFO("File dialog cancelled");
    }
    return result;
}
