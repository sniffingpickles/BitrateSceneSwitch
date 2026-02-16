#include "config.hpp"
#include <obs-module.h>
#include <algorithm>

namespace BitrateSwitch {

const char* getServerTypeName(ServerType type)
{
    switch (type) {
    case ServerType::Belabox: return "BELABOX";
    case ServerType::Nginx: return "NGINX";
    case ServerType::SrtLiveServer: return "SRT Live Server";
    case ServerType::Mediamtx: return "MediaMTX";
    case ServerType::NodeMediaServer: return "Node Media Server";
    case ServerType::Nimble: return "Nimble";
    case ServerType::Rist: return "RIST";
    case ServerType::OpenIRL: return "OpenIRL";
    case ServerType::IrlHosting: return "IRLHosting";
    case ServerType::Xiu: return "Xiu";
    default: return "Unknown";
    }
}

ServerType getServerTypeFromName(const std::string& name)
{
    if (name == "BELABOX" || name == "Belabox") return ServerType::Belabox;
    if (name == "NGINX" || name == "Nginx") return ServerType::Nginx;
    if (name == "SRT Live Server" || name == "SrtLiveServer") return ServerType::SrtLiveServer;
    if (name == "MediaMTX" || name == "Mediamtx") return ServerType::Mediamtx;
    if (name == "Node Media Server" || name == "NodeMediaServer") return ServerType::NodeMediaServer;
    if (name == "Nimble") return ServerType::Nimble;
    if (name == "RIST" || name == "Rist") return ServerType::Rist;
    if (name == "OpenIRL") return ServerType::OpenIRL;
    if (name == "IRLHosting" || name == "IrlHosting") return ServerType::IrlHosting;
    if (name == "Xiu") return ServerType::Xiu;
    return ServerType::Belabox;
}

Config::Config()
{
    setDefaults();
}

Config::~Config() = default;

void Config::setDefaults()
{
    enabled = true;
    onlyWhenStreaming = true;
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

    optionalScenes = OptionalScenes();
    options = OptionalOptions();
    servers.clear();
}

void Config::sortServersByPriority()
{
    std::sort(servers.begin(), servers.end(),
        [](const StreamServerConfig& a, const StreamServerConfig& b) {
            return a.priority < b.priority;
        });
}

obs_data_t *Config::save()
{
    obs_data_t *data = obs_data_create();

    // Core settings
    obs_data_set_bool(data, "enabled", enabled);
    obs_data_set_bool(data, "only_when_streaming", onlyWhenStreaming);
    obs_data_set_bool(data, "instant_recover", instantRecover);
    obs_data_set_bool(data, "auto_notify", autoNotify);
    obs_data_set_int(data, "retry_attempts", retryAttempts);

    // Triggers
    obs_data_set_int(data, "trigger_low", triggers.low);
    obs_data_set_int(data, "trigger_rtt", triggers.rtt);
    obs_data_set_int(data, "trigger_offline", triggers.offline);
    obs_data_set_int(data, "trigger_rtt_offline", triggers.rttOffline);

    // Switching scenes
    obs_data_set_string(data, "scene_normal", scenes.normal.c_str());
    obs_data_set_string(data, "scene_low", scenes.low.c_str());
    obs_data_set_string(data, "scene_offline", scenes.offline.c_str());

    // Optional scenes
    obs_data_set_string(data, "scene_starting", optionalScenes.starting.c_str());
    obs_data_set_string(data, "scene_ending", optionalScenes.ending.c_str());
    obs_data_set_string(data, "scene_privacy", optionalScenes.privacy.c_str());
    obs_data_set_string(data, "scene_refresh", optionalScenes.refresh.c_str());

    // Optional options
    obs_data_set_int(data, "offline_timeout", options.offlineTimeoutMinutes);
    obs_data_set_bool(data, "record_while_streaming", options.recordWhileStreaming);
    obs_data_set_bool(data, "switch_to_starting", options.switchToStartingOnStreamStart);
    obs_data_set_bool(data, "switch_from_starting", options.switchFromStartingToLive);

    // Stream servers
    obs_data_array_t *serversArray = obs_data_array_create();
    for (const auto &server : servers) {
        obs_data_t *serverData = obs_data_create();
        obs_data_set_int(serverData, "type", static_cast<int>(server.type));
        obs_data_set_string(serverData, "name", server.name.c_str());
        obs_data_set_string(serverData, "stats_url", server.statsUrl.c_str());
        obs_data_set_string(serverData, "publisher", server.publisher.c_str());
        obs_data_set_string(serverData, "application", server.application.c_str());
        obs_data_set_string(serverData, "key", server.key.c_str());
        obs_data_set_string(serverData, "id", server.id.c_str());
        obs_data_set_string(serverData, "auth_user", server.authUser.c_str());
        obs_data_set_string(serverData, "auth_pass", server.authPass.c_str());
        obs_data_set_int(serverData, "priority", server.priority);
        obs_data_set_bool(serverData, "enabled", server.enabled);
        
        // Override scenes
        obs_data_set_bool(serverData, "override_enabled", server.overrideScenes.enabled);
        obs_data_set_string(serverData, "override_normal", server.overrideScenes.normal.c_str());
        obs_data_set_string(serverData, "override_low", server.overrideScenes.low.c_str());
        obs_data_set_string(serverData, "override_offline", server.overrideScenes.offline.c_str());
        
        // Depends on
        obs_data_set_bool(serverData, "depends_enabled", server.dependsOn.enabled);
        obs_data_set_string(serverData, "depends_server", server.dependsOn.serverName.c_str());
        
        obs_data_array_push_back(serversArray, serverData);
        obs_data_release(serverData);
    }
    obs_data_set_array(data, "servers", serversArray);
    obs_data_array_release(serversArray);

    // Chat configuration
    obs_data_set_bool(data, "chat_enabled", chat.enabled);
    obs_data_set_int(data, "chat_platform", static_cast<int>(chat.platform));
    obs_data_set_string(data, "chat_channel", chat.channel.c_str());
    obs_data_set_string(data, "chat_bot_username", chat.botUsername.c_str());
    obs_data_set_string(data, "chat_oauth_token", chat.oauthToken.c_str());
    obs_data_set_bool(data, "chat_announce", chat.announceSceneChanges);
    
    // Chat admins
    obs_data_array_t *adminsArray = obs_data_array_create();
    for (const auto &admin : chat.admins) {
        obs_data_t *adminData = obs_data_create();
        obs_data_set_string(adminData, "name", admin.c_str());
        obs_data_array_push_back(adminsArray, adminData);
        obs_data_release(adminData);
    }
    obs_data_set_array(data, "chat_admins", adminsArray);
    obs_data_array_release(adminsArray);
    
    // Chat commands
    obs_data_set_string(data, "chat_cmd_live", chat.cmdLive.c_str());
    obs_data_set_string(data, "chat_cmd_low", chat.cmdLow.c_str());
    obs_data_set_string(data, "chat_cmd_brb", chat.cmdBrb.c_str());
    obs_data_set_string(data, "chat_cmd_refresh", chat.cmdRefresh.c_str());
    obs_data_set_string(data, "chat_cmd_status", chat.cmdStatus.c_str());
    obs_data_set_string(data, "chat_cmd_trigger", chat.cmdTrigger.c_str());
    obs_data_set_string(data, "chat_cmd_fix", chat.cmdFix.c_str());

    // Message templates
    obs_data_set_string(data, "msg_switched_live", messages.switchedToLive.c_str());
    obs_data_set_string(data, "msg_switched_low", messages.switchedToLow.c_str());
    obs_data_set_string(data, "msg_switched_offline", messages.switchedToOffline.c_str());
    obs_data_set_string(data, "msg_status", messages.statusResponse.c_str());
    obs_data_set_string(data, "msg_status_offline", messages.statusOffline.c_str());
    obs_data_set_string(data, "msg_refreshing", messages.refreshing.c_str());
    obs_data_set_string(data, "msg_fix", messages.fixAttempt.c_str());
    obs_data_set_string(data, "msg_stream_started", messages.streamStarted.c_str());
    obs_data_set_string(data, "msg_stream_stopped", messages.streamStopped.c_str());
    obs_data_set_string(data, "msg_scene_switched", messages.sceneSwitched.c_str());

    // Custom commands
    obs_data_array_t *customCmdsArray = obs_data_array_create();
    for (const auto &cmd : customCommands) {
        obs_data_t *cmdData = obs_data_create();
        obs_data_set_string(cmdData, "trigger", cmd.trigger.c_str());
        obs_data_set_string(cmdData, "response", cmd.response.c_str());
        obs_data_set_bool(cmdData, "enabled", cmd.enabled);
        obs_data_array_push_back(customCmdsArray, cmdData);
        obs_data_release(cmdData);
    }
    obs_data_set_array(data, "custom_commands", customCmdsArray);
    obs_data_array_release(customCmdsArray);

    return data;
}

void Config::load(obs_data_t *data)
{
    if (!data)
        return;

    // Core settings
    enabled = obs_data_get_bool(data, "enabled");
    onlyWhenStreaming = obs_data_get_bool(data, "only_when_streaming");
    instantRecover = obs_data_get_bool(data, "instant_recover");
    autoNotify = obs_data_get_bool(data, "auto_notify");
    retryAttempts = static_cast<uint8_t>(obs_data_get_int(data, "retry_attempts"));
    if (retryAttempts == 0) retryAttempts = 5;

    // Triggers
    triggers.low = static_cast<uint32_t>(obs_data_get_int(data, "trigger_low"));
    triggers.rtt = static_cast<uint32_t>(obs_data_get_int(data, "trigger_rtt"));
    triggers.offline = static_cast<uint32_t>(obs_data_get_int(data, "trigger_offline"));
    triggers.rttOffline = static_cast<uint32_t>(obs_data_get_int(data, "trigger_rtt_offline"));

    // Switching scenes
    const char *normal = obs_data_get_string(data, "scene_normal");
    const char *low = obs_data_get_string(data, "scene_low");
    const char *offline = obs_data_get_string(data, "scene_offline");
    if (normal && *normal) scenes.normal = normal;
    if (low && *low) scenes.low = low;
    if (offline && *offline) scenes.offline = offline;

    // Optional scenes
    const char *starting = obs_data_get_string(data, "scene_starting");
    const char *ending = obs_data_get_string(data, "scene_ending");
    const char *privacy = obs_data_get_string(data, "scene_privacy");
    const char *refresh = obs_data_get_string(data, "scene_refresh");
    if (starting) optionalScenes.starting = starting;
    if (ending) optionalScenes.ending = ending;
    if (privacy) optionalScenes.privacy = privacy;
    if (refresh) optionalScenes.refresh = refresh;

    // Optional options
    options.offlineTimeoutMinutes = static_cast<uint32_t>(obs_data_get_int(data, "offline_timeout"));
    options.recordWhileStreaming = obs_data_get_bool(data, "record_while_streaming");
    options.switchToStartingOnStreamStart = obs_data_get_bool(data, "switch_to_starting");
    options.switchFromStartingToLive = obs_data_get_bool(data, "switch_from_starting");

    // Stream servers
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
            server.application = obs_data_get_string(serverData, "application");
            server.key = obs_data_get_string(serverData, "key");
            server.id = obs_data_get_string(serverData, "id");
            server.authUser = obs_data_get_string(serverData, "auth_user");
            server.authPass = obs_data_get_string(serverData, "auth_pass");
            server.priority = static_cast<int>(obs_data_get_int(serverData, "priority"));
            server.enabled = obs_data_get_bool(serverData, "enabled");
            
            // Override scenes
            server.overrideScenes.enabled = obs_data_get_bool(serverData, "override_enabled");
            const char *ovNormal = obs_data_get_string(serverData, "override_normal");
            const char *ovLow = obs_data_get_string(serverData, "override_low");
            const char *ovOffline = obs_data_get_string(serverData, "override_offline");
            if (ovNormal) server.overrideScenes.normal = ovNormal;
            if (ovLow) server.overrideScenes.low = ovLow;
            if (ovOffline) server.overrideScenes.offline = ovOffline;
            
            // Depends on
            server.dependsOn.enabled = obs_data_get_bool(serverData, "depends_enabled");
            const char *dependsServer = obs_data_get_string(serverData, "depends_server");
            if (dependsServer) server.dependsOn.serverName = dependsServer;
            
            servers.push_back(server);
            obs_data_release(serverData);
        }
        obs_data_array_release(serversArray);
    }

    sortServersByPriority();

    // Chat configuration
    chat.enabled = obs_data_get_bool(data, "chat_enabled");
    chat.platform = static_cast<ChatPlatform>(obs_data_get_int(data, "chat_platform"));
    const char *chatChannel = obs_data_get_string(data, "chat_channel");
    const char *chatBot = obs_data_get_string(data, "chat_bot_username");
    const char *chatOauth = obs_data_get_string(data, "chat_oauth_token");
    if (chatChannel) chat.channel = chatChannel;
    if (chatBot) chat.botUsername = chatBot;
    if (chatOauth) chat.oauthToken = chatOauth;
    chat.announceSceneChanges = obs_data_get_bool(data, "chat_announce");
    
    // Chat admins
    chat.admins.clear();
    obs_data_array_t *adminsArray = obs_data_get_array(data, "chat_admins");
    if (adminsArray) {
        size_t adminCount = obs_data_array_count(adminsArray);
        for (size_t i = 0; i < adminCount; i++) {
            obs_data_t *adminData = obs_data_array_item(adminsArray, i);
            const char *adminName = obs_data_get_string(adminData, "name");
            if (adminName && *adminName) {
                chat.admins.push_back(adminName);
            }
            obs_data_release(adminData);
        }
        obs_data_array_release(adminsArray);
    }
    
    // Chat commands
    const char *cmdLive = obs_data_get_string(data, "chat_cmd_live");
    const char *cmdLow = obs_data_get_string(data, "chat_cmd_low");
    const char *cmdBrb = obs_data_get_string(data, "chat_cmd_brb");
    const char *cmdRefresh = obs_data_get_string(data, "chat_cmd_refresh");
    const char *cmdStatus = obs_data_get_string(data, "chat_cmd_status");
    const char *cmdTrigger = obs_data_get_string(data, "chat_cmd_trigger");
    const char *cmdFix = obs_data_get_string(data, "chat_cmd_fix");
    if (cmdLive && *cmdLive) chat.cmdLive = cmdLive;
    if (cmdLow && *cmdLow) chat.cmdLow = cmdLow;
    if (cmdBrb && *cmdBrb) chat.cmdBrb = cmdBrb;
    if (cmdRefresh && *cmdRefresh) chat.cmdRefresh = cmdRefresh;
    if (cmdStatus && *cmdStatus) chat.cmdStatus = cmdStatus;
    if (cmdTrigger && *cmdTrigger) chat.cmdTrigger = cmdTrigger;
    if (cmdFix && *cmdFix) chat.cmdFix = cmdFix;

    // Message templates (only override if saved value is non-empty)
    auto loadMsg = [&](const char *key, std::string &dest) {
        const char *val = obs_data_get_string(data, key);
        if (val && *val) dest = val;
    };
    loadMsg("msg_switched_live", messages.switchedToLive);
    loadMsg("msg_switched_low", messages.switchedToLow);
    loadMsg("msg_switched_offline", messages.switchedToOffline);
    loadMsg("msg_status", messages.statusResponse);
    loadMsg("msg_status_offline", messages.statusOffline);
    loadMsg("msg_refreshing", messages.refreshing);
    loadMsg("msg_fix", messages.fixAttempt);
    loadMsg("msg_stream_started", messages.streamStarted);
    loadMsg("msg_stream_stopped", messages.streamStopped);
    loadMsg("msg_scene_switched", messages.sceneSwitched);

    // Custom commands
    customCommands.clear();
    obs_data_array_t *customCmdsArray = obs_data_get_array(data, "custom_commands");
    if (customCmdsArray) {
        size_t cmdCount = obs_data_array_count(customCmdsArray);
        for (size_t i = 0; i < cmdCount; i++) {
            obs_data_t *cmdData = obs_data_array_item(customCmdsArray, i);
            CustomChatCommand cmd;
            const char *trigger = obs_data_get_string(cmdData, "trigger");
            const char *response = obs_data_get_string(cmdData, "response");
            if (trigger && *trigger) cmd.trigger = trigger;
            if (response) cmd.response = response;
            cmd.enabled = obs_data_get_bool(cmdData, "enabled");
            if (!cmd.trigger.empty()) {
                customCommands.push_back(cmd);
            }
            obs_data_release(cmdData);
        }
        obs_data_array_release(customCmdsArray);
    }
}

} // namespace BitrateSwitch
