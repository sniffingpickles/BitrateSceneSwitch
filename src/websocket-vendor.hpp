#pragma once

#include <obs-module.h>
#include <obs-data.h>

namespace BitrateSwitch {

class Switcher;
class Config;

class WebSocketVendor {
public:
    WebSocketVendor();
    ~WebSocketVendor();

    // Must be called from obs_module_post_load()
    bool registerVendor();
    void unregisterVendor();

    void setSwitcher(Switcher *switcher) { switcher_ = switcher; }
    void setConfig(Config *config) { config_ = config; }

    static WebSocketVendor& instance();

private:
    // Vendor request callbacks
    static void onGetSettings(obs_data_t *requestData, obs_data_t *responseData, void *priv_data);
    static void onSetSettings(obs_data_t *requestData, obs_data_t *responseData, void *priv_data);
    static void onGetStatus(obs_data_t *requestData, obs_data_t *responseData, void *priv_data);
    static void onSwitchScene(obs_data_t *requestData, obs_data_t *responseData, void *priv_data);
    static void onStartStream(obs_data_t *requestData, obs_data_t *responseData, void *priv_data);
    static void onStopStream(obs_data_t *requestData, obs_data_t *responseData, void *priv_data);
    static void onGetVersion(obs_data_t *requestData, obs_data_t *responseData, void *priv_data);

    void *vendor_ = nullptr;
    Switcher *switcher_ = nullptr;
    Config *config_ = nullptr;
    bool registered_ = false;

    static constexpr const char* VENDOR_NAME = "BitrateSceneSwitch";
};

} // namespace BitrateSwitch
