#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

class OpenIRLServer : public StreamServer {
public:
    explicit OpenIRLServer(const StreamServerConfig &config);
    ~OpenIRLServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;
    std::string getSourceInfo() override;

private:
    BitrateInfo fetchStats();
};

} // namespace BitrateSwitch
