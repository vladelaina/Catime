#include "window_core_internal.h"

BOOL SaveWindowSettings(HWND hwnd) {
    if (!hwnd) return FALSE;
    if ((IsSystemPositionChangeGuardActive() ||
         g_pendingSystemPositionRestore) && !CLOCK_EDIT_MODE) return FALSE;
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        LOG_WARNING("Failed to get window rect for saving");
        return FALSE;
    }
    CLOCK_WINDOW_POS_X = rect.left;
    CLOCK_WINDOW_POS_Y = rect.top;
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    char posX[16], posY[16], scale[64], pluginScale[64];
    char monitorId[256] = {0};
    char offsetX[16], offsetY[16], axisRatio[16], crossOffset[16];
    int monitorOffsetX = 0;
    int monitorOffsetY = 0;
    int taskbarAxisRatio = 0;
    int taskbarCrossOffset = 0;
    BOOL taskbarAnchored = FALSE;
    BOOL taskbarAvailable = FALSE;
    snprintf(posX, sizeof(posX), "%d", CLOCK_WINDOW_POS_X);
    snprintf(posY, sizeof(posY), "%d", CLOCK_WINDOW_POS_Y);
    snprintf(scale, sizeof(scale), "%.9g", CLOCK_WINDOW_SCALE);
    snprintf(pluginScale, sizeof(pluginScale), "%.9g",
             PLUGIN_FONT_SCALE_FACTOR);
    BOOL placementAvailable = WindowCore_GetMonitorPlacementData(
        &rect, monitorId, sizeof(monitorId),
        &monitorOffsetX, &monitorOffsetY,
        &taskbarAvailable, &taskbarAnchored,
        &taskbarAxisRatio, &taskbarCrossOffset);
    snprintf(offsetX, sizeof(offsetX), "%d", monitorOffsetX);
    snprintf(offsetY, sizeof(offsetY), "%d", monitorOffsetY);
    snprintf(axisRatio, sizeof(axisRatio), "%d", taskbarAxisRatio);
    snprintf(crossOffset, sizeof(crossOffset), "%d", taskbarCrossOffset);

    if ((!CLOCK_WINDOW_POSITION_MANUAL ||
         g_positionTemporarilyRelocatedForDisplay) && !CLOCK_EDIT_MODE) {
        const IniKeyValue updates[] = {
            {INI_SECTION_DISPLAY, "WINDOW_SCALE", scale},
            {INI_SECTION_DISPLAY, "PLUGIN_SCALE", pluginScale}
        };
        BOOL saved = WriteIniMultipleAtomic(
            configPath, updates, sizeof(updates) / sizeof(updates[0]));
        if (!saved) {
            LOG_WARNING(
                "Failed to save window scale while preserving unavailable-monitor position");
        }
        return saved;
    }

    IniKeyValue updates[11];
    size_t count = 0;
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
    updates[count++] = (IniKeyValue){
        INI_SECTION_DISPLAY, "WINDOW_SCALE", scale};
    updates[count++] = (IniKeyValue){
        INI_SECTION_DISPLAY, "PLUGIN_SCALE", pluginScale};
    if (!WriteIniMultipleAtomic(configPath, updates, count)) {
        LOG_WARNING("Failed to save window settings");
        return FALSE;
    }
    CLOCK_WINDOW_POSITION_MANUAL = TRUE;
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
