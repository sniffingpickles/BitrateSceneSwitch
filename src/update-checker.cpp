#include "update-checker.hpp"
#include <obs-module.h>
#include <curl/curl.h>
#include <sstream>
#include <regex>

namespace BitrateSwitch {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
    size_t totalSize = size * nmemb;
    userp->append((char*)contents, totalSize);
    return totalSize;
}

UpdateChecker::UpdateChecker()
{
}

UpdateChecker::~UpdateChecker()
{
    if (checkThread_.joinable()) {
        checkThread_.join();
    }
}

void UpdateChecker::checkForUpdates(UpdateCallback callback)
{
    if (checking_) return;
    
    checking_ = true;
    
    if (checkThread_.joinable()) {
        checkThread_.join();
    }
    
    checkThread_ = std::thread([this, callback]() {
        UpdateInfo info;
        info.currentVersion = CURRENT_VERSION;
        
        std::string response = fetchLatestRelease();
        
        if (!response.empty()) {
            // Parse JSON response for tag_name
            std::regex tagRegex("\"tag_name\"\\s*:\\s*\"([^\"]+)\"");
            std::regex urlRegex("\"html_url\"\\s*:\\s*\"([^\"]+)\"");
            std::regex bodyRegex("\"body\"\\s*:\\s*\"([^\"]+)\"");
            
            std::smatch match;
            if (std::regex_search(response, match, tagRegex)) {
                info.latestVersion = match[1].str();
                // Remove 'v' prefix if present
                if (!info.latestVersion.empty() && info.latestVersion[0] == 'v') {
                    info.latestVersion = info.latestVersion.substr(1);
                }
            }
            
            if (std::regex_search(response, match, urlRegex)) {
                info.downloadUrl = match[1].str();
            }
            
            if (std::regex_search(response, match, bodyRegex)) {
                info.releaseNotes = match[1].str();
            }
            
            info.hasUpdate = isNewerVersion(info.latestVersion, info.currentVersion);
        }
        
        checking_ = false;
        
        if (callback) {
            callback(info);
        }
    });
}

std::string UpdateChecker::fetchLatestRelease()
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        blog(LOG_WARNING, "[BitrateSceneSwitch] Failed to initialize CURL for update check");
        return "";
    }
    
    std::string response;
    
    curl_easy_setopt(curl, CURLOPT_URL, GITHUB_API_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "BitrateSceneSwitch-UpdateChecker");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        blog(LOG_WARNING, "[BitrateSceneSwitch] Update check failed: %s", curl_easy_strerror(res));
        response.clear();
    }
    
    curl_easy_cleanup(curl);
    return response;
}

bool UpdateChecker::parseVersion(const std::string& version, int& major, int& minor, int& patch)
{
    std::regex versionRegex("(\\d+)\\.(\\d+)\\.(\\d+)");
    std::smatch match;
    
    if (std::regex_search(version, match, versionRegex)) {
        major = std::stoi(match[1].str());
        minor = std::stoi(match[2].str());
        patch = std::stoi(match[3].str());
        return true;
    }
    return false;
}

bool UpdateChecker::isNewerVersion(const std::string& latest, const std::string& current)
{
    int latestMajor, latestMinor, latestPatch;
    int currentMajor, currentMinor, currentPatch;
    
    if (!parseVersion(latest, latestMajor, latestMinor, latestPatch)) return false;
    if (!parseVersion(current, currentMajor, currentMinor, currentPatch)) return false;
    
    if (latestMajor > currentMajor) return true;
    if (latestMajor < currentMajor) return false;
    
    if (latestMinor > currentMinor) return true;
    if (latestMinor < currentMinor) return false;
    
    return latestPatch > currentPatch;
}

} // namespace BitrateSwitch
