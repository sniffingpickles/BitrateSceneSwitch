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
#include "chat-client.hpp"
#include "kick-chat.hpp"
#include "twitch-pubsub.hpp"

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
    BitrateInfo getLastBitrateInfo() const;  
    std::string getStatusString();
    std::string getCurrentScene();
    bool isCurrentlyStreaming() const { return isStreaming_; }
    SwitchType getCurrentSwitchType() const { return prevSwitchType_; }

    // Manual scene switching commands
    void switchToLive();
    void switchToLow();
    void switchToBrb();
    void switchToPrivacy();
    void switchToStarting();
    void switchToEnding();
    void refreshScene();
    void triggerSwitch();
    void fixMediaSources();  // Refresh media sources (RTMP/SRT/etc)
    bool switchToSceneByName(const std::string& name);  // Case-insensitive scene switch
    
    // Chat integration
    void connectChat();
    void disconnectChat();
    bool isChatConnected() const;
    void requestChatReconnect() { chatReconnectRequested_ = true; }

private:
    void switcherThread();
    void doSwitchCheck();
    
    SwitchType getOnlineServerStatus(StreamServer** activeServer);
    SwitchType getOnlineServerStatusLocked(StreamServer** activeServer);
    void switchToScene(const std::string &sceneName);
    std::string getSceneForType(SwitchType type, StreamServer* server = nullptr);
    
    bool isSceneSwitchable(const std::string &scene);
    
    void handleStartingScene();
    void handleOfflineTimeout();
    void handleChatCommand(const ChatMessage& msg);
    void handleCustomCommands(const ChatMessage& msg);
    void handleRaidStop(const std::string &targetLogin, const std::string &displayName);
    void announceSceneChange(SwitchType type);
    void sendChatMessage(const std::string &text);
    std::string formatTemplate(const std::string &tmpl, const std::string &sceneOverride = "");

    Config *config_;
    std::unique_ptr<ChatClient> twitchChat_;
    std::unique_ptr<KickChatClient> kickChat_;
    std::unique_ptr<TwitchPubSubClient> twitchPubSub_;
    mutable std::mutex chatMutex_;
    std::vector<std::unique_ptr<StreamServer>> servers_;
    
    std::thread switcherThread_;
    std::thread refreshThread_;
    std::atomic<bool> refreshing_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> isStreaming_{false};
    std::atomic<bool> isRecording_{false};
    std::atomic<bool> chatReconnectRequested_{false};
    std::atomic<bool> manualOverride_{false};
    std::chrono::steady_clock::time_point chatNextReconnect_;
    int chatReconnectDelay_ = 0;
    mutable std::mutex mutex_;

    SwitchType prevSwitchType_ = SwitchType::Offline;
    uint8_t sameTypeCount_ = 0;
    std::chrono::steady_clock::time_point sameTypeStart_;
    std::chrono::steady_clock::time_point offlineStart_;
    std::chrono::steady_clock::time_point streamStartTime_;
    
    std::string currentScene_;
    std::string prevScene_;
    std::string lastUsedServerName_;  // Track which server was last used (for override scenes when offline)
    bool wasOnStartingScene_ = false;

    BitrateInfo lastBitrateInfo_;

    // RIST stale frame fix
    bool ristFixPending_ = false;
    bool ristFixFired_ = false;
    bool hasBeenOnline_ = false;
    std::chrono::steady_clock::time_point ristFixTriggerTime_;
    void handleRistStaleFrameFix();
};

} // namespace BitrateSwitch
