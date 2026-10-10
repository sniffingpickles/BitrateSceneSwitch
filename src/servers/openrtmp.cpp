#include "openrtmp.hpp"
#include "../switcher.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>
#include <cstdint>
#include <limits>

namespace BitrateSwitch {

OpenRTMPServer::OpenRTMPServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    name_ = config.name;
    overrideScenes_ = config.overrideScenes;
}

BitrateInfo OpenRTMPServer::fetchStats()
{
    BitrateInfo info;
    info.serverName = name_;

    if (statsUrl_.empty())
        return info;

    HttpResponse response = httpClient_.get(statsUrl_);
    if (!response.success)
        return info;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray::fromStdString(response.body), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return info;

    QJsonObject stats = document.object();

    if (stats.contains("streams")) {
        const QJsonValue streams = stats.value("streams");
        if (!streams.isArray() || streams.toArray().isEmpty())
            return info;
        stats = streams.toArray().first().toObject();
    }

    const QJsonValue bitrate = stats.value("bitrate_kbps");
    if (!bitrate.isDouble())
        return info;

    const double kbps = bitrate.toDouble();
    if (!std::isfinite(kbps) || kbps <= 0.0 ||
        kbps >= static_cast<double>((std::numeric_limits<int64_t>::max)()))
        return info;

    info.bitrateKbps = static_cast<int64_t>(std::llround(kbps));
    if (info.bitrateKbps == 0)
        return info;

    const QJsonValue rtt = stats.value("rtt_ms");
    if (rtt.isDouble()) {
        const double rttMs = rtt.toDouble();
        if (std::isfinite(rttMs) && rttMs >= 0.0 &&
            rttMs < static_cast<double>((std::numeric_limits<int>::max)()))
            info.rttMs = rttMs;
    }

    info.isOnline = true;
    return info;
}

SwitchType OpenRTMPServer::checkSwitch(const Triggers &triggers)
{
    return evaluateTriggers(fetchStats(), triggers);
}

BitrateInfo OpenRTMPServer::getBitrate()
{
    BitrateInfo info = fetchStats();
    if (info.isOnline) {
        info.message = std::to_string(info.bitrateKbps) + " kbps, " +
                       std::to_string(static_cast<int>(std::round(info.rttMs))) + " ms";
    }
    return info;
}

} // namespace BitrateSwitch
