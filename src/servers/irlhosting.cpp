#include "irlhosting.hpp"
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

// Find an object in a JSON array where a given key matches a value
std::string findObjectInArray(const std::string &arrayJson, const std::string &key, const std::string &value)
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
        std::string foundValue = extractJsonValue(obj, key);
        if (foundValue == value) return obj;
        
        pos = endPos;
    }
    return "";
}

} // anonymous namespace

namespace BitrateSwitch {

IrlHostingServer::IrlHostingServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    publisher_ = config.publisher;  // SRT publisher stream ID (e.g. publish/live/feed1)
    application_ = config.application;  // RTMP application name
    key_ = config.key;  // RTMP stream key
    name_ = config.name;
    overrideScenes_ = config.overrideScenes;
}

BitrateInfo IrlHostingServer::parseSrtStats(const std::string &body)
{
    BitrateInfo info;
    info.serverName = name_;

    // SRT format: { "service": "SRT", "publishers": [ { "stream": "...", "bitrate": ..., "rtt": ... }, ... ] }
    std::string publishersArray = extractJsonArray(body, "publishers");
    if (publishersArray.empty()) return info;

    std::string publisherObj = findObjectInArray(publishersArray, "stream", publisher_);
    if (publisherObj.empty()) return info;

    std::string bitrateStr = extractJsonValue(publisherObj, "bitrate");
    std::string rttStr = extractJsonValue(publisherObj, "rtt");
    std::string mbpsBwStr = extractJsonValue(publisherObj, "mbpsBandwidth");
    std::string mbpsRecvStr = extractJsonValue(publisherObj, "mbpsRecvRate");

    if (!bitrateStr.empty()) info.bitrateKbps = std::stoll(bitrateStr);
    if (!rttStr.empty()) info.rttMs = std::stod(rttStr);
    if (!mbpsBwStr.empty()) info.mbpsBandwidth = std::stod(mbpsBwStr);
    if (!mbpsRecvStr.empty()) info.mbpsRecvRate = std::stod(mbpsRecvStr);

    info.isOnline = info.bitrateKbps > 0;
    return info;
}

BitrateInfo IrlHostingServer::parseRtmpStats(const std::string &body)
{
    BitrateInfo info;
    info.serverName = name_;

    // RTMP format: { "service": "RTMP", "applications": [ { "name": "...", "live": { "streams": [ { "name": "...", "bytes": { "incoming": ... } } ] } } ] }
    std::string applicationsArray = extractJsonArray(body, "applications");
    if (applicationsArray.empty()) return info;

    std::string appObj = findObjectInArray(applicationsArray, "name", application_);
    if (appObj.empty()) return info;

    std::string liveObj = extractNestedObject(appObj, "live");
    if (liveObj.empty()) return info;

    std::string streamsArray = extractJsonArray(liveObj, "streams");
    if (streamsArray.empty()) return info;

    std::string streamObj = findObjectInArray(streamsArray, "name", key_);
    if (streamObj.empty()) return info;

    std::string bytesObj = extractNestedObject(streamObj, "bytes");
    if (!bytesObj.empty()) {
        std::string incomingStr = extractJsonValue(bytesObj, "incoming");
        if (!incomingStr.empty()) {
            int64_t incoming = std::stoll(incomingStr);
            info.bitrateKbps = incoming / 1024;
        }
    }

    info.isOnline = info.bitrateKbps > 0;
    return info;
}

BitrateInfo IrlHostingServer::fetchStats()
{
    BitrateInfo info;
    info.serverName = name_;

    HttpResponse response = httpClient_.get(statsUrl_);
    if (!response.success) return info;

    // Determine service type from "service" field
    std::string service = extractJsonValue(response.body, "service");

    if (service == "SRT") {
        return parseSrtStats(response.body);
    } else if (service == "RTMP") {
        return parseRtmpStats(response.body);
    }

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
        if (info.rttMs > 0) {
            info.message = std::to_string(info.bitrateKbps) + " kbps, " +
                           std::to_string(static_cast<int>(std::round(info.rttMs))) + " ms";
        } else {
            info.message = std::to_string(info.bitrateKbps) + " kbps";
        }
    }
    return info;
}

std::string IrlHostingServer::getSourceInfo()
{
    BitrateInfo info = fetchStats();
    if (!info.isOnline) return "Offline";

    if (info.rttMs > 0) {
        std::string result = std::to_string(info.bitrateKbps) + " Kbps, " +
                             std::to_string(static_cast<int>(std::round(info.rttMs))) + " ms";
        if (info.mbpsBandwidth > 0) {
            result += " | Estimated bandwidth " + std::to_string(static_cast<int>(std::round(info.mbpsBandwidth))) +
                      " Mbps, Receiving rate " + std::to_string(info.mbpsRecvRate).substr(0, 5) + " Mbps";
        }
        return result;
    }

    return std::to_string(info.bitrateKbps) + " Kbps";
}

} // namespace BitrateSwitch
