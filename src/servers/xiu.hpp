#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

class XiuServer : public StreamServer {
public:
    explicit XiuServer(const StreamServerConfig &config);
    ~XiuServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;
    std::string getSourceInfo() override;

private:
    BitrateInfo fetchStats();
};

} // namespace BitrateSwitch
