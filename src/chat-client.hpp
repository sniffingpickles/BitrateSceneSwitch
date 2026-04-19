#pragma once

#include "config.hpp"
#include <chrono>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

namespace BitrateSwitch {

enum class ChatCommand {
    None,
    Live,
    Low,
    Brb,
    Privacy,
    Refresh,
    Status,
    Trigger,
    Fix,
    SwitchScene,
    Start,
    Stop
};

struct ChatMessage {
    std::string username;
    std::string message;
    ChatCommand command = ChatCommand::None;
    std::string args;
};

class ChatClient {
public:
    using CommandCallback = std::function<void(const ChatMessage&)>;
    
    ChatClient();
    ~ChatClient();
    
    void setConfig(const ChatConfig& config);
    void setCommandCallback(CommandCallback callback);
    void setRoomIdCallback(std::function<void(const std::string &)> cb);
    
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    void sendMessage(const std::string& message);

    static ChatCommand parseCommandForConfig(const ChatConfig &cfg, const std::string &message,
                                             std::string &args);
    
private:
    void receiveLoop();
    void handleMessage(const std::string& raw);
    ChatMessage parseMessage(const std::string& raw);
    ChatCommand parseCommand(const std::string& message, std::string& args);
    bool isAdmin(const std::string& username);
    void sendRaw(const std::string& data);
    void extractRoomIdFromTags(const std::string &raw);
    
    ChatConfig config_;
    CommandCallback callback_;
    std::function<void(const std::string &)> roomIdCallback_;
    bool roomIdSent_ = false;
    
    SOCKET socket_ = INVALID_SOCKET;
    std::thread receiveThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::mutex sendMutex_;
    
    std::chrono::steady_clock::time_point lastTrafficTime_;
    std::chrono::steady_clock::time_point lastPingSent_;

    static constexpr const char* TWITCH_IRC_HOST = "irc.chat.twitch.tv";
    static constexpr int TWITCH_IRC_PORT = 6667;
    // twitch sends server PING ~5min; declare dead at 6min of silence
    static constexpr int LIVENESS_TIMEOUT_SEC = 360;
    // proactively ping every 4min so a dead socket fails the send
    static constexpr int PROACTIVE_PING_SEC = 240;
};

} // namespace BitrateSwitch
