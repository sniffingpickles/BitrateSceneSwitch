#include "rist.hpp"
#include "../switcher.hpp"
#include <cmath>
#include <functional>
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

// Extract a JSON array (returns content including brackets)
std::string extractJsonArray(const std::string &json, const std::string &key)
{
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t bracketPos = json.find('[', keyPos);
    if (bracketPos == std::string::npos) return "";

    int depth = 1;
    size_t endPos = bracketPos + 1;
    while (depth > 0 && endPos < json.length()) {
        if (json[endPos] == '[') depth++;
        else if (json[endPos] == ']') depth--;
        endPos++;
    }

    return json.substr(bracketPos, endPos - bracketPos);
}

// Iterate over objects in a JSON array, calling fn for each object body
void forEachObjectInArray(const std::string &arrayJson,
                          const std::function<void(const std::string &)> &fn)
{
    size_t pos = 0;
    while (pos < arrayJson.length()) {
        size_t objStart = arrayJson.find('{', pos);
        if (objStart == std::string::npos) break;

        int depth = 1;
        size_t endPos = objStart + 1;
        while (depth > 0 && endPos < arrayJson.length()) {
            if (arrayJson[endPos] == '{') depth++;
            else if (arrayJson[endPos] == '}') depth--;
            endPos++;
        }

        std::string obj = arrayJson.substr(objStart, endPos - objStart);
        fn(obj);
        pos = endPos;
    }
}

} // anonymous namespace

namespace BitrateSwitch {

RistServer::RistServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    name_ = config.name;
    overrideScenes_ = config.overrideScenes;
}

BitrateInfo RistServer::fetchStats()
{
    BitrateInfo info;
    info.serverName = name_;

    HttpResponse response = httpClient_.get(statsUrl_);
    if (!response.success) return info;

    // RIST JSON format:
    // { "receiver-stats": { "flowinstant": { "peers": [ { "stats": { "rtt": ..., "bitrate": ... } }, ... ] } } }
    std::string receiverStats = extractNestedObject(response.body, "receiver-stats");
    if (receiverStats.empty()) return info;

    std::string flowinstant = extractNestedObject(receiverStats, "flowinstant");
    if (flowinstant.empty()) return info;

    std::string peersArray = extractJsonArray(flowinstant, "peers");
    if (peersArray.empty()) return info;

    // Sum bitrate across all peers, average RTT
    int64_t totalBitrate = 0;
    double totalRtt = 0.0;
    int peerCount = 0;

    forEachObjectInArray(peersArray, [&](const std::string &peerObj) {
        std::string statsObj = extractNestedObject(peerObj, "stats");
        if (statsObj.empty()) return;

        std::string bitrateStr = extractJsonValue(statsObj, "bitrate");
        std::string rttStr = extractJsonValue(statsObj, "rtt");

        try {
            if (!bitrateStr.empty()) {
                totalBitrate += std::stoll(bitrateStr);
            }
            if (!rttStr.empty()) {
                totalRtt += std::stod(rttStr);
            }
        } catch (...) {
            return;  // Skip this peer on parse error
        }
        peerCount++;
    });

    if (peerCount == 0) return info;

    // Bitrate from RIST is in bits/s, convert to kbps
    info.bitrateKbps = totalBitrate / 1024;
    info.rttMs = totalRtt / peerCount;
    info.isOnline = info.bitrateKbps > 0;

    return info;
}

SwitchType RistServer::checkSwitch(const Triggers &triggers)
{
    BitrateInfo info = fetchStats();
    return evaluateTriggers(info, triggers);
}

BitrateInfo RistServer::getBitrate()
{
    BitrateInfo info = fetchStats();
    if (info.bitrateKbps > 0) {
        info.message = std::to_string(info.bitrateKbps) + " kbps, " +
                       std::to_string(static_cast<int>(std::round(info.rttMs))) + " ms";
    }
    return info;
}

std::string RistServer::getSourceInfo()
{
    BitrateInfo info = fetchStats();
    if (!info.isOnline) return "Offline";

    return std::to_string(info.bitrateKbps) + " Kbps, " +
           std::to_string(static_cast<int>(std::round(info.rttMs))) + " ms";
}

} // namespace BitrateSwitch
