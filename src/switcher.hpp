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

    BitrateInfo getCurrentBitrate();
    std::string getStatusString();

private:
    void switcherThread();
    void doSwitchCheck();
    
    SwitchType getOnlineServerStatus();
    void switchToScene(const std::string &sceneName);
    std::string getSceneForType(SwitchType type);
    
    bool isSceneSwitchable(const std::string &scene);
    std::string getCurrentScene();

    Config *config_;
    std::vector<std::unique_ptr<StreamServer>> servers_;
    
    std::thread switcherThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> isStreaming_{false};
    std::mutex mutex_;

    SwitchType prevSwitchType_ = SwitchType::Offline;
    uint8_t sameTypeCount_ = 0;
    std::chrono::steady_clock::time_point sameTypeStart_;
    
    std::string currentScene_;
    std::string prevScene_;

    BitrateInfo lastBitrateInfo_;
};

} // namespace BitrateSwitch
