#include "chat-client.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <algorithm>
#include <cstring>
#include <utility>

namespace BitrateSwitch {

struct RoomIdPack {
    std::function<void(const std::string &)> fn;
    std::string id;
};

// winsock init is handled by curl_global_init in plugin-main,
// no need to mess with WSAStartup/WSACleanup here

ChatClient::ChatClient() = default;

ChatClient::~ChatClient()
{
    disconnect();
}

void ChatClient::setConfig(const ChatConfig& config)
{
    config_ = config;
}

void ChatClient::setCommandCallback(CommandCallback callback)
{
    callback_ = callback;
}

void ChatClient::setRoomIdCallback(std::function<void(const std::string &)> cb)
{
    roomIdCallback_ = std::move(cb);
    roomIdSent_ = false;
}

bool ChatClient::connect()
{
    if (connected_) return true;
    if (config_.channel.empty() || config_.oauthToken.empty()) {
        blog(LOG_WARNING, "[BitrateSceneSwitch] Chat: Missing channel or OAuth token");
        return false;
    }
    
    const char* host = TWITCH_IRC_HOST;
    int port = TWITCH_IRC_PORT;
    
    struct addrinfo hints{}, *result;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host, std::to_string(port).c_str(), &hints, &result) != 0) {
        blog(LOG_ERROR, "[BitrateSceneSwitch] Chat: Failed to resolve host %s", host);
        return false;
    }
    
    socket_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (socket_ == INVALID_SOCKET) {
        freeaddrinfo(result);
        blog(LOG_ERROR, "[BitrateSceneSwitch] Chat: Failed to create socket");
        return false;
    }
    
    if (::connect(socket_, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        blog(LOG_ERROR, "[BitrateSceneSwitch] Chat: Failed to connect to %s:%d", host, port);
        return false;
    }
    
    freeaddrinfo(result);

    // Set recv timeout so the receive thread can check running_ periodically
#ifdef _WIN32
    DWORD rcvTimeout = 5000;
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (const char *)&rcvTimeout, sizeof(rcvTimeout));
#else
    struct timeval rcvTimeout = {5, 0};
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &rcvTimeout, sizeof(rcvTimeout));
#endif

    std::string username = config_.botUsername.empty() ? config_.channel : config_.botUsername;
    std::transform(username.begin(), username.end(), username.begin(), ::tolower);
    
    sendRaw("PASS " + config_.oauthToken + "\r\n");
    sendRaw("NICK " + username + "\r\n");
    sendRaw("JOIN #" + config_.channel + "\r\n");
    sendRaw("CAP REQ :twitch.tv/commands twitch.tv/tags\r\n");
    
    connected_ = true;
    running_ = true;
    receiveThread_ = std::thread(&ChatClient::receiveLoop, this);
    
    blog(LOG_INFO, "[BitrateSceneSwitch] Chat: Connected to Twitch channel #%s", config_.channel.c_str());
    
    return true;
}

void ChatClient::disconnect()
{
    running_ = false;
    connected_ = false;
    
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
    
    blog(LOG_INFO, "[BitrateSceneSwitch] Chat: Disconnected");
}

bool ChatClient::isConnected() const
{
    return connected_;
}

void ChatClient::sendMessage(const std::string& message)
{
    if (!connected_ || config_.channel.empty()) return;
    sendRaw("PRIVMSG #" + config_.channel + " :" + message + "\r\n");
}

void ChatClient::receiveLoop()
{
    char buffer[4096];
    std::string pending;
    
    while (running_) {
        int received = recv(socket_, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            if (!running_)
                break;
#ifdef _WIN32
            if (WSAGetLastError() == WSAETIMEDOUT)
                continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
#endif
            blog(LOG_WARNING, "[BitrateSceneSwitch] Chat: Connection lost");
            connected_ = false;
            break;
        }
        
        buffer[received] = '\0';
        pending += buffer;
        
        size_t pos;
        while ((pos = pending.find("\r\n")) != std::string::npos) {
            std::string line = pending.substr(0, pos);
            pending = pending.substr(pos + 2);
            handleMessage(line);
        }
    }
}

void ChatClient::extractRoomIdFromTags(const std::string &raw)
{
    if (roomIdSent_ || raw.empty() || raw[0] != '@')
        return;
    size_t sp = raw.find(' ');
    if (sp == std::string::npos)
        return;
    std::string tags = raw.substr(1, sp - 1);
    size_t pos = 0;
    while (pos < tags.size()) {
        size_t eq = tags.find('=', pos);
        if (eq == std::string::npos)
            break;
        size_t semi = tags.find(';', eq);
        std::string key = tags.substr(pos, eq - pos);
        std::string val = (semi == std::string::npos) ? tags.substr(eq + 1)
                                                      : tags.substr(eq + 1, semi - eq - 1);
        if (key == "room-id") {
            roomIdSent_ = true;
            if (roomIdCallback_) {
                auto *p = new RoomIdPack{roomIdCallback_, val};
                obs_queue_task(
                    OBS_TASK_UI,
                    [](void *vp) {
                        auto *pack = static_cast<RoomIdPack *>(vp);
                        pack->fn(pack->id);
                        delete pack;
                    },
                    p,
                    false);
            }
            return;
        }
        pos = (semi == std::string::npos) ? tags.size() : semi + 1;
    }
}

