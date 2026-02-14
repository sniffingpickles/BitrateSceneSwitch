#include "nginx.hpp"
#include "../switcher.hpp"

namespace {

std::string extractXmlValue(const std::string &xml, const std::string &tag)
{
    std::string openTag = "<" + tag + ">";
    std::string closeTag = "</" + tag + ">";
    
    size_t start = xml.find(openTag);
    if (start == std::string::npos) return "";
    
    start += openTag.length();
    size_t end = xml.find(closeTag, start);
    if (end == std::string::npos) return "";
    
    return xml.substr(start, end - start);
}

std::string findStreamByName(const std::string &xml, const std::string &name)
{
    size_t pos = 0;
    while (pos < xml.length()) {
        size_t streamStart = xml.find("<stream>", pos);
        if (streamStart == std::string::npos) break;
        
        size_t streamEnd = xml.find("</stream>", streamStart);
        if (streamEnd == std::string::npos) break;
        
        std::string streamBlock = xml.substr(streamStart, streamEnd - streamStart + 9);
        std::string streamName = extractXmlValue(streamBlock, "name");
        
        if (streamName == name) return streamBlock;
        
        pos = streamEnd + 9;
    }
    return "";
}

} // anonymous namespace

namespace BitrateSwitch {

NginxServer::NginxServer(const StreamServerConfig &config)
{
    statsUrl_ = config.statsUrl;
    publisher_ = config.key;
    name_ = config.name;
    
    size_t slashPos = publisher_.find('/');
    if (slashPos != std::string::npos) {
        application_ = publisher_.substr(0, slashPos);
        publisher_ = publisher_.substr(slashPos + 1);
    } else {
        application_ = "live";
    }
}

BitrateInfo NginxServer::fetchStats()
{
    BitrateInfo info;

    HttpResponse response = httpClient_.get(statsUrl_);
    if (!response.success) return info;

    std::string streamBlock = findStreamByName(response.body, publisher_);
    if (streamBlock.empty()) return info;

    std::string bwInStr = extractXmlValue(streamBlock, "bw_in");
    if (!bwInStr.empty()) {
        int64_t bwBytes = std::stoll(bwInStr);
        info.bitrateKbps = (bwBytes * 8) / 1000;
    }

    info.isOnline = info.bitrateKbps > 0;
    return info;
}

SwitchType NginxServer::checkSwitch(const Triggers &triggers)
{
    BitrateInfo info = fetchStats();
    return evaluateTriggers(info, triggers);
}

BitrateInfo NginxServer::getBitrate()
{
    BitrateInfo info = fetchStats();
    if (info.bitrateKbps > 0) {
        info.message = std::to_string(info.bitrateKbps) + " kbps";
    }
    return info;
}

} // namespace BitrateSwitch
