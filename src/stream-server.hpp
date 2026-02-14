#pragma once

#include <string>
#include <memory>
#include "config.hpp"
#include "http-client.hpp"

namespace BitrateSwitch {

enum class SwitchType;

struct BitrateInfo {
    int64_t bitrateKbps = 0;
    double rttMs = 0.0;
    int droppedPackets = 0;
    bool isOnline = false;
    std::string message;
};

class StreamServer {
public:
    virtual ~StreamServer() = default;

    virtual SwitchType checkSwitch(const Triggers &triggers) = 0;
    virtual BitrateInfo getBitrate() = 0;

    static std::unique_ptr<StreamServer> create(const StreamServerConfig &config);

protected:
    HttpClient httpClient_;
    std::string statsUrl_;
    std::string publisher_;
    std::string name_;

    SwitchType evaluateTriggers(const BitrateInfo &info, const Triggers &triggers);
};

} // namespace BitrateSwitch
