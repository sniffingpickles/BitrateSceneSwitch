#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

class OpenRTMPServer : public StreamServer {
public:
    explicit OpenRTMPServer(const StreamServerConfig &config);

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;

private:
    BitrateInfo fetchStats();
};

} // namespace BitrateSwitch
