#include "config.hpp"
#include <obs-module.h>

namespace BitrateSwitch {

Config::Config()
{
    setDefaults();
}

Config::~Config() = default;

void Config::setDefaults()
{
    enabled = true;
    onlyWhenStreaming = false;
    instantRecover = true;
    autoNotify = true;
    retryAttempts = 5;

    triggers.low = 800;
    triggers.rtt = 2500;
    triggers.offline = 0;
    triggers.rttOffline = 0;

    scenes.normal = "Live";
    scenes.low = "Low";
    scenes.offline = "Offline";

    servers.clear();
    offlineTimeoutMinutes = 0;
}

obs_data_t *Config::save()
{
    obs_data_t *data = obs_data_create();

    obs_data_set_bool(data, "enabled", enabled);
    obs_data_set_bool(data, "only_when_streaming", onlyWhenStreaming);
    obs_data_set_bool(data, "instant_recover", instantRecover);
    obs_data_set_bool(data, "auto_notify", autoNotify);
    obs_data_set_int(data, "retry_attempts", retryAttempts);

    obs_data_set_int(data, "trigger_low", triggers.low);
    obs_data_set_int(data, "trigger_rtt", triggers.rtt);
    obs_data_set_int(data, "trigger_offline", triggers.offline);
    obs_data_set_int(data, "trigger_rtt_offline", triggers.rttOffline);

    obs_data_set_string(data, "scene_normal", scenes.normal.c_str());
    obs_data_set_string(data, "scene_low", scenes.low.c_str());
    obs_data_set_string(data, "scene_offline", scenes.offline.c_str());

    obs_data_array_t *serversArray = obs_data_array_create();
    for (const auto &server : servers) {
        obs_data_t *serverData = obs_data_create();
        obs_data_set_int(serverData, "type", static_cast<int>(server.type));
        obs_data_set_string(serverData, "name", server.name.c_str());
        obs_data_set_string(serverData, "stats_url", server.statsUrl.c_str());
        obs_data_set_string(serverData, "publisher", server.publisher.c_str());
        obs_data_set_int(serverData, "priority", server.priority);
        obs_data_set_bool(serverData, "enabled", server.enabled);
        obs_data_array_push_back(serversArray, serverData);
        obs_data_release(serverData);
    }
    obs_data_set_array(data, "servers", serversArray);
    obs_data_array_release(serversArray);

    obs_data_set_int(data, "offline_timeout", offlineTimeoutMinutes);

    return data;
}

void Config::load(obs_data_t *data)
{
    if (!data)
        return;

    enabled = obs_data_get_bool(data, "enabled");
    onlyWhenStreaming = obs_data_get_bool(data, "only_when_streaming");
    instantRecover = obs_data_get_bool(data, "instant_recover");
    autoNotify = obs_data_get_bool(data, "auto_notify");
    retryAttempts = static_cast<uint8_t>(obs_data_get_int(data, "retry_attempts"));
    if (retryAttempts == 0) retryAttempts = 5;

    triggers.low = static_cast<uint32_t>(obs_data_get_int(data, "trigger_low"));
    triggers.rtt = static_cast<uint32_t>(obs_data_get_int(data, "trigger_rtt"));
    triggers.offline = static_cast<uint32_t>(obs_data_get_int(data, "trigger_offline"));
    triggers.rttOffline = static_cast<uint32_t>(obs_data_get_int(data, "trigger_rtt_offline"));

    const char *normal = obs_data_get_string(data, "scene_normal");
    const char *low = obs_data_get_string(data, "scene_low");
    const char *offline = obs_data_get_string(data, "scene_offline");
    if (normal && *normal) scenes.normal = normal;
    if (low && *low) scenes.low = low;
    if (offline && *offline) scenes.offline = offline;

    servers.clear();
    obs_data_array_t *serversArray = obs_data_get_array(data, "servers");
    if (serversArray) {
        size_t count = obs_data_array_count(serversArray);
        for (size_t i = 0; i < count; i++) {
            obs_data_t *serverData = obs_data_array_item(serversArray, i);
            StreamServerConfig server;
            server.type = static_cast<ServerType>(obs_data_get_int(serverData, "type"));
            server.name = obs_data_get_string(serverData, "name");
            server.statsUrl = obs_data_get_string(serverData, "stats_url");
            server.publisher = obs_data_get_string(serverData, "publisher");
            server.priority = static_cast<int>(obs_data_get_int(serverData, "priority"));
            server.enabled = obs_data_get_bool(serverData, "enabled");
            servers.push_back(server);
            obs_data_release(serverData);
        }
        obs_data_array_release(serversArray);
    }

    offlineTimeoutMinutes = static_cast<uint32_t>(obs_data_get_int(data, "offline_timeout"));
}

} // namespace BitrateSwitch
