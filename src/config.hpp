#pragma once

#include <obs.h>
#include <string>
#include <vector>

namespace BitrateSwitch {

enum class ServerType {
    Belabox = 0,
    Nginx,
    SrtLiveServer,
    Mediamtx
};

struct Triggers {
    uint32_t low = 800;
    uint32_t rtt = 2500;
    uint32_t offline = 0;
    uint32_t rttOffline = 0;
};

struct SwitchingScenes {
    std::string normal = "Live";
    std::string low = "Low";
    std::string offline = "Offline";
};

struct StreamServerConfig {
    ServerType type = ServerType::Belabox;
    std::string name;
    std::string statsUrl;
    std::string publisher;
    int priority = 0;
    bool enabled = true;
};

class Config {
public:
    Config();
    ~Config();

    obs_data_t *save();
    void load(obs_data_t *data);

    // Settings
    bool enabled = true;
    bool onlyWhenStreaming = false;
    bool instantRecover = true;
    bool autoNotify = true;
    uint8_t retryAttempts = 5;

    Triggers triggers;
    SwitchingScenes scenes;
    std::vector<StreamServerConfig> servers;

    uint32_t offlineTimeoutMinutes = 0;

private:
    void setDefaults();
};

} // namespace BitrateSwitch
