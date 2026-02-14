#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

class SlsServer : public StreamServer {
public:
    explicit SlsServer(const StreamServerConfig &config);
    ~SlsServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;

private:
    BitrateInfo fetchStats();
};

} // namespace BitrateSwitch
