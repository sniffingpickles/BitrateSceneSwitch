#include "xiu.hpp"
#include "../switcher.hpp"
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

// Get the first object from a JSON array
std::string firstObjectInArray(const std::string &arrayJson)
{
    size_t objStart = arrayJson.find('{');
    if (objStart == std::string::npos) return "";

    int depth = 1;
    size_t endPos = objStart + 1;
    while (depth > 0 && endPos < arrayJson.length()) {
        if (arrayJson[endPos] == '{') depth++;
        else if (arrayJson[endPos] == '}') depth--;
        endPos++;
    }

    return arrayJson.substr(objStart, endPos - objStart);
}

// Escape a string for safe inclusion in a JSON string value
std::string escapeJsonString(const std::string &s)
{
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += c; break;
        }
    }
    return result;
}

} // anonymous namespace

namespace BitrateSwitch {

XiuServer::XiuServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    application_ = config.application;
    key_ = config.key;
    name_ = config.name;
    overrideScenes_ = config.overrideScenes;
}

BitrateInfo XiuServer::fetchStats()
{
    BitrateInfo info;
    info.serverName = name_;

    // Xiu uses POST with JSON body containing the identifier
    std::string postBody = "{\"identifier\":{\"rtmp\":{\"app_name\":\"" + escapeJsonString(application_) +
                           "\",\"stream_name\":\"" + escapeJsonString(key_) + "\"}}}";

    HttpResponse response = httpClient_.post(statsUrl_, postBody);
    if (!response.success) return info;

    // Response format: { "error_code": 0, "desp": "succ", "data": [ { "publisher": { ... }, "subscriber_count": N } ] }
    std::string errorCodeStr = extractJsonValue(response.body, "error_code");
    if (errorCodeStr.empty() || errorCodeStr != "0") return info;

    std::string dataArray = extractJsonArray(response.body, "data");
    if (dataArray.empty() || dataArray == "[]") return info;

    std::string firstItem = firstObjectInArray(dataArray);
    if (firstItem.empty()) return info;

    std::string publisherObj = extractNestedObject(firstItem, "publisher");
    if (publisherObj.empty()) return info;

    // Xiu uses "recv_bitrate(kbits/s)" as the field name
    std::string bitrateStr = extractJsonValue(publisherObj, "recv_bitrate(kbits/s)");
    if (!bitrateStr.empty()) {
        try {
            info.bitrateKbps = std::stoll(bitrateStr);
        } catch (...) {
            info.bitrateKbps = 0;
        }
    }

    info.isOnline = info.bitrateKbps > 0;
    return info;
}

SwitchType XiuServer::checkSwitch(const Triggers &triggers)
{
    BitrateInfo info = fetchStats();
    return evaluateTriggers(info, triggers);
}

BitrateInfo XiuServer::getBitrate()
{
    BitrateInfo info = fetchStats();
    if (info.bitrateKbps > 0) {
        info.message = std::to_string(info.bitrateKbps) + " kbps";
    }
    return info;
}

std::string XiuServer::getSourceInfo()
{
    BitrateInfo info = fetchStats();
    if (!info.isOnline) return "Offline";

    return std::to_string(info.bitrateKbps) + " Kbps";
}

} // namespace BitrateSwitch
