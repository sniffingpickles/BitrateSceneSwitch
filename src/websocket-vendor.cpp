#include "websocket-vendor.hpp"
#include "switcher.hpp"
#include "config.hpp"
#include "update-checker.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>

// Include the obs-websocket API header
#include "obs-websocket-api.h"

namespace BitrateSwitch {

WebSocketVendor::WebSocketVendor()
{
}

WebSocketVendor::~WebSocketVendor()
{
    unregisterVendor();
}

WebSocketVendor& WebSocketVendor::instance()
{
    static WebSocketVendor inst;
    return inst;
}

bool WebSocketVendor::registerVendor()
{
    if (registered_) return true;

    vendor_ = obs_websocket_register_vendor(VENDOR_NAME);
    if (!vendor_) {
        blog(LOG_WARNING, "[BitrateSceneSwitch] Failed to register WebSocket vendor. obs-websocket may not be installed.");
        return false;
    }

    obs_websocket_vendor_register_request(vendor_, "GetSettings", onGetSettings, this);
    obs_websocket_vendor_register_request(vendor_, "SetSettings", onSetSettings, this);
    obs_websocket_vendor_register_request(vendor_, "GetStatus", onGetStatus, this);
    obs_websocket_vendor_register_request(vendor_, "SwitchScene", onSwitchScene, this);
    obs_websocket_vendor_register_request(vendor_, "StartStream", onStartStream, this);
    obs_websocket_vendor_register_request(vendor_, "StopStream", onStopStream, this);
    obs_websocket_vendor_register_request(vendor_, "GetVersion", onGetVersion, this);

    registered_ = true;
    blog(LOG_INFO, "[BitrateSceneSwitch] WebSocket vendor registered with %d requests", 7);
    return true;
}

void WebSocketVendor::unregisterVendor()
{
    if (!registered_ || !vendor_) return;

    obs_websocket_vendor_unregister_request(vendor_, "GetSettings");
    obs_websocket_vendor_unregister_request(vendor_, "SetSettings");
    obs_websocket_vendor_unregister_request(vendor_, "GetStatus");
    obs_websocket_vendor_unregister_request(vendor_, "SwitchScene");
    obs_websocket_vendor_unregister_request(vendor_, "StartStream");
    obs_websocket_vendor_unregister_request(vendor_, "StopStream");
    obs_websocket_vendor_unregister_request(vendor_, "GetVersion");

    registered_ = false;
    blog(LOG_INFO, "[BitrateSceneSwitch] WebSocket vendor unregistered");
}

// ==================== Request Handlers ====================

void WebSocketVendor::onGetSettings(obs_data_t *requestData, obs_data_t *responseData, void *priv_data)
{
    (void)requestData;
    auto *self = static_cast<WebSocketVendor*>(priv_data);
    if (!self->config_) return;

    Config *cfg = self->config_;

    // Core settings
    obs_data_set_bool(responseData, "enabled", cfg->enabled);
    obs_data_set_bool(responseData, "onlyWhenStreaming", cfg->onlyWhenStreaming);
    obs_data_set_bool(responseData, "instantRecover", cfg->instantRecover);
    obs_data_set_int(responseData, "retryAttempts", cfg->retryAttempts);

    // Triggers
    obs_data_set_int(responseData, "triggerLow", cfg->triggers.low);
    obs_data_set_int(responseData, "triggerRtt", cfg->triggers.rtt);
    obs_data_set_int(responseData, "triggerOffline", cfg->triggers.offline);
    obs_data_set_int(responseData, "triggerRttOffline", cfg->triggers.rttOffline);

    // Scenes
    obs_data_set_string(responseData, "sceneNormal", cfg->scenes.normal.c_str());
    obs_data_set_string(responseData, "sceneLow", cfg->scenes.low.c_str());
    obs_data_set_string(responseData, "sceneOffline", cfg->scenes.offline.c_str());

    // Optional scenes
    obs_data_set_string(responseData, "sceneStarting", cfg->optionalScenes.starting.c_str());
    obs_data_set_string(responseData, "sceneEnding", cfg->optionalScenes.ending.c_str());
    obs_data_set_string(responseData, "scenePrivacy", cfg->optionalScenes.privacy.c_str());
    obs_data_set_string(responseData, "sceneRefresh", cfg->optionalScenes.refresh.c_str());
}

void WebSocketVendor::onSetSettings(obs_data_t *requestData, obs_data_t *responseData, void *priv_data)
{
    auto *self = static_cast<WebSocketVendor*>(priv_data);
    if (!self->config_) return;

    Config *cfg = self->config_;

    // Only update fields that are present in the request
    if (obs_data_has_user_value(requestData, "enabled"))
        cfg->enabled = obs_data_get_bool(requestData, "enabled");
    if (obs_data_has_user_value(requestData, "onlyWhenStreaming"))
        cfg->onlyWhenStreaming = obs_data_get_bool(requestData, "onlyWhenStreaming");
    if (obs_data_has_user_value(requestData, "instantRecover"))
        cfg->instantRecover = obs_data_get_bool(requestData, "instantRecover");
    if (obs_data_has_user_value(requestData, "retryAttempts"))
        cfg->retryAttempts = (uint8_t)obs_data_get_int(requestData, "retryAttempts");

    // Triggers
    if (obs_data_has_user_value(requestData, "triggerLow"))
        cfg->triggers.low = (uint32_t)obs_data_get_int(requestData, "triggerLow");
    if (obs_data_has_user_value(requestData, "triggerRtt"))
        cfg->triggers.rtt = (uint32_t)obs_data_get_int(requestData, "triggerRtt");
    if (obs_data_has_user_value(requestData, "triggerOffline"))
        cfg->triggers.offline = (uint32_t)obs_data_get_int(requestData, "triggerOffline");
    if (obs_data_has_user_value(requestData, "triggerRttOffline"))
        cfg->triggers.rttOffline = (uint32_t)obs_data_get_int(requestData, "triggerRttOffline");

    // Scenes
    if (obs_data_has_user_value(requestData, "sceneNormal"))
        cfg->scenes.normal = obs_data_get_string(requestData, "sceneNormal");
    if (obs_data_has_user_value(requestData, "sceneLow"))
        cfg->scenes.low = obs_data_get_string(requestData, "sceneLow");
    if (obs_data_has_user_value(requestData, "sceneOffline"))
        cfg->scenes.offline = obs_data_get_string(requestData, "sceneOffline");

    // Optional scenes
    if (obs_data_has_user_value(requestData, "sceneStarting"))
        cfg->optionalScenes.starting = obs_data_get_string(requestData, "sceneStarting");
    if (obs_data_has_user_value(requestData, "sceneEnding"))
        cfg->optionalScenes.ending = obs_data_get_string(requestData, "sceneEnding");
    if (obs_data_has_user_value(requestData, "scenePrivacy"))
        cfg->optionalScenes.privacy = obs_data_get_string(requestData, "scenePrivacy");
    if (obs_data_has_user_value(requestData, "sceneRefresh"))
        cfg->optionalScenes.refresh = obs_data_get_string(requestData, "sceneRefresh");

    // Save and reload
    if (self->switcher_)
        self->switcher_->reloadServers();

    obs_data_set_bool(responseData, "success", true);
    blog(LOG_INFO, "[BitrateSceneSwitch] Settings updated via WebSocket");
}

void WebSocketVendor::onGetStatus(obs_data_t *requestData, obs_data_t *responseData, void *priv_data)
{
    (void)requestData;
    auto *self = static_cast<WebSocketVendor*>(priv_data);
    if (!self->switcher_) return;

    auto info = self->switcher_->getLastBitrateInfo();
    
    obs_data_set_string(responseData, "currentScene", self->switcher_->getCurrentScene().c_str());
    obs_data_set_bool(responseData, "isStreaming", self->switcher_->isCurrentlyStreaming());
    obs_data_set_int(responseData, "bitrateKbps", info.bitrateKbps);
    obs_data_set_double(responseData, "rttMs", info.rttMs);
    obs_data_set_bool(responseData, "isOnline", info.isOnline);
    obs_data_set_string(responseData, "serverName", info.serverName.c_str());
    obs_data_set_string(responseData, "statusMessage", info.message.c_str());
    obs_data_set_bool(responseData, "enabled", self->config_ ? self->config_->enabled : false);
}

void WebSocketVendor::onSwitchScene(obs_data_t *requestData, obs_data_t *responseData, void *priv_data)
{
    auto *self = static_cast<WebSocketVendor*>(priv_data);
    if (!self->switcher_) return;

    const char *sceneName = obs_data_get_string(requestData, "sceneName");
    if (!sceneName || !strlen(sceneName)) {
        obs_data_set_bool(responseData, "success", false);
        obs_data_set_string(responseData, "error", "sceneName is required");
        return;
    }

    bool result = self->switcher_->switchToSceneByName(sceneName);
    obs_data_set_bool(responseData, "success", result);
    if (!result) {
        obs_data_set_string(responseData, "error", "Scene not found");
    }
}

void WebSocketVendor::onStartStream(obs_data_t *requestData, obs_data_t *responseData, void *priv_data)
{
    (void)requestData;
    (void)priv_data;

    if (obs_frontend_streaming_active()) {
        obs_data_set_bool(responseData, "success", false);
        obs_data_set_string(responseData, "error", "Stream is already running");
        return;
    }

    obs_frontend_streaming_start();
    obs_data_set_bool(responseData, "success", true);
}

void WebSocketVendor::onStopStream(obs_data_t *requestData, obs_data_t *responseData, void *priv_data)
{
    (void)requestData;
    (void)priv_data;

    if (!obs_frontend_streaming_active()) {
        obs_data_set_bool(responseData, "success", false);
        obs_data_set_string(responseData, "error", "Stream is not running");
        return;
    }

    obs_frontend_streaming_stop();
    obs_data_set_bool(responseData, "success", true);
}

void WebSocketVendor::onGetVersion(obs_data_t *requestData, obs_data_t *responseData, void *priv_data)
{
    (void)requestData;
    (void)priv_data;

    obs_data_set_string(responseData, "version", UpdateChecker::getCurrentVersion().c_str());
    obs_data_set_string(responseData, "vendor", VENDOR_NAME);
}

} // namespace BitrateSwitch
