#include "stream-server.hpp"
#include "switcher.hpp"
#include "servers/belabox.hpp"
#include "servers/nginx.hpp"
#include "servers/sls.hpp"
#include "servers/mediamtx.hpp"
#include "servers/nms.hpp"
#include "servers/nimble.hpp"
#include "servers/rist.hpp"
#include "servers/openirl.hpp"
#include "servers/xiu.hpp"
#include <obs-module.h>

namespace BitrateSwitch {

SwitchType StreamServer::evaluateTriggers(const BitrateInfo &info, const Triggers &triggers)
{
    if (!info.isOnline || info.bitrateKbps == 0)
        return SwitchType::Offline;

    // Check offline triggers first
    if (triggers.offline > 0 && info.bitrateKbps > 0 && 
        info.bitrateKbps <= static_cast<int64_t>(triggers.offline))
        return SwitchType::Offline;

    if (triggers.rttOffline > 0 && info.rttMs >= static_cast<double>(triggers.rttOffline))
        return SwitchType::Offline;

    // Special case: bitrate of 1 means return to previous scene
    if (info.bitrateKbps == 1)
        return SwitchType::Previous;

    // Check low triggers
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
    case ServerType::NodeMediaServer:
        return std::make_unique<NmsServer>(config);
    case ServerType::Nimble:
        return std::make_unique<NimbleServer>(config);
    case ServerType::Rist:
        return std::make_unique<RistServer>(config);
    case ServerType::OpenIRL:
        return std::make_unique<OpenIRLServer>(config);
    case ServerType::IrlHosting:
        return std::make_unique<SlsServer>(config);
    case ServerType::Xiu:
        return std::make_unique<XiuServer>(config);
    default:
        blog(LOG_WARNING, "[BitrateSceneSwitch] Unknown server type %d, using Belabox", 
             static_cast<int>(config.type));
        return std::make_unique<BelaboxServer>(config);
    }
}

} // namespace BitrateSwitch
