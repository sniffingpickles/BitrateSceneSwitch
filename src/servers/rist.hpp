#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

class RistServer : public StreamServer {
public:
    explicit RistServer(const StreamServerConfig &config);
    ~RistServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;
    std::string getSourceInfo() override;

private:
    // Unified entry point – decides between HTTP and WebSocket
    BitrateInfo fetchStats();

    // Original HTTP implementation
    BitrateInfo fetchStatsHttp();
    // New WebSocket implementation
    BitrateInfo fetchStatsWs();

    std::string statsUrl_;
    std::string name_;
    OverrideScenes overrideScenes_;   // <-- correct type
    HttpClient httpClient_;
};

} // namespace BitrateSwitch
