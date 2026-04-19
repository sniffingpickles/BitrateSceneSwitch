#pragma once

#ifdef _WIN32
// pull winsock2 before windows.h so headers that include winsock2.h later
// (chat-client.hpp via switcher.hpp) don't collide with the legacy winsock
// types that windows.h would otherwise define
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
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
#else
	CURL *curl_ = nullptr;
#endif
	std::atomic<bool> connected_{false};
};

} // namespace BitrateSwitch
