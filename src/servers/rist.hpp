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
    BitrateInfo fetchStats();
};

} // namespace BitrateSwitch
