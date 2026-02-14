#include "switcher.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <algorithm>
#include <cstring>
#include <thread>

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
    streamStartTime_ = std::chrono::steady_clock::now();
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

    // Instant recover: force switch when coming back from offline
    bool forceSwitch = config_->instantRecover &&
                       prevSwitchType_ == SwitchType::Offline &&
                       currentSwitchType != SwitchType::Offline;

    // NOALBS: If server changed while we have a Previous type, force switch to Normal
    if (currentSwitchType == SwitchType::Previous && activeServer) {
        if (!lastUsedServerName_.empty() && lastUsedServerName_ != activeServer->getName()) {
            currentSwitchType = SwitchType::Normal;
            forceSwitch = true;
        }
    }

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

    // NOALBS: Avoid triggering offline timeout when stream just started
    // Grace period = retry_attempts + 5 seconds
    if (!config_->onlyWhenStreaming) {
        auto streamElapsed = std::chrono::steady_clock::now() - streamStartTime_;
        auto gracePeriod = std::chrono::seconds(config_->retryAttempts + 5);
        if (streamElapsed <= gracePeriod) {
            sameTypeStart_ = std::chrono::steady_clock::now();
        }
    }

    sameTypeCount_ = 0;

    // Handle offline timeout
    handleOfflineTimeout();

    // NOALBS: When offline, use last used server for override scenes
    StreamServer* serverForScenes = activeServer;
    if (currentSwitchType == SwitchType::Offline && !lastUsedServerName_.empty()) {
        // Find the last used server for its override scenes
        for (auto& server : servers_) {
            if (server->getName() == lastUsedServerName_) {
                serverForScenes = server.get();
                break;
            }
        }
    }

    std::string targetScene;
    if (currentSwitchType == SwitchType::Previous) {
        targetScene = prevScene_;
    } else {
        targetScene = getSceneForType(currentSwitchType, serverForScenes);
    }

    // NOALBS: Track previous scene and last used server
    if (currentSwitchType == SwitchType::Normal || 
        currentSwitchType == SwitchType::Low) {
        prevScene_ = targetScene;
    }
    
    if (currentSwitchType != SwitchType::Offline && activeServer) {
        lastUsedServerName_ = activeServer->getName();
    }

    // NOALBS: Skip switching to offline if on starting scene with auto-switch enabled
    std::string currentScene = getCurrentScene();
    if (!config_->optionalScenes.starting.empty() &&
        currentScene == config_->optionalScenes.starting &&
        config_->options.switchFromStartingToLive &&
        currentSwitchType == SwitchType::Offline) {
        // Don't switch to offline when on starting scene - wait for signal
        return;
    }

    switchToScene(targetScene);
    announceSceneChange(currentSwitchType);
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
        std::string previousScene = getCurrentScene();
        
        // Don't refresh if already on refresh scene
        if (previousScene == config_->optionalScenes.refresh) {
            blog(LOG_INFO, "[BitrateSceneSwitch] Refresh: already on refresh scene, skipping");
            return;
        }
        
        switchToScene(config_->optionalScenes.refresh);
        blog(LOG_INFO, "[BitrateSceneSwitch] Refresh: switching to refresh scene");
        
        // Wait 5 seconds then switch back (in a separate thread to not block)
        std::thread([this, previousScene]() {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            switchToScene(previousScene);
            blog(LOG_INFO, "[BitrateSceneSwitch] Refresh: returning to previous scene: %s", previousScene.c_str());
        }).detach();
    } else {
        // No refresh scene configured, fall back to fix
        fixMediaSources();
    }
}

void Switcher::fixMediaSources()
{
    // Get current scene and refresh all media sources with RTMP/SRT/UDP/RIST/RTSP inputs
    obs_source_t *currentSceneSource = obs_frontend_get_current_scene();
    if (!currentSceneSource) return;
    
    obs_scene_t *scene = obs_scene_from_source(currentSceneSource);
    if (!scene) {
        obs_source_release(currentSceneSource);
        return;
    }
    
    // Enumerate scene items and refresh media sources
    obs_scene_enum_items(scene, [](obs_scene_t*, obs_sceneitem_t *item, void*) -> bool {
        obs_source_t *source = obs_sceneitem_get_source(item);
        if (!source) return true;
        
        const char *sourceId = obs_source_get_id(source);
        if (!sourceId) return true;
        
        // Check if it's a media source (ffmpeg_source or vlc_source)
        if (strcmp(sourceId, "ffmpeg_source") == 0 || strcmp(sourceId, "vlc_source") == 0) {
            obs_data_t *settings = obs_source_get_settings(source);
            if (settings) {
                const char *input = obs_data_get_string(settings, "input");
                if (input) {
                    std::string inputStr = input;
                    std::transform(inputStr.begin(), inputStr.end(), inputStr.begin(), ::tolower);
                    
                    // Check if it's a streaming protocol
                    if (inputStr.find("rtmp") != std::string::npos ||
                        inputStr.find("srt") != std::string::npos ||
                        inputStr.find("udp") != std::string::npos ||
                        inputStr.find("rist") != std::string::npos ||
                        inputStr.find("rtsp") != std::string::npos) {
                        
                        // Refresh by updating settings (triggers reconnect)
                        obs_source_update(source, settings);
                        blog(LOG_INFO, "[BitrateSceneSwitch] Fix: refreshed media source: %s", 
                             obs_source_get_name(source));
                    }
                }
                obs_data_release(settings);
            }
        }
        return true;
    }, nullptr);
    
    obs_source_release(currentSceneSource);
    blog(LOG_INFO, "[BitrateSceneSwitch] Fix: refreshed media sources");
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
        fixMediaSources();
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
    case ChatCommand::Start:
        if (!isStreaming_) {
            obs_frontend_streaming_start();
            if (chatClient_) {
                chatClient_->sendMessage("Starting stream...");
            }
            blog(LOG_INFO, "[BitrateSceneSwitch] Stream started via chat command");
        } else {
            if (chatClient_) {
                chatClient_->sendMessage("Stream is already running");
            }
        }
        break;
    case ChatCommand::Stop:
        if (isStreaming_) {
            obs_frontend_streaming_stop();
            if (chatClient_) {
                chatClient_->sendMessage("Stopping stream...");
            }
            blog(LOG_INFO, "[BitrateSceneSwitch] Stream stopped via chat command");
        } else {
            if (chatClient_) {
                chatClient_->sendMessage("Stream is not running");
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
