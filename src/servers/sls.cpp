#include "sls.hpp"
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

SlsServer::SlsServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    publisher_ = config.key;  // Stream key from UI maps to publisher in JSON
    name_ = config.name;
}

BitrateInfo SlsServer::fetchStats()
{
    BitrateInfo info;

    HttpResponse response = httpClient_.get(statsUrl_);
    if (!response.success) return info;

    std::string publishers = extractNestedObject(response.body, "publishers");
    if (publishers.empty()) return info;

    std::string publisherData = extractNestedObject(publishers, publisher_);
    if (publisherData.empty()) return info;

    std::string bitrateStr = extractJsonValue(publisherData, "bitrate");
    std::string estBitrateStr = extractJsonValue(publisherData, "est_bitrate");

    if (!estBitrateStr.empty())
        info.bitrateKbps = std::stoll(estBitrateStr);
    else if (!bitrateStr.empty())
        info.bitrateKbps = std::stoll(bitrateStr);

    info.isOnline = info.bitrateKbps > 0;
    return info;
}

SwitchType SlsServer::checkSwitch(const Triggers &triggers)
{
    BitrateInfo info = fetchStats();
    return evaluateTriggers(info, triggers);
}

BitrateInfo SlsServer::getBitrate()
{
    BitrateInfo info = fetchStats();
    if (info.bitrateKbps > 0) {
        info.message = std::to_string(info.bitrateKbps) + " kbps";
    }
    return info;
}

} // namespace BitrateSwitch
