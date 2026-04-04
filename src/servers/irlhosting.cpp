#include "irlhosting.hpp"
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

    size_t valueEnd = json.find_first_of(",}\n\r", valueStart);
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

IrlHostingServer::IrlHostingServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    publisher_ = config.key;
    name_ = config.name;
    apiKey_ = config.authPass;
    overrideScenes_ = config.overrideScenes;
}

BitrateInfo IrlHostingServer::fetchStats()
{
    BitrateInfo info;
    info.serverName = name_;

    HttpResponse response;
    if (!apiKey_.empty()) {
        response = httpClient_.get(statsUrl_, apiKey_);
    } else {
        response = httpClient_.get(statsUrl_);
    }
    if (!response.success) return info;

    std::string publishers = extractNestedObject(response.body, "publishers");
    if (publishers.empty()) return info;

    std::string publisherData = extractNestedObject(publishers, publisher_);
    if (publisherData.empty()) return info;

    std::string bitrateStr = extractJsonValue(publisherData, "bitrate");
    std::string rttStr = extractJsonValue(publisherData, "rtt");
    std::string mbpsBwStr = extractJsonValue(publisherData, "mbps_bandwidth");
    std::string mbpsRecvStr = extractJsonValue(publisherData, "mbps_recv_rate");
    std::string pktDropStr = extractJsonValue(publisherData, "pkt_rcv_drop");
    std::string bytesLostStr = extractJsonValue(publisherData, "bytes_rcv_loss");

    try {
        if (!bitrateStr.empty()) info.bitrateKbps = std::stoll(bitrateStr);
        if (!rttStr.empty()) info.rttMs = std::stod(rttStr);
        if (!mbpsBwStr.empty()) info.mbpsBandwidth = std::stod(mbpsBwStr);
        if (!mbpsRecvStr.empty()) info.mbpsRecvRate = std::stod(mbpsRecvStr);
        if (!pktDropStr.empty()) info.droppedPackets = std::stoi(pktDropStr);
        if (!bytesLostStr.empty()) info.bytesLost = std::stoi(bytesLostStr);
    } catch (...) {
        return info;
    }

    info.isOnline = info.bitrateKbps > 0;
    return info;
}

SwitchType IrlHostingServer::checkSwitch(const Triggers &triggers)
{
    BitrateInfo info = fetchStats();
    return evaluateTriggers(info, triggers);
}

BitrateInfo IrlHostingServer::getBitrate()
{
    BitrateInfo info = fetchStats();
    if (info.bitrateKbps > 0) {
        info.message = std::to_string(info.bitrateKbps) + " kbps, " +
                       std::to_string(static_cast<int>(std::round(info.rttMs))) + " ms";
    }
    return info;
}

} // namespace BitrateSwitch
