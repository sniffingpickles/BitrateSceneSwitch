#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

class NginxServer : public StreamServer {
public:
    explicit NginxServer(const StreamServerConfig &config);
    ~NginxServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;

private:
    BitrateInfo fetchStats();
    std::string application_;
};

} // namespace BitrateSwitch
