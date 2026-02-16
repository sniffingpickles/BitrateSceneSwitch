#include "mediamtx.hpp"
#include "../switcher.hpp"
#include <cmath>
#include <obs-module.h>

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

std::string extractNestedObject(const std::string &json, const std::string &key)
{
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t bracePos = json.find('{', keyPos);
    if (bracePos == std::string::npos) return "";

    int depth = 1;
    size_t endPos = bracePos + 1;
    while (depth > 0 && endPos < json.length()) {
        if (json[endPos] == '{') depth++;
        else if (json[endPos] == '}') depth--;
        endPos++;
    }

    return json.substr(bracePos, endPos - bracePos);
}

} // anonymous namespace

namespace BitrateSwitch {

MediamtxServer::MediamtxServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    publisher_ = config.key;
    name_ = config.name;
    authUser_ = config.authUser;
    authPass_ = config.authPass;
    overrideScenes_ = config.overrideScenes;
    lastTimestamp_ = std::chrono::steady_clock::now();
}

double MediamtxServer::fetchSrtRtt(const std::string &sourceId)
{
    // Derive the SRT stats URL from the main stats URL
    // e.g. http://localhost:9997/v3/paths/get/mystream -> http://localhost:9997/v3/srtconns/get/{id}
    size_t v3Pos = statsUrl_.find("/v3");
    if (v3Pos == std::string::npos) return 0.0;

    std::string srtUrl = statsUrl_.substr(0, v3Pos) + "/v3/srtconns/get/" + sourceId;

    HttpResponse response = httpClient_.get(srtUrl);
    if (!response.success) return 0.0;

    std::string rttStr = extractJsonValue(response.body, "msRTT");
    if (!rttStr.empty()) {
        try {
            return std::stod(rttStr);
        } catch (...) {
            return 0.0;
        }
    }
    return 0.0;
}

BitrateInfo MediamtxServer::fetchStats()
{
    BitrateInfo info;
    info.serverName = name_;

    // Build the full URL: statsUrl/publisher (e.g. /v3/paths/get/mystream)
    std::string url = statsUrl_;
    if (!publisher_.empty()) {
        if (url.back() != '/') url += '/';
        url += publisher_;
    }

    HttpResponse response = httpClient_.get(url);
    if (!response.success) return info;

    std::string readyStr = extractJsonValue(response.body, "ready");
    if (readyStr != "true") return info;

    // Check if source is SRT for RTT stats
    std::string sourceObj = extractNestedObject(response.body, "source");
    if (!sourceObj.empty()) {
        std::string sourceType = extractJsonValue(sourceObj, "type");
        std::string sourceId = extractJsonValue(sourceObj, "id");
        if (sourceType == "srtConn" && !sourceId.empty()) {
            info.rttMs = fetchSrtRtt(sourceId);
        }
    }

    // Calculate bitrate from bytesReceived delta over time
    std::string bytesReceivedStr = extractJsonValue(response.body, "bytesReceived");
    if (bytesReceivedStr.empty()) return info;

    uint64_t bytesReceived;
    try {
        bytesReceived = std::stoull(bytesReceivedStr);
    } catch (...) {
        return info;
    }

    if (bytesReceived == prevBytesReceived_ && cacheInitialized_) {
        // No new data since last poll — return cached bitrate instead of reporting offline
        info.bitrateKbps = cachedBitrateKbps_;
        info.isOnline = info.bitrateKbps > 0;
        return info;
    }

    auto now = std::chrono::steady_clock::now();

    if (!cacheInitialized_) {
        prevBytesReceived_ = bytesReceived;
        lastTimestamp_ = now;
        cacheInitialized_ = true;
        info.isOnline = bytesReceived > 0;
        info.bitrateKbps = 0;
        return info;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimestamp_);
    if (elapsed.count() >= 1000) {
        if (bytesReceived > prevBytesReceived_) {
            uint64_t diffBits = (bytesReceived - prevBytesReceived_) * 8;
            double bitsPerSecond = static_cast<double>(diffBits) / (elapsed.count() / 1000.0);
            double kbps = bitsPerSecond / 1024.0;
            cachedBitrateKbps_ = static_cast<int64_t>(kbps);
        }

        prevBytesReceived_ = bytesReceived;
        lastTimestamp_ = now;
    }

    info.bitrateKbps = cachedBitrateKbps_;
    info.isOnline = info.bitrateKbps > 0;
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
    if (info.bitrateKbps > 0) {
        info.message = std::to_string(info.bitrateKbps) + " kbps";
        if (info.rttMs > 0) {
            info.message += ", " + std::to_string(static_cast<int>(std::round(info.rttMs))) + " ms";
        }
    }
    return info;
}

} // namespace BitrateSwitch