void ChatClient::handleMessage(const std::string& raw)
{
    extractRoomIdFromTags(raw);

    if (raw.substr(0, 4) == "PING") {
        sendRaw("PONG" + raw.substr(4) + "\r\n");
        return;
    }
    
    if (raw.find("PRIVMSG") != std::string::npos) {
        ChatMessage msg = parseMessage(raw);
        if (!isAdmin(msg.username))
            return;
        
        // Pass all messages (including None) so custom commands can be checked
        if (msg.message.empty() || msg.message[0] != '!')
            return;
        
        if (callback_) {
            callback_(msg);
        }
    }
}

ChatMessage ChatClient::parseMessage(const std::string& raw)
{
    ChatMessage msg;
    
    size_t exclaim = raw.find('!');
    if (exclaim != std::string::npos) {
        size_t colon = raw.rfind(':', exclaim);
        if (colon != std::string::npos && colon > 0) {
            msg.username = raw.substr(colon + 1, exclaim - colon - 1);
        }
    }
    
    size_t msgStart = raw.find(" :", raw.find("PRIVMSG"));
    if (msgStart != std::string::npos) {
        msg.message = raw.substr(msgStart + 2);
    }
    
    std::transform(msg.username.begin(), msg.username.end(), msg.username.begin(), ::tolower);
    
    msg.command = parseCommand(msg.message, msg.args);
    
    return msg;
}

ChatCommand ChatClient::parseCommand(const std::string& message, std::string& args)
{
    return parseCommandForConfig(config_, message, args);
}

ChatCommand ChatClient::parseCommandForConfig(const ChatConfig &cfg, const std::string &message,
                                              std::string &args)
{
    std::string lower = message;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    auto checkCmd = [&](const std::string& cmd, ChatCommand type) -> bool {
        std::string cmdLower = cmd;
        std::transform(cmdLower.begin(), cmdLower.end(), cmdLower.begin(), ::tolower);
        if (lower == cmdLower || lower.substr(0, cmdLower.length() + 1) == cmdLower + " ") {
            if (lower.length() > cmdLower.length() + 1) {
                args = message.substr(cmdLower.length() + 1);
            }
            return true;
        }
        return false;
    };
    
    if (checkCmd(cfg.cmdLive, ChatCommand::Live)) return ChatCommand::Live;
    if (checkCmd(cfg.cmdLow, ChatCommand::Low)) return ChatCommand::Low;
    if (checkCmd(cfg.cmdBrb, ChatCommand::Brb)) return ChatCommand::Brb;
    if (checkCmd(cfg.cmdPrivacy, ChatCommand::Privacy)) return ChatCommand::Privacy;
    if (checkCmd(cfg.cmdRefresh, ChatCommand::Refresh)) return ChatCommand::Refresh;
    if (checkCmd(cfg.cmdStatus, ChatCommand::Status)) return ChatCommand::Status;
    if (checkCmd(cfg.cmdTrigger, ChatCommand::Trigger)) return ChatCommand::Trigger;
    if (checkCmd(cfg.cmdFix, ChatCommand::Fix)) return ChatCommand::Fix;
    if (checkCmd(cfg.cmdSwitchScene, ChatCommand::SwitchScene)) return ChatCommand::SwitchScene;
    if (cfg.cmdSwitchScene == "!s") {
        if (checkCmd("!ss", ChatCommand::SwitchScene)) return ChatCommand::SwitchScene;
    } else {
        if (checkCmd("!s", ChatCommand::SwitchScene)) return ChatCommand::SwitchScene;
    }
    if (checkCmd(cfg.cmdStart, ChatCommand::Start)) return ChatCommand::Start;
    if (checkCmd(cfg.cmdStop, ChatCommand::Stop)) return ChatCommand::Stop;
    
    return ChatCommand::None;
}

bool ChatClient::isAdmin(const std::string& username)
{
    std::string lower = username;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Channel owner always has access
    std::string channelLower = config_.channel;
    std::transform(channelLower.begin(), channelLower.end(), channelLower.begin(), ::tolower);
    if (lower == channelLower) return true;
    
    // If no admins configured, only channel owner has access
    if (config_.admins.empty()) return false;
    
    // Check explicit admin list
    for (const auto& admin : config_.admins) {
        std::string adminLower = admin;
        std::transform(adminLower.begin(), adminLower.end(), adminLower.begin(), ::tolower);
        if (lower == adminLower) return true;
    }
    
    return false;
}

void ChatClient::sendRaw(const std::string& data)
{
    std::lock_guard<std::mutex> lock(sendMutex_);
    if (socket_ != INVALID_SOCKET) {
        send(socket_, data.c_str(), (int)data.length(), 0);
    }
}

} // namespace BitrateSwitch
