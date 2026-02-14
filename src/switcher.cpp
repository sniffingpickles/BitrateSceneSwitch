#include "switcher.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

namespace BitrateSwitch {

Switcher::Switcher(Config *config)
    : config_(config)
    , sameTypeStart_(std::chrono::steady_clock::now())
    , offlineStart_(std::chrono::steady_clock::now())
{
    prevScene_ = config_->scenes.normal;
    reloadServers();
}

Switcher::~Switcher()
{
    stop();
}

void Switcher::reloadServers()
{
    std::lock_guard<std::mutex> lock(mutex_);
    servers_.clear();
    
    for (const auto &serverConfig : config_->servers) {
        if (serverConfig.enabled) {
            servers_.push_back(StreamServer::create(serverConfig));
        }
    }
    
    blog(LOG_INFO, "[BitrateSceneSwitch] Loaded %zu servers", servers_.size());
}

void Switcher::start()
{
    if (running_)
        return;

    running_ = true;
    switcherThread_ = std::thread(&Switcher::switcherThread, this);
    blog(LOG_INFO, "[BitrateSceneSwitch] Switcher started");
}

void Switcher::stop()
{
    running_ = false;
    if (switcherThread_.joinable()) {
        switcherThread_.join();
    }
    blog(LOG_INFO, "[BitrateSceneSwitch] Switcher stopped");
}

void Switcher::onStreamingStarted()
{
    isStreaming_ = true;
    sameTypeStart_ = std::chrono::steady_clock::now();
    offlineStart_ = std::chrono::steady_clock::now();
    blog(LOG_INFO, "[BitrateSceneSwitch] Streaming started");

    // Handle optional: switch to starting scene on stream start
    if (config_->options.switchToStartingOnStreamStart && 
        !config_->optionalScenes.starting.empty()) {
        switchToScene(config_->optionalScenes.starting);
        wasOnStartingScene_ = true;
    }

    // Handle optional: auto-record while streaming
    if (config_->options.recordWhileStreaming && !isRecording_) {
        obs_frontend_recording_start();
    }
}

void Switcher::onStreamingStopped()
{
    isStreaming_ = false;
    wasOnStartingScene_ = false;
    blog(LOG_INFO, "[BitrateSceneSwitch] Streaming stopped");

    // Stop recording if we started it
    if (config_->options.recordWhileStreaming && isRecording_) {
        obs_frontend_recording_stop();
    }

    // Switch to ending scene if configured
    if (!config_->optionalScenes.ending.empty()) {
        switchToScene(config_->optionalScenes.ending);
    }
}

void Switcher::onSceneChanged()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    obs_source_t *sceneSource = obs_frontend_get_current_scene();
    if (sceneSource) {
        const char *name = obs_source_get_name(sceneSource);
        if (name) {
            currentScene_ = name;
        }
        obs_source_release(sceneSource);
    }
}

void Switcher::onRecordingStarted()
{
    isRecording_ = true;
    blog(LOG_INFO, "[BitrateSceneSwitch] Recording started");
}

void Switcher::onRecordingStopped()
{
    isRecording_ = false;
    blog(LOG_INFO, "[BitrateSceneSwitch] Recording stopped");
}

void Switcher::switcherThread()
{
    blog(LOG_INFO, "[BitrateSceneSwitch] Switcher thread running");

    while (running_) {
        os_sleep_ms(1000);

        if (!running_)
            break;

        if (!config_->enabled)
            continue;

        if (config_->onlyWhenStreaming && !isStreaming_)
            continue;

        std::string current = getCurrentScene();
        if (!isSceneSwitchable(current))
            continue;

        doSwitchCheck();
    }
}

