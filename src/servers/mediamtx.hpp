#pragma once

#include "../stream-server.hpp"
#include <chrono>

namespace BitrateSwitch {

class MediamtxServer : public StreamServer {
public:
    explicit MediamtxServer(const StreamServerConfig &config);
    ~MediamtxServer() override = default;

    SwitchType checkSwitch(const Triggers &triggers) override;
    BitrateInfo getBitrate() override;

private:
    BitrateInfo fetchStats();
    double fetchSrtRtt(const std::string &sourceId);

    // Cache for bitrate calculation from bytesReceived delta
    uint64_t prevBytesReceived_ = 0;
    std::chrono::steady_clock::time_point lastTimestamp_;
    int64_t cachedBitrateKbps_ = 0;
    bool cacheInitialized_ = false;
};

} // namespace BitrateSwitch
