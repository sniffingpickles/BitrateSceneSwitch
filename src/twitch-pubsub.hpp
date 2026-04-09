#pragma once

#include "ws-client.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace BitrateSwitch {

class TwitchPubSubClient {
public:
	using RaidCallback = std::function<void(const std::string &targetLogin,
						const std::string &displayName)>;

	TwitchPubSubClient();
	~TwitchPubSubClient();

	void setRaidCallback(RaidCallback cb);
	void subscribeRaid(const std::string &broadcasterUserId);
	void start();
	void stop();
	bool isConnected() const;

private:
	void workerMain();
	void flushListen();

	RaidCallback raidCb_;
	std::mutex mutex_;
	std::vector<std::string> topics_;
	bool resendListen_ = false;
	std::atomic<bool> running_{false};
	std::atomic<bool> connected_{false};
	WsClient ws_;
	std::thread worker_;
	int nonce_ = 0;

	std::chrono::steady_clock::time_point lastRaidEmit_;
	bool haveLastRaidEmit_ = false;
};

} // namespace BitrateSwitch