void Switcher::doSwitchCheck()
{
    StreamServer* activeServer = nullptr;
    SwitchType currentSwitchType = getOnlineServerStatus(&activeServer);

    // Handle starting scene auto-switch
    if (wasOnStartingScene_ && config_->options.switchFromStartingToLive) {
        if (currentSwitchType == SwitchType::Normal || currentSwitchType == SwitchType::Low) {
            wasOnStartingScene_ = false;
            // Will switch to appropriate scene below
        }
    }

    bool forceSwitch = config_->instantRecover &&
                       prevSwitchType_ == SwitchType::Offline &&
                       currentSwitchType != SwitchType::Offline;

    if (prevSwitchType_ == currentSwitchType) {
        sameTypeCount_++;
    } else {
        prevSwitchType_ = currentSwitchType;
        sameTypeCount_ = 0;
        sameTypeStart_ = std::chrono::steady_clock::now();
        
        // Track offline start time
        if (currentSwitchType == SwitchType::Offline) {
            offlineStart_ = std::chrono::steady_clock::now();
        }
    }

    if (sameTypeCount_ < config_->retryAttempts && !forceSwitch)
        return;

    sameTypeCount_ = 0;

    // Handle offline timeout
    handleOfflineTimeout();

    std::string targetScene;
    if (currentSwitchType == SwitchType::Previous) {
        targetScene = prevScene_;
    } else {
        targetScene = getSceneForType(currentSwitchType, activeServer);
    }

    if (currentSwitchType == SwitchType::Normal || 
        currentSwitchType == SwitchType::Low) {
        prevScene_ = targetScene;
    }

    switchToScene(targetScene);
}

void Switcher::handleOfflineTimeout()
{
    if (prevSwitchType_ != SwitchType::Offline)
        return;
    
    if (config_->options.offlineTimeoutMinutes == 0)
        return;
    
    if (!isStreaming_)
        return;

    auto elapsed = std::chrono::steady_clock::now() - offlineStart_;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(elapsed).count();
    
    if (minutes >= static_cast<long>(config_->options.offlineTimeoutMinutes)) {
        blog(LOG_INFO, "[BitrateSceneSwitch] Offline timeout reached (%d min), stopping stream", 
             config_->options.offlineTimeoutMinutes);
        obs_frontend_streaming_stop();
    }
}

void Switcher::handleStartingScene()
{
    // Called when we detect stream is online and we were on starting scene
    if (wasOnStartingScene_ && config_->options.switchFromStartingToLive) {
        wasOnStartingScene_ = false;
        switchToScene(config_->scenes.normal);
    }
}

SwitchType Switcher::getOnlineServerStatus(StreamServer** activeServer)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &server : servers_) {
        SwitchType status = server->checkSwitch(config_->triggers);
        
        if (status != SwitchType::Offline) {
            lastBitrateInfo_ = server->getBitrate();
            lastBitrateInfo_.serverName = server->getName();
            lastActiveServerName_ = server->getName();
            if (activeServer) *activeServer = server.get();
            return status;
        }
    }

    lastBitrateInfo_ = BitrateInfo();
    if (activeServer) *activeServer = nullptr;
    return SwitchType::Offline;
}

void Switcher::switchToScene(const std::string &sceneName)
{
    std::string current = getCurrentScene();
    
    if (current == sceneName)
        return;

    obs_source_t *sceneSource = obs_get_source_by_name(sceneName.c_str());
    if (sceneSource) {
        obs_frontend_set_current_scene(sceneSource);
        obs_source_release(sceneSource);
        blog(LOG_INFO, "[BitrateSceneSwitch] Switched to scene: %s", sceneName.c_str());
    } else {
        blog(LOG_WARNING, "[BitrateSceneSwitch] Scene not found: %s", sceneName.c_str());
    }
}

std::string Switcher::getSceneForType(SwitchType type, StreamServer* server)
{
    // Check for server override scenes
    if (server && server->hasOverrideScenes()) {
        const OverrideScenes& override = server->getOverrideScenes();
        switch (type) {
        case SwitchType::Normal:
            if (!override.normal.empty()) return override.normal;
            break;
        case SwitchType::Low:
            if (!override.low.empty()) return override.low;
            break;
        case SwitchType::Offline:
            if (!override.offline.empty()) return override.offline;
            break;
        default:
            break;
        }
    }

    // Default scenes
    switch (type) {
    case SwitchType::Normal:
        return config_->scenes.normal;
    case SwitchType::Low:
        return config_->scenes.low;
    case SwitchType::Offline:
        return config_->scenes.offline;
    default:
        return config_->scenes.normal;
    }
}

