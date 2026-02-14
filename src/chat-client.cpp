#include "chat-client.hpp"
#include <obs-module.h>
#include <algorithm>
#include <cstring>

namespace BitrateSwitch {

ChatClient::ChatClient()
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

ChatClient::~ChatClient()
{
    disconnect();
#ifdef _WIN32
    WSACleanup();
#endif
}

void ChatClient::setConfig(const ChatConfig& config)
{
    config_ = config;
}

void ChatClient::setCommandCallback(CommandCallback callback)
{
    callback_ = callback;
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
            if (running_) {
                blog(LOG_WARNING, "[BitrateSceneSwitch] Chat: Connection lost");
                connected_ = false;
            }
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

void ChatClient::handleMessage(const std::string& raw)
{
    if (raw.substr(0, 4) == "PING") {
        sendRaw("PONG" + raw.substr(4) + "\r\n");
        return;
    }
    
    if (raw.find("PRIVMSG") != std::string::npos) {
        ChatMessage msg = parseMessage(raw);
        if (msg.command != ChatCommand::None && isAdmin(msg.username)) {
            if (callback_) {
                callback_(msg);
            }
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
    std::string lower = message;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    auto checkCmd = [&](const std::string& cmd, ChatCommand type) -> bool {
        if (lower == cmd || lower.substr(0, cmd.length() + 1) == cmd + " ") {
            if (lower.length() > cmd.length() + 1) {
                args = message.substr(cmd.length() + 1);
            }
            return true;
        }
        return false;
    };
    
    if (checkCmd(config_.cmdLive, ChatCommand::Live)) return ChatCommand::Live;
    if (checkCmd(config_.cmdLow, ChatCommand::Low)) return ChatCommand::Low;
    if (checkCmd(config_.cmdBrb, ChatCommand::Brb)) return ChatCommand::Brb;
    if (checkCmd(config_.cmdRefresh, ChatCommand::Refresh)) return ChatCommand::Refresh;
    if (checkCmd(config_.cmdStatus, ChatCommand::Status)) return ChatCommand::Status;
    if (checkCmd(config_.cmdTrigger, ChatCommand::Trigger)) return ChatCommand::Trigger;
    if (checkCmd(config_.cmdFix, ChatCommand::Fix)) return ChatCommand::Fix;
    if (checkCmd(config_.cmdSwitchScene, ChatCommand::SwitchScene)) return ChatCommand::SwitchScene;
    
    return ChatCommand::None;
}

bool ChatClient::isAdmin(const std::string& username)
{
    if (config_.admins.empty()) return true;
    
    std::string lower = username;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    for (const auto& admin : config_.admins) {
        std::string adminLower = admin;
        std::transform(adminLower.begin(), adminLower.end(), adminLower.begin(), ::tolower);
        if (lower == adminLower) return true;
    }
    
    std::string channelLower = config_.channel;
    std::transform(channelLower.begin(), channelLower.end(), channelLower.begin(), ::tolower);
    if (lower == channelLower) return true;
    
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
