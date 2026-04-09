#pragma once

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

#include <atomic>
#include <string>

namespace BitrateSwitch {

class WsClient {
public:
	enum class RecvResult { Message, Timeout, Error, Closed };

	WsClient();
	~WsClient();

	bool connect(const std::string &url);
	void disconnect();
	bool send(const std::string &text);
	RecvResult recv(std::string &out);
	bool isConnected() const;

private:
#ifdef _WIN32
	HINTERNET session_ = nullptr;
	HINTERNET connect_ = nullptr;
	HINTERNET websocket_ = nullptr;
#endif
	std::atomic<bool> connected_{false};
};

} // namespace BitrateSwitch