bool Switcher::isSceneSwitchable(const std::string &scene)
{
    // Check main switching scenes
    if (scene == config_->scenes.normal ||
        scene == config_->scenes.low ||
        scene == config_->scenes.offline) {
        return true;
    }
    
    // Check if on starting scene and auto-switch is enabled
    if (wasOnStartingScene_ && scene == config_->optionalScenes.starting) {
        return config_->options.switchFromStartingToLive;
    }
    
    return false;
}

std::string Switcher::getCurrentScene()
{
    obs_source_t *sceneSource = obs_frontend_get_current_scene();
    std::string name;
    
    if (sceneSource) {
        const char *sceneName = obs_source_get_name(sceneSource);
        if (sceneName) {
            name = sceneName;
        }
        obs_source_release(sceneSource);
    }
    
    return name;
}

// Manual scene switching methods
void Switcher::switchToLive()
{
    switchToScene(config_->scenes.normal);
    blog(LOG_INFO, "[BitrateSceneSwitch] Manual switch to Live scene");
}

void Switcher::switchToPrivacy()
{
    if (!config_->optionalScenes.privacy.empty()) {
        switchToScene(config_->optionalScenes.privacy);
        blog(LOG_INFO, "[BitrateSceneSwitch] Manual switch to Privacy scene");
    }
}

void Switcher::switchToStarting()
{
    if (!config_->optionalScenes.starting.empty()) {
        switchToScene(config_->optionalScenes.starting);
        wasOnStartingScene_ = true;
        blog(LOG_INFO, "[BitrateSceneSwitch] Manual switch to Starting scene");
    }
}

void Switcher::switchToEnding()
{
    if (!config_->optionalScenes.ending.empty()) {
        switchToScene(config_->optionalScenes.ending);
        blog(LOG_INFO, "[BitrateSceneSwitch] Manual switch to Ending scene");
    }
}

void Switcher::refreshScene()
{
    if (!config_->optionalScenes.refresh.empty()) {
        std::string current = getCurrentScene();
        switchToScene(config_->optionalScenes.refresh);
        blog(LOG_INFO, "[BitrateSceneSwitch] Refresh: switching to refresh scene");
    }
}

void Switcher::switchToLow()
{
    switchToScene(config_->scenes.low);
    blog(LOG_INFO, "[BitrateSceneSwitch] Manual switch to Low scene");
}

void Switcher::switchToBrb()
{
    switchToScene(config_->scenes.offline);
    blog(LOG_INFO, "[BitrateSceneSwitch] Manual switch to BRB/Offline scene");
}

void Switcher::triggerSwitch()
{
    doSwitchCheck();
    blog(LOG_INFO, "[BitrateSceneSwitch] Manual trigger of switch check");
}

bool Switcher::switchToSceneByName(const std::string& name)
{
    // Get all scenes and find one matching case-insensitively
    obs_frontend_source_list scenes = {};
    obs_frontend_get_scenes(&scenes);
    
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    
    std::string foundScene;
    for (size_t i = 0; i < scenes.sources.num; i++) {
        obs_source_t *source = scenes.sources.array[i];
        const char *sceneName = obs_source_get_name(source);
        if (sceneName) {
            std::string sceneNameLower = sceneName;
            std::transform(sceneNameLower.begin(), sceneNameLower.end(), sceneNameLower.begin(), ::tolower);
            if (sceneNameLower == nameLower) {
                foundScene = sceneName;
                break;
            }
        }
    }
    
    obs_frontend_source_list_free(&scenes);
    
    if (!foundScene.empty()) {
        switchToScene(foundScene);
        blog(LOG_INFO, "[BitrateSceneSwitch] Manual switch to scene: %s", foundScene.c_str());
        return true;
    }
    
    blog(LOG_WARNING, "[BitrateSceneSwitch] Scene not found: %s", name.c_str());
    return false;
}

