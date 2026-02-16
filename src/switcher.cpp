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
    , streamStartTime_(std::chrono::steady_clock::now())
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

    // If the active server changed, force a Normal switch instead of Previous
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

    // Grace period: avoid false offline timeout right after stream start
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

    // When offline, use last-known server for its override scenes
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

    // Track previous scene and last used server for recovery
    if (currentSwitchType == SwitchType::Normal || 
        currentSwitchType == SwitchType::Low) {
        prevScene_ = targetScene;
    }
    
    if (currentSwitchType != SwitchType::Offline && activeServer) {
        lastUsedServerName_ = activeServer->getName();
    }

    // Don't switch to offline while on starting scene (wait for feed)
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
        
        if (previousScene == config_->optionalScenes.refresh) {
            blog(LOG_INFO, "[BitrateSceneSwitch] Refresh: already on refresh scene, skipping");
            return;
        }
        
        switchToScene(config_->optionalScenes.refresh);
        blog(LOG_INFO, "[BitrateSceneSwitch] Refresh: switching to refresh scene");
        
        // Capture running_ flag by reference to check if switcher is still alive.
        // The thread will bail out early if the switcher is shutting down.
        std::atomic<bool> &runningRef = running_;
        std::thread([this, previousScene, &runningRef]() {
            for (int i = 0; i < 50 && runningRef; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (runningRef) {
                switchToScene(previousScene);
                blog(LOG_INFO, "[BitrateSceneSwitch] Refresh: returned to scene: %s",
                     previousScene.c_str());
            }
        }).detach();
    } else {
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
                if (input && input[0] != '\0') {
                    std::string inputStr = input;
                    std::transform(inputStr.begin(), inputStr.end(), inputStr.begin(), ::tolower);
                    
                    // Match streaming protocol prefixes (consistent with NOALBS)
                    bool isStreamSource =
                        inputStr.rfind("rtmp", 0) == 0 ||
                        inputStr.rfind("srt", 0) == 0 ||
                        inputStr.rfind("udp", 0) == 0 ||
                        inputStr.rfind("rist", 0) == 0 ||
                        inputStr.rfind("rtsp", 0) == 0;
                    
                    if (isStreamSource) {
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

    auto reply = [this](const std::string &text) {
        if (chatClient_ && chatClient_->isConnected())
            chatClient_->sendMessage(text);
    };
    auto announce = [this, &reply](const std::string &text) {
        if (config_->chat.announceSceneChanges)
            reply(text);
    };

    switch (msg.command) {
    case ChatCommand::Live:
        switchToLive();
        announce(formatTemplate(config_->messages.sceneSwitched, config_->scenes.normal));
        break;
    case ChatCommand::Low:
        switchToLow();
        announce(formatTemplate(config_->messages.sceneSwitched, config_->scenes.low));
        break;
    case ChatCommand::Brb:
        switchToBrb();
        announce(formatTemplate(config_->messages.sceneSwitched, config_->scenes.offline));
        break;
    case ChatCommand::Refresh:
        refreshScene();
        announce(formatTemplate(config_->messages.refreshing));
        break;
    case ChatCommand::Status:
        if (lastBitrateInfo_.isOnline)
            reply(formatTemplate(config_->messages.statusResponse));
        else
            reply(formatTemplate(config_->messages.statusOffline));
        break;
    case ChatCommand::Trigger:
        triggerSwitch();
        announce("Triggered switch check");
        break;
    case ChatCommand::Fix:
        fixMediaSources();
        announce(formatTemplate(config_->messages.fixAttempt));
        break;
    case ChatCommand::SwitchScene:
        if (msg.args.empty()) {
            reply("Usage: !ss <scene_name>");
        } else if (switchToSceneByName(msg.args)) {
            announce(formatTemplate(config_->messages.sceneSwitched, msg.args));
        } else {
            reply("Scene not found: " + msg.args);
        }
        break;
    case ChatCommand::Start:
        if (isStreaming_) {
            reply("Stream is already running");
        } else {
            obs_frontend_streaming_start();
            reply(formatTemplate(config_->messages.streamStarted));
            blog(LOG_INFO, "[BitrateSceneSwitch] Stream started via chat");
        }
        break;
    case ChatCommand::Stop:
        if (!isStreaming_) {
            reply("Stream is not running");
        } else {
            obs_frontend_streaming_stop();
            reply(formatTemplate(config_->messages.streamStopped));
            blog(LOG_INFO, "[BitrateSceneSwitch] Stream stopped via chat");
        }
        break;
    case ChatCommand::None:
        // Check custom commands for unrecognized messages
        handleCustomCommands(msg);
        break;
    default:
        break;
    }
}

void Switcher::announceSceneChange(SwitchType type)
{
    if (!config_->chat.announceSceneChanges || !chatClient_ || !chatClient_->isConnected())
        return;
    
    std::string tmpl;
    switch (type) {
    case SwitchType::Normal:
        tmpl = config_->messages.switchedToLive;
        break;
    case SwitchType::Low:
        tmpl = config_->messages.switchedToLow;
        break;
    case SwitchType::Offline:
        tmpl = config_->messages.switchedToOffline;
        break;
    default:
        return;
    }
    
    chatClient_->sendMessage(formatTemplate(tmpl));
}

std::string Switcher::formatTemplate(const std::string &tmpl, const std::string &sceneOverride)
{
    std::string result = tmpl;
    
    auto replaceAll = [](std::string &str, const std::string &from, const std::string &to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    };
    
    // Build placeholder values from current state
    BitrateInfo info = lastBitrateInfo_;
    std::string scene = sceneOverride.empty() ? getCurrentScene() : sceneOverride;
    
    replaceAll(result, "{bitrate}", std::to_string(info.bitrateKbps));
    replaceAll(result, "{rtt}", std::to_string(static_cast<int>(info.rttMs)));
    replaceAll(result, "{scene}", scene);
    replaceAll(result, "{prev_scene}", prevScene_);
    replaceAll(result, "{server}", info.serverName);
    replaceAll(result, "{status}", info.isOnline ? "Online" : "Offline");
    replaceAll(result, "{uptime}", isStreaming_ ? "Live" : "Not streaming");
    
    return result;
}

void Switcher::handleCustomCommands(const ChatMessage& msg)
{
    if (config_->customCommands.empty()) return;
    
    std::string msgLower = msg.message;
    std::transform(msgLower.begin(), msgLower.end(), msgLower.begin(), ::tolower);
    
    for (const auto &cmd : config_->customCommands) {
        if (!cmd.enabled || cmd.trigger.empty()) continue;
        
        std::string triggerLower = cmd.trigger;
        std::transform(triggerLower.begin(), triggerLower.end(), triggerLower.begin(), ::tolower);
        
        // Match exact command or command with trailing space (for args)
        if (msgLower == triggerLower || msgLower.rfind(triggerLower + " ", 0) == 0) {
            if (chatClient_ && chatClient_->isConnected()) {
                chatClient_->sendMessage(formatTemplate(cmd.response));
            }
            return;
        }
    }
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
        return formatTemplate(config_->messages.statusResponse);
    }
    
    return formatTemplate(config_->messages.statusOffline);
}

} // namespace BitrateSwitch
