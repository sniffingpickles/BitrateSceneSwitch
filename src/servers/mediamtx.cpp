#include "mediamtx.hpp"
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

    size_t valueEnd = json.find_first_of(",}\n\r]", valueStart);
    if (valueEnd == std::string::npos) valueEnd = json.length();
    
    std::string value = json.substr(valueStart, valueEnd - valueStart);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.pop_back();
    return value;
}

} // anonymous namespace

namespace BitrateSwitch {

MediamtxServer::MediamtxServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    publisher_ = config.publisher;
    name_ = config.name;
}

BitrateInfo MediamtxServer::fetchStats()
{
    BitrateInfo info;

    std::string url = statsUrl_;
    if (!publisher_.empty()) {
        if (url.back() != '/') url += '/';
        url += publisher_;
    }

    HttpResponse response = httpClient_.get(url);
    if (!response.success) return info;

    std::string readyStr = extractJsonValue(response.body, "ready");
    if (readyStr != "true") return info;

    std::string bytesReceivedStr = extractJsonValue(response.body, "bytesReceived");
    if (!bytesReceivedStr.empty()) {
        int64_t bytes = std::stoll(bytesReceivedStr);
        info.isOnline = bytes > 0;
        info.bitrateKbps = info.isOnline ? 1000 : 0;
    }

    return info;
}

SwitchType MediamtxServer::checkSwitch(const Triggers &triggers)
{
    BitrateInfo info = fetchStats();
    return evaluateTriggers(info, triggers);
}

BitrateInfo MediamtxServer::getBitrate()
{
    BitrateInfo info = fetchStats();
    if (info.isOnline) {
        info.message = "Connected";
    }
    return info;
}

} // namespace BitrateSwitch
