#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

// Node Media Server
class NmsServer : public StreamServer {
public:
    explicit NmsServer(const StreamServerConfig &config);
    ~NmsServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;

private:
    BitrateInfo fetchStats();
};

} // namespace BitrateSwitch
