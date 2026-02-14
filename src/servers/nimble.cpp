#include "nimble.hpp"
#include "../switcher.hpp"
#include <cmath>

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

NimbleServer::NimbleServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    id_ = config.id;
    application_ = config.application;
    key_ = config.key;
    name_ = config.name;
    overrideScenes_ = config.overrideScenes;
}

BitrateInfo NimbleServer::fetchStats()
{
    BitrateInfo info;
    info.serverName = name_;

    // Fetch SRT receiver stats
    std::string srtUrl = statsUrl_ + "/manage/srt_receiver_stats";
    HttpResponse srtResponse = httpClient_.get(srtUrl);
    if (!srtResponse.success) return info;

    // Find receiver matching our ID
    size_t idPos = srtResponse.body.find("\"id\":\"" + id_);
    if (idPos == std::string::npos) return info;

    // Check state
    size_t statePos = srtResponse.body.find("\"state\":", idPos);
    if (statePos != std::string::npos) {
        std::string stateBlock = srtResponse.body.substr(statePos, 50);
        if (stateBlock.find("disconnected") != std::string::npos) {
            return info;
        }
    }

    // Extract RTT from link stats
    std::string linkStats = extractNestedObject(srtResponse.body.substr(idPos), "link");
    if (!linkStats.empty()) {
        std::string rttStr = extractJsonValue(linkStats, "rtt");
        if (!rttStr.empty()) {
            info.rttMs = std::stod(rttStr);
        }
    }

    // Fetch RTMP stats for bitrate
    std::string rtmpUrl = statsUrl_ + "/manage/rtmp_status";
    HttpResponse rtmpResponse = httpClient_.get(rtmpUrl);
    if (rtmpResponse.success) {
        // Find app and stream
        size_t appPos = rtmpResponse.body.find("\"app\":\"" + application_ + "\"");
        if (appPos != std::string::npos) {
            size_t strmPos = rtmpResponse.body.find("\"strm\":\"" + key_ + "\"", appPos);
            if (strmPos != std::string::npos) {
                // Find bandwidth near this stream
                size_t bwPos = rtmpResponse.body.rfind("\"bandwidth\":", strmPos);
                if (bwPos != std::string::npos && bwPos > appPos) {
                    std::string bwStr = extractJsonValue(rtmpResponse.body.substr(bwPos), "bandwidth");
                    if (!bwStr.empty()) {
                        // Remove quotes if present
                        if (bwStr.front() == '"') bwStr = bwStr.substr(1, bwStr.size() - 2);
                        int64_t bw = std::stoll(bwStr);
                        info.bitrateKbps = bw / 1024;
                    }
                }
            }
        }
    }

    info.isOnline = info.bitrateKbps > 0 || info.rttMs > 0;
    return info;
}

SwitchType NimbleServer::checkSwitch(const Triggers &triggers)
{
    BitrateInfo info = fetchStats();
    return evaluateTriggers(info, triggers);
}

BitrateInfo NimbleServer::getBitrate()
{
    BitrateInfo info = fetchStats();
    if (info.bitrateKbps > 0) {
        info.message = std::to_string(info.bitrateKbps) + " kbps, " + 
                       std::to_string(static_cast<int>(std::round(info.rttMs))) + " ms";
    }
    return info;
}

std::string NimbleServer::getSourceInfo()
{
    BitrateInfo info = fetchStats();
    if (!info.isOnline) return "Offline";
    
    return std::to_string(info.bitrateKbps) + " Kbps, " +
           std::to_string(static_cast<int>(std::round(info.rttMs))) + " ms RTT";
}

} // namespace BitrateSwitch
