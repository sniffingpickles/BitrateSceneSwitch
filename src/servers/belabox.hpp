#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

class BelaboxServer : public StreamServer {
public:
    explicit BelaboxServer(const StreamServerConfig &config);
    ~BelaboxServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;

private:
    BitrateInfo fetchStats();
};

} // namespace BitrateSwitch
