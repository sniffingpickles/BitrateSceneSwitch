#pragma once

#include "../stream-server.hpp"

namespace BitrateSwitch {

// same stats format as SLS, just branded for IRLHosting
class IrlHostingServer : public StreamServer {
public:
    explicit IrlHostingServer(const StreamServerConfig &config);
    ~IrlHostingServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;

private:
    BitrateInfo fetchStats();
    std::string apiKey_;
};

} // namespace BitrateSwitch
