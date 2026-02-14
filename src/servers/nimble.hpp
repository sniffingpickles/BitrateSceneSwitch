#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

// Nimble Streamer
class NimbleServer : public StreamServer {
public:
    explicit NimbleServer(const StreamServerConfig &config);
    ~NimbleServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;
    std::string getSourceInfo() override;

private:
    BitrateInfo fetchStats();
};

} // namespace BitrateSwitch