void Switcher::connectChat()
{
    if (!config_->chat.enabled) return;
    
    if (!chatClient_) {
        chatClient_ = std::make_unique<ChatClient>();
        chatClient_->setCommandCallback([this](const ChatMessage& msg) {
            handleChatCommand(msg);
        });
    }
    
    chatClient_->setConfig(config_->chat);
    if (chatClient_->connect()) {
        blog(LOG_INFO, "[BitrateSceneSwitch] Chat connected");
    }
}

void Switcher::disconnectChat()
{
    if (chatClient_) {
        chatClient_->disconnect();
        blog(LOG_INFO, "[BitrateSceneSwitch] Chat disconnected");
    }
}

bool Switcher::isChatConnected() const
{
    return chatClient_ && chatClient_->isConnected();
}

void Switcher::handleChatCommand(const ChatMessage& msg)
{
    blog(LOG_INFO, "[BitrateSceneSwitch] Chat command from %s: %s", 
         msg.username.c_str(), msg.message.c_str());
    
    switch (msg.command) {
    case ChatCommand::Live:
        switchToLive();
        if (config_->chat.announceSceneChanges && chatClient_) {
            chatClient_->sendMessage("Switched to Live scene");
        }
        break;
    case ChatCommand::Low:
        switchToLow();
        if (config_->chat.announceSceneChanges && chatClient_) {
            chatClient_->sendMessage("Switched to Low scene");
        }
        break;
    case ChatCommand::Brb:
        switchToBrb();
        if (config_->chat.announceSceneChanges && chatClient_) {
            chatClient_->sendMessage("Switched to BRB scene");
        }
        break;
    case ChatCommand::Refresh:
        refreshScene();
        if (config_->chat.announceSceneChanges && chatClient_) {
            chatClient_->sendMessage("Refreshing scene...");
        }
        break;
    case ChatCommand::Status:
        if (chatClient_) {
            chatClient_->sendMessage(getStatusString());
        }
        break;
    case ChatCommand::Trigger:
        triggerSwitch();
        if (config_->chat.announceSceneChanges && chatClient_) {
            chatClient_->sendMessage("Triggered switch check");
        }
        break;
    case ChatCommand::Fix:
        refreshScene();
        if (config_->chat.announceSceneChanges && chatClient_) {
            chatClient_->sendMessage("Attempting to fix stream...");
        }
        break;
    case ChatCommand::SwitchScene:
        if (!msg.args.empty()) {
            if (switchToSceneByName(msg.args)) {
                if (config_->chat.announceSceneChanges && chatClient_) {
                    chatClient_->sendMessage("Switched to scene: " + msg.args);
                }
            } else {
                if (chatClient_) {
                    chatClient_->sendMessage("Scene not found: " + msg.args);
                }
            }
        } else {
            if (chatClient_) {
                chatClient_->sendMessage("Usage: !ss <scene_name>");
            }
        }
        break;
    default:
        break;
    }
}

void Switcher::announceSceneChange(SwitchType type)
{
    if (!config_->chat.announceSceneChanges || !chatClient_ || !chatClient_->isConnected())
        return;
    
    std::string msg;
    switch (type) {
    case SwitchType::Normal:
        msg = "Switched to Live scene (bitrate recovered)";
        break;
    case SwitchType::Low:
        msg = "Switched to Low scene (low bitrate detected)";
        break;
    case SwitchType::Offline:
        msg = "Switched to Offline scene";
        break;
    default:
        return;
    }
    
    chatClient_->sendMessage(msg);
}

BitrateInfo Switcher::getCurrentBitrate()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return lastBitrateInfo_;
}

std::string Switcher::getStatusString()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!config_->enabled)
        return "Disabled";
    
    if (config_->onlyWhenStreaming && !isStreaming_)
        return "Waiting for stream";
    
    if (servers_.empty())
        return "No servers configured";
    
    if (lastBitrateInfo_.isOnline) {
        std::string status = "Online";
        if (!lastBitrateInfo_.serverName.empty()) {
            status += " (" + lastBitrateInfo_.serverName + ")";
        }
        status += " - " + lastBitrateInfo_.message;
        return status;
    }
    
    return "Offline";
}

} // namespace BitrateSwitch
