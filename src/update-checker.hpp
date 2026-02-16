#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>

namespace BitrateSwitch {

struct UpdateInfo {
    bool hasUpdate = false;
    std::string latestVersion;
    std::string currentVersion;
    std::string downloadUrl;
    std::string releaseNotes;
};

class UpdateChecker {
public:
    using UpdateCallback = std::function<void(const UpdateInfo&)>;

    UpdateChecker();
    ~UpdateChecker();

    void checkForUpdates(UpdateCallback callback);
    void setCurrentVersion(const std::string& version);
    
    static std::string getCurrentVersion() { return CURRENT_VERSION; }
    
private:
    std::string fetchLatestRelease();
    bool parseVersion(const std::string& version, int& major, int& minor, int& patch);
    bool isNewerVersion(const std::string& latest, const std::string& current);
    
    static constexpr const char* CURRENT_VERSION = "1.0.3";
    static constexpr const char* GITHUB_API_URL = "https://api.github.com/repos/sniffingpickles/BitrateSceneSwitch/releases/latest";
    
    std::thread checkThread_;
    std::atomic<bool> checking_{false};
};

} // namespace BitrateSwitch
