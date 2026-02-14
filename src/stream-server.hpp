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
    int bytesLost = 0;
    double mbpsBandwidth = 0.0;
    double mbpsRecvRate = 0.0;
    bool isOnline = false;
    std::string message;
    std::string serverName;
};

class StreamServer {
public:
    virtual ~StreamServer() = default;

    virtual SwitchType checkSwitch(const Triggers &triggers) = 0;
    virtual BitrateInfo getBitrate() = 0;
    virtual std::string getSourceInfo() { return getBitrate().message; }

    static std::unique_ptr<StreamServer> create(const StreamServerConfig &config);

    // Server metadata
    std::string getName() const { return name_; }
    bool hasOverrideScenes() const { return overrideScenes_.enabled; }
    const OverrideScenes& getOverrideScenes() const { return overrideScenes_; }

protected:
    HttpClient httpClient_;
    std::string statsUrl_;
    std::string publisher_;
    std::string application_;
    std::string key_;
    std::string id_;
    std::string name_;
    std::string authUser_;
    std::string authPass_;
    OverrideScenes overrideScenes_;

    SwitchType evaluateTriggers(const BitrateInfo &info, const Triggers &triggers);
};

} // namespace BitrateSwitch
