/*
 * Bitrate Scene Switch - OBS Plugin
 * Automatic scene switching based on stream server bitrate
 * Automatic bitrate-based scene switching for OBS Studio
 */

#include <obs-module.h>
#include <obs-frontend-api.h>

#include "switcher.hpp"
#include "config.hpp"
#include "settings-dialog.hpp"
#include "websocket-vendor.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("BitrateSceneSwitch", "en-US")

static BitrateSwitch::Switcher *g_switcher = nullptr;
static BitrateSwitch::Config *g_config = nullptr;

const char *obs_module_name(void)
{
    return "Bitrate Scene Switch";
}

const char *obs_module_description(void)
{
    return "Automatic scene switching based on stream server bitrate";
}

static void frontend_event_callback(enum obs_frontend_event event, void *data)
{
    UNUSED_PARAMETER(data);

    if (!g_switcher)
        return;

    switch (event) {
    case OBS_FRONTEND_EVENT_STREAMING_STARTED:
        g_switcher->onStreamingStarted();
        break;
    case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
        g_switcher->onStreamingStopped();
        break;
    case OBS_FRONTEND_EVENT_RECORDING_STARTED:
        g_switcher->onRecordingStarted();
        break;
    case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
        g_switcher->onRecordingStopped();
        break;
    case OBS_FRONTEND_EVENT_SCENE_CHANGED:
        g_switcher->onSceneChanged();
        break;
    case OBS_FRONTEND_EVENT_EXIT:
        g_switcher->stop();
        break;
    default:
        break;
    }
}

static void save_callback(obs_data_t *save_data, bool saving, void *data)
{
    UNUSED_PARAMETER(data);

    if (saving) {
        if (g_config) {
            obs_data_t *config_data = g_config->save();
            obs_data_set_obj(save_data, "bitrate-scene-switch", config_data);
            obs_data_release(config_data);
        }
    } else {
        obs_data_t *config_data = obs_data_get_obj(save_data, "bitrate-scene-switch");
        if (config_data) {
            if (g_config) {
                g_config->load(config_data);
                if (g_switcher) {
                    g_switcher->reloadServers();
                }
            }
            obs_data_release(config_data);
        }
    }
}

static void menu_callback(void *data)
{
    UNUSED_PARAMETER(data);
    
    QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
    BitrateSwitch::SettingsDialog dialog(g_config, g_switcher, parent);
    dialog.exec();
}

bool obs_module_load(void)
{
    blog(LOG_INFO, "[BitrateSceneSwitch] Plugin loaded (version %s)", PLUGIN_VERSION);

    g_config = new BitrateSwitch::Config();
    g_switcher = new BitrateSwitch::Switcher(g_config);

    obs_frontend_add_event_callback(frontend_event_callback, nullptr);
    obs_frontend_add_save_callback(save_callback, nullptr);

    // Add Tools menu item
    obs_frontend_add_tools_menu_item("Bitrate Scene Switch", menu_callback, nullptr);

    // Start the switcher
    g_switcher->start();

    return true;
}

void obs_module_post_load(void)
{
    // Register WebSocket vendor requests (obs-websocket must be loaded first)
    auto &wsVendor = BitrateSwitch::WebSocketVendor::instance();
    wsVendor.setConfig(g_config);
    wsVendor.setSwitcher(g_switcher);
    if (wsVendor.registerVendor()) {
        blog(LOG_INFO, "[BitrateSceneSwitch] WebSocket vendor requests registered");
    }
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[BitrateSceneSwitch] Plugin unloading");

    BitrateSwitch::WebSocketVendor::instance().unregisterVendor();

    obs_frontend_remove_event_callback(frontend_event_callback, nullptr);
    obs_frontend_remove_save_callback(save_callback, nullptr);

    if (g_switcher) {
        g_switcher->stop();
        delete g_switcher;
        g_switcher = nullptr;
    }

    if (g_config) {
        delete g_config;
        g_config = nullptr;
    }
}
