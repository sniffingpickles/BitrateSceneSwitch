#include "openirl.hpp"
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

OpenIRLServer::OpenIRLServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    name_ = config.name;
    overrideScenes_ = config.overrideScenes;
}

BitrateInfo OpenIRLServer::fetchStats()
{
    BitrateInfo info;
    info.serverName = name_;

    HttpResponse response = httpClient_.get(statsUrl_);
    if (!response.success) return info;

    // OpenIRL JSON format:
    // { "publisher": { "bitrate": 5000, "rtt": 10.5, "dropped_pkts": 0, "latency": 120, "buffer": 500, "uptime": 3600 } }
    // If "publisher" field is absent, the stream is offline.
    std::string publisherData = extractNestedObject(response.body, "publisher");
    if (publisherData.empty()) return info;

    std::string bitrateStr = extractJsonValue(publisherData, "bitrate");
    std::string rttStr = extractJsonValue(publisherData, "rtt");
    std::string droppedStr = extractJsonValue(publisherData, "dropped_pkts");
    std::string latencyStr = extractJsonValue(publisherData, "latency");

    try {
        if (!bitrateStr.empty()) info.bitrateKbps = std::stoll(bitrateStr);
        if (!rttStr.empty()) info.rttMs = std::stod(rttStr);
        if (!droppedStr.empty()) info.droppedPackets = std::stoi(droppedStr);
    } catch (...) {
        return info;  // Return defaults on parse error
    }

    info.isOnline = info.bitrateKbps > 0;
    return info;
}

SwitchType OpenIRLServer::checkSwitch(const Triggers &triggers)
{
    BitrateInfo info = fetchStats();
    return evaluateTriggers(info, triggers);
}

BitrateInfo OpenIRLServer::getBitrate()
{
    BitrateInfo info = fetchStats();
    if (info.bitrateKbps > 0) {
        info.message = std::to_string(info.bitrateKbps) + " Kbps, " +
                       std::to_string(static_cast<int>(std::round(info.rttMs))) + " ms";
    }
    return info;
}

std::string OpenIRLServer::getSourceInfo()
{
    BitrateInfo info = fetchStats();
    if (!info.isOnline) return "Offline";

    std::string result = std::to_string(info.bitrateKbps) + " Kbps, " +
                         std::to_string(static_cast<int>(std::round(info.rttMs))) + " ms";
    result += " | dropped " + std::to_string(info.droppedPackets) + " packets";
    return result;
}

} // namespace BitrateSwitch
