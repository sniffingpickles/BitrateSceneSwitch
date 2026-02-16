#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

class IrlHostingServer : public StreamServer {
public:
    explicit IrlHostingServer(const StreamServerConfig &config);
    ~IrlHostingServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;
    std::string getSourceInfo() override;

private:
    BitrateInfo fetchStats();
    BitrateInfo parseSrtStats(const std::string &body);
    BitrateInfo parseRtmpStats(const std::string &body);
};

} // namespace BitrateSwitch
