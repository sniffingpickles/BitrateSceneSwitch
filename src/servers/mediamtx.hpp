#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

class MediamtxServer : public StreamServer {
public:
    explicit MediamtxServer(const StreamServerConfig &config);
    ~MediamtxServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;

private:
    BitrateInfo fetchStats();
};

} // namespace BitrateSwitch
