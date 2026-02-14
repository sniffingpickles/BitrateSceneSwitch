#include "stream-server.hpp"
#include "switcher.hpp"
#include "servers/belabox.hpp"
#include "servers/nginx.hpp"
#include "servers/sls.hpp"
#include "servers/mediamtx.hpp"
#include <obs-module.h>

namespace BitrateSwitch {

SwitchType StreamServer::evaluateTriggers(const BitrateInfo &info, const Triggers &triggers)
{
    if (!info.isOnline || info.bitrateKbps == 0)
        return SwitchType::Offline;

    if (triggers.offline > 0 && info.bitrateKbps > 0 && 
        info.bitrateKbps <= static_cast<int64_t>(triggers.offline))
        return SwitchType::Offline;

    if (triggers.rttOffline > 0 && info.rttMs >= static_cast<double>(triggers.rttOffline))
        return SwitchType::Offline;

    if (info.bitrateKbps == 1)
        return SwitchType::Previous;

    if (triggers.low > 0 && info.bitrateKbps <= static_cast<int64_t>(triggers.low))
        return SwitchType::Low;

    if (triggers.rtt > 0 && info.rttMs >= static_cast<double>(triggers.rtt))
        return SwitchType::Low;

    return SwitchType::Normal;
}

std::unique_ptr<StreamServer> StreamServer::create(const StreamServerConfig &config)
{
    switch (config.type) {
    case ServerType::Belabox:
        return std::make_unique<BelaboxServer>(config);
    case ServerType::Nginx:
        return std::make_unique<NginxServer>(config);
    case ServerType::SrtLiveServer:
        return std::make_unique<SlsServer>(config);
    case ServerType::Mediamtx:
        return std::make_unique<MediamtxServer>(config);
    default:
        return std::make_unique<BelaboxServer>(config);
    }
}

} // namespace BitrateSwitch
