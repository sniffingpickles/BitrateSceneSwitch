#include "switcher.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

namespace BitrateSwitch {

Switcher::Switcher(Config *config)
    : config_(config)
    , sameTypeStart_(std::chrono::steady_clock::now())
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
    blog(LOG_INFO, "[BitrateSceneSwitch] Streaming started");
}

void Switcher::onStreamingStopped()
{
    isStreaming_ = false;
    blog(LOG_INFO, "[BitrateSceneSwitch] Streaming stopped");
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
    SwitchType currentSwitchType = getOnlineServerStatus();

    bool forceSwitch = config_->instantRecover &&
                       prevSwitchType_ == SwitchType::Offline &&
                       currentSwitchType != SwitchType::Offline;

    if (prevSwitchType_ == currentSwitchType) {
        sameTypeCount_++;
    } else {
        prevSwitchType_ = currentSwitchType;
        sameTypeCount_ = 0;
        sameTypeStart_ = std::chrono::steady_clock::now();
    }

    if (sameTypeCount_ < config_->retryAttempts && !forceSwitch)
        return;

    sameTypeCount_ = 0;

    // Handle offline timeout
    if (currentSwitchType == SwitchType::Offline && 
        config_->offlineTimeoutMinutes > 0 &&
        isStreaming_) {
        
        auto elapsed = std::chrono::steady_clock::now() - sameTypeStart_;
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(elapsed).count();
        
        if (minutes >= config_->offlineTimeoutMinutes) {
            blog(LOG_INFO, "[BitrateSceneSwitch] Offline timeout reached, stopping stream");
            obs_frontend_streaming_stop();
            return;
        }
    }

    std::string targetScene;
    if (currentSwitchType == SwitchType::Previous) {
        targetScene = prevScene_;
    } else {
        targetScene = getSceneForType(currentSwitchType);
    }

    if (currentSwitchType == SwitchType::Normal || 
        currentSwitchType == SwitchType::Low) {
        prevScene_ = targetScene;
    }

    switchToScene(targetScene);
}

SwitchType Switcher::getOnlineServerStatus()
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &server : servers_) {
        SwitchType status = server->checkSwitch(config_->triggers);
        
        if (status != SwitchType::Offline) {
            lastBitrateInfo_ = server->getBitrate();
            return status;
        }
    }

    lastBitrateInfo_ = BitrateInfo();
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

std::string Switcher::getSceneForType(SwitchType type)
{
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
    return scene == config_->scenes.normal ||
           scene == config_->scenes.low ||
           scene == config_->scenes.offline;
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
    
    if (lastBitrateInfo_.isOnline)
        return "Online - " + lastBitrateInfo_.message;
    
    return "Offline";
}

} // namespace BitrateSwitch
