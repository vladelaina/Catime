#include "main/main_initialization.h"
#include "async_update_checker.h"
#include "audio_player.h"
#include "config.h"
#include "config/config_plugin_security.h"
#include "config/config_watcher.h"
#include "dialog/dialog_font_picker.h"
#include "dialog/dialog_notification_audio.h"
#include "drawing/drawing_effect.h"
#include "drawing/drawing_image.h"
#include "drawing/drawing_render.h"
#include "font.h"
#include "language.h"
#include "log.h"
#include "markdown/markdown_interactive.h"
#include "notification.h"
#include "plugin/plugin_data.h"
#include "plugin/plugin_manager.h"
#include "tray/tray_animation_menu.h"
#include "tray/tray_menu_font.h"
#include "update_checker.h"
#include "window/window_visual_effects.h"

void CleanupResources(void) {
    CleanupMarkdownInteractive();
    CleanupDrawingRenderCache();
    CleanupDrawingEffects();
    StopNotificationSound();
    CleanupNotificationResources();
    CleanupUpdateThreadBlocking();
    CleanupUpdateCheckResources();
    AnimationMenu_Shutdown();
    FontMenu_Shutdown();
    CleanupSystemFontDialogResources();
    if (!UnloadCurrentFontResource()) {
        LOG_WARNING("Failed to unload font resources during final cleanup");
    }
    NotificationSoundCache_Shutdown();
    PluginManager_Shutdown();
    PluginData_Shutdown();
    CleanupPluginTrustCS();
    ShutdownDrawingImage();
    ShutdownWindowVisualEffects();
    if (ConfigWatcher_Shutdown()) {
        ShutdownIniCache();
    } else {
        LOG_WARNING("Config watcher did not stop; INI cache retained");
    }
    CleanupLanguage();
    CoUninitialize();
    CleanupLogSystem();
}
