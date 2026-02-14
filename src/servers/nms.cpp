#include "nms.hpp"
#include "../switcher.hpp"

namespace {

std::string extractJsonValue(const std::string &json, const std::string &key)
{
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";

    size_t valueStart = json.find_first_not_of(" \t\n\r", colonPos + 1);
    if (valueStart == std::string::npos) return "";

    if (json[valueStart] == '"') {
        size_t valueEnd = json.find('"', valueStart + 1);
        if (valueEnd == std::string::npos) return "";
        return json.substr(valueStart + 1, valueEnd - valueStart - 1);
    }

    size_t valueEnd = json.find_first_of(",}\n\r", valueStart);
    if (valueEnd == std::string::npos) valueEnd = json.length();
    
    std::string value = json.substr(valueStart, valueEnd - valueStart);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.pop_back();
    return value;
}

} // anonymous namespace

namespace BitrateSwitch {

NmsServer::NmsServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    application_ = config.application;
    key_ = config.key;
    name_ = config.name;
    authUser_ = config.authUser;
    authPass_ = config.authPass;
    overrideScenes_ = config.overrideScenes;
}

BitrateInfo NmsServer::fetchStats()
{
    BitrateInfo info;
    info.serverName = name_;

    // Build URL: statsUrl/application/key
    std::string url = statsUrl_;
    if (!application_.empty()) {
        if (url.back() != '/') url += '/';
        url += application_;
    }
    if (!key_.empty()) {
        if (url.back() != '/') url += '/';
        url += key_;
    }

    HttpResponse response = httpClient_.get(url);
    if (!response.success) return info;

    // Parse NMS JSON response
    std::string isLiveStr = extractJsonValue(response.body, "isLive");
    std::string bitrateStr = extractJsonValue(response.body, "bitrate");

    if (isLiveStr != "true") return info;

    if (!bitrateStr.empty()) {
        info.bitrateKbps = std::stoll(bitrateStr);
    }

    info.isOnline = info.bitrateKbps > 0;
    return info;
}

SwitchType NmsServer::checkSwitch(const Triggers &triggers)
{
    BitrateInfo info = fetchStats();
    
    // NMS: bitrate 0 means return to previous
    if (info.isOnline && info.bitrateKbps == 0)
        return SwitchType::Previous;
    
    return evaluateTriggers(info, triggers);
}

BitrateInfo NmsServer::getBitrate()
{
    BitrateInfo info = fetchStats();
    if (info.bitrateKbps > 0) {
        info.message = std::to_string(info.bitrateKbps) + " kbps";
    }
    return info;
}

} // namespace BitrateSwitch
