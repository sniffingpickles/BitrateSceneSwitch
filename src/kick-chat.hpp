#pragma once

#include "chat-client.hpp"
#include "config.hpp"
#include "ws-client.hpp"
#include <atomic>
#include <functional>
#include <thread>

namespace BitrateSwitch {

class KickChatClient {
public:
	using CommandCallback = std::function<void(const ChatMessage &)>;
	using RaidCallback = std::function<void(const std::string &targetSlug,
						const std::string &displayName)>;

	KickChatClient();
	~KickChatClient();

	void setConfig(const ChatConfig &cfg);
	void setCommandCallback(CommandCallback cb);
	void setRaidCallback(RaidCallback cb);

	bool connect();
	void disconnect();
	bool isConnected() const;
	void sendMessage(const std::string &);

private:
	void workerMain();
	void dispatchText(const std::string &utf8);
	static bool parsePusherEvent(const std::string &utf8,
				     std::string *eventName,
				     std::string *dataJson);
	void handleChatJson(const std::string &dataJson);
	void handleHostRaidJson(const std::string &dataJson);

	ChatConfig config_;
	CommandCallback cmdCb_;
	RaidCallback raidCb_;

	WsClient ws_;
	std::atomic<bool> running_{false};
	std::atomic<bool> connected_{false};
	std::thread worker_;
};

} // namespace BitrateSwitch
