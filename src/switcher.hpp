#pragma once

#include <obs.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <string>
#include <chrono>

#include "config.hpp"
#include "stream-server.hpp"

namespace BitrateSwitch {

enum class SwitchType {
    Normal,
    Low,
    Offline,
    Previous
};

class Switcher {
public:
    explicit Switcher(Config *config);
    ~Switcher();

    void start();
    void stop();
    void reloadServers();

    void onStreamingStarted();
    void onStreamingStopped();
    void onSceneChanged();
    void onRecordingStarted();
    void onRecordingStopped();

    BitrateInfo getCurrentBitrate();
    std::string getStatusString();
    SwitchType getCurrentSwitchType() const { return prevSwitchType_; }

    // Manual scene switching commands
    void switchToLive();
    void switchToPrivacy();
    void switchToStarting();
    void switchToEnding();
    void refreshScene();

private:
    void switcherThread();
    void doSwitchCheck();
    
    SwitchType getOnlineServerStatus(StreamServer** activeServer);
    void switchToScene(const std::string &sceneName);
    std::string getSceneForType(SwitchType type, StreamServer* server = nullptr);
    
    bool isSceneSwitchable(const std::string &scene);
    std::string getCurrentScene();
    
    void handleStartingScene();
    void handleOfflineTimeout();

    Config *config_;
    std::vector<std::unique_ptr<StreamServer>> servers_;
    
    std::thread switcherThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> isStreaming_{false};
    std::atomic<bool> isRecording_{false};
    std::mutex mutex_;

    SwitchType prevSwitchType_ = SwitchType::Offline;
    uint8_t sameTypeCount_ = 0;
    std::chrono::steady_clock::time_point sameTypeStart_;
    std::chrono::steady_clock::time_point offlineStart_;
    
    std::string currentScene_;
    std::string prevScene_;
    std::string lastActiveServerName_;
    bool wasOnStartingScene_ = false;

    BitrateInfo lastBitrateInfo_;
};

} // namespace BitrateSwitch
