#pragma once

#include <obs.h>
#include <string>
#include <vector>
#include <optional>

namespace BitrateSwitch {

// All supported server types from NOALBS
enum class ServerType {
    Belabox = 0,
    Nginx,
    SrtLiveServer,
    Mediamtx,
    NodeMediaServer,
    Nimble,
    Rist,
    OpenIRL,
    IrlHosting,
    Xiu
};

struct Triggers {
    uint32_t low = 800;           // Low bitrate threshold (kbps)
    uint32_t rtt = 2500;          // RTT threshold for low scene (ms)
    uint32_t offline = 0;         // Offline bitrate threshold (0 = disabled)
    uint32_t rttOffline = 0;      // RTT threshold for offline scene (ms)
};

struct SwitchingScenes {
    std::string normal = "Live";
    std::string low = "Low";
    std::string offline = "Offline";
};

// Optional scenes from NOALBS
struct OptionalScenes {
    std::string starting;         // Scene to switch to when stream starts
    std::string ending;           // Scene to switch to when stream ends
    std::string privacy;          // Privacy scene (manual switch)
    std::string refresh;          // Refresh scene for fixing issues
};

// Override scenes per server
struct OverrideScenes {
    std::string normal;
    std::string low;
    std::string offline;
    bool enabled = false;
};

// Server dependency (for backup servers)
struct DependsOn {
    std::string serverName;
    SwitchingScenes backupScenes;
    bool enabled = false;
};

struct StreamServerConfig {
    ServerType type = ServerType::Belabox;
    std::string name;
    std::string statsUrl;
    std::string publisher;
    std::string application;      // For NGINX/NMS
    std::string key;              // Stream key
    std::string id;               // For Nimble
    std::string authUser;         // For authenticated servers
    std::string authPass;
    int priority = 0;
    bool enabled = true;
    OverrideScenes overrideScenes;
    DependsOn dependsOn;
};

// Optional options from NOALBS
struct OptionalOptions {
    uint32_t offlineTimeoutMinutes = 0;       // Auto-stop after X minutes offline (0 = disabled)
    bool recordWhileStreaming = false;         // Auto-record when streaming
    bool switchToStartingOnStreamStart = false; // Switch to starting scene on stream start
    bool switchFromStartingToLive = false;     // Auto-switch from starting to live when feed detected
};

class Config {
public:
    Config();
    ~Config();

    obs_data_t *save();
    void load(obs_data_t *data);
    void sortServersByPriority();

    // Core settings
    bool enabled = true;
    bool onlyWhenStreaming = false;
    bool instantRecover = true;
    bool autoNotify = true;
    uint8_t retryAttempts = 5;

    Triggers triggers;
    SwitchingScenes scenes;
    OptionalScenes optionalScenes;
    OptionalOptions options;
    std::vector<StreamServerConfig> servers;

private:
    void setDefaults();
};

// Helper to get server type name
const char* getServerTypeName(ServerType type);
ServerType getServerTypeFromName(const std::string& name);

} // namespace BitrateSwitch
