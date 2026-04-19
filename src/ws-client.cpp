#include "ws-client.hpp"
#include <obs-module.h>

#ifdef _WIN32

namespace BitrateSwitch {

static std::wstring utf8ToWide(const std::string &s)
{
	if (s.empty())
		return {};
	int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
	std::wstring w(len, 0);
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
	return w;
}

WsClient::WsClient() = default;

WsClient::~WsClient()
{
	disconnect();
}

bool WsClient::connect(const std::string &url)
{
	disconnect();

	std::string httpUrl = url;
	bool secure = false;
	if (url.compare(0, 6, "wss://") == 0) {
		httpUrl = "https://" + url.substr(6);
		secure = true;
	} else if (url.compare(0, 5, "ws://") == 0) {
		httpUrl = "http://" + url.substr(5);
	}

	std::wstring wurl = utf8ToWide(httpUrl);

	URL_COMPONENTS uc = {};
	uc.dwStructSize = sizeof(uc);
	uc.dwHostNameLength = (DWORD)-1;
	uc.dwUrlPathLength = (DWORD)-1;
	uc.dwExtraInfoLength = (DWORD)-1;
	if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
		return false;

	std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
	std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
	if (uc.dwExtraInfoLength > 0)
		path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
	if (path.empty())
		path = L"/";

	session_ = WinHttpOpen(L"BitrateSceneSwitch/1.0",
			       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			       WINHTTP_NO_PROXY_NAME,
			       WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session_)
		return false;

	// resolve/connect/send capped at 5s so a dead route can't wedge the
	// worker thread (and therefore plugin shutdown). receive stays at
	// 1s so the worker can poll running_ between recv calls.
	WinHttpSetTimeouts(session_, 5000, 5000, 5000, 1000);

	DWORD recvTimeout = 1000;
	WinHttpSetOption(session_, WINHTTP_OPTION_RECEIVE_TIMEOUT,
			 &recvTimeout, sizeof(recvTimeout));

	connect_ = WinHttpConnect(session_, host.c_str(), uc.nPort, 0);
	if (!connect_) {
		disconnect();
		return false;
	}

	DWORD flags = WINHTTP_FLAG_REFRESH;
	if (secure)
		flags |= WINHTTP_FLAG_SECURE;

	HINTERNET request = WinHttpOpenRequest(
		connect_, L"GET", path.c_str(), nullptr,
		WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
	if (!request) {
		disconnect();
		return false;
	}

	if (!WinHttpSetOption(request,
			      WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,
			      nullptr, 0)) {
		WinHttpCloseHandle(request);
		disconnect();
		return false;
	}

	DWORD keepAlive = 30000;
	WinHttpSetOption(request,
			 WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL,
			 &keepAlive, sizeof(keepAlive));

	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
				WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		WinHttpCloseHandle(request);
		disconnect();
		return false;
	}

	if (!WinHttpReceiveResponse(request, nullptr)) {
		WinHttpCloseHandle(request);
		disconnect();
		return false;
	}

	websocket_ = WinHttpWebSocketCompleteUpgrade(request, 0);
	WinHttpCloseHandle(request);

	if (!websocket_) {
		disconnect();
		return false;
	}

	connected_ = true;
	return true;
}

void WsClient::disconnect()
{
	connected_ = false;
	if (websocket_) {
		WinHttpCloseHandle(websocket_);
		websocket_ = nullptr;
	}
	if (connect_) {
		WinHttpCloseHandle(connect_);
		connect_ = nullptr;
	}
	if (session_) {
		WinHttpCloseHandle(session_);
		session_ = nullptr;
	}
}

bool WsClient::send(const std::string &text)
{
	if (!websocket_ || !connected_)
		return false;
	DWORD err = WinHttpWebSocketSend(
		websocket_, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
		(PVOID)text.data(), (DWORD)text.size());
	if (err != NO_ERROR) {
		connected_ = false;
		return false;
	}
	return true;
}

WsClient::RecvResult WsClient::recv(std::string &out)
{
	out.clear();
	if (!websocket_ || !connected_)
		return RecvResult::Error;

	char buf[8192];
	for (;;) {
		DWORD bytesRead = 0;
		WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
		DWORD err = WinHttpWebSocketReceive(websocket_, buf,
						    sizeof(buf),
						    &bytesRead, &type);
		if (err == ERROR_WINHTTP_TIMEOUT)
			return RecvResult::Timeout;
		if (err != NO_ERROR) {
			connected_ = false;
			return RecvResult::Error;
		}
		if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
			connected_ = false;
			return RecvResult::Closed;
		}
		out.append(buf, bytesRead);
		if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ||
		    type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE)
			return RecvResult::Message;
	}
}

bool WsClient::isConnected() const
{
	return connected_;
}

} // namespace BitrateSwitch

#else // !_WIN32 — POSIX + libcurl WebSocket

#include <curl/websockets.h>
#include <poll.h>

namespace BitrateSwitch {

WsClient::WsClient() = default;

WsClient::~WsClient()
{
	disconnect();
}

bool WsClient::connect(const std::string &url)
{
	disconnect();

	curl_ = curl_easy_init();
	if (!curl_)
		return false;

	curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl_, CURLOPT_CONNECT_ONLY, 2L);
	// bound the connect handshake so a dead route can't wedge us for
	// libcurl's default 5min connect timeout. these only apply during
	// curl_easy_perform; once we switch to ws send/recv they no longer
	// gate the long-running session.
	curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
	curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, 10000L);

	CURLcode res = curl_easy_perform(curl_);
	if (res != CURLE_OK) {
		blog(LOG_WARNING,
		     "[BitrateSceneSwitch] WS connect failed: %s",
		     curl_easy_strerror(res));
		curl_easy_cleanup(curl_);
		curl_ = nullptr;
		return false;
	}

	connected_ = true;
	return true;
}

void WsClient::disconnect()
{
	connected_ = false;
	if (curl_) {
		size_t sent = 0;
		curl_ws_send(curl_, "", 0, &sent, 0, CURLWS_CLOSE);
		curl_easy_cleanup(curl_);
		curl_ = nullptr;
	}
}

bool WsClient::send(const std::string &text)
{
	if (!curl_ || !connected_)
		return false;

	size_t sent = 0;
	CURLcode res = curl_ws_send(curl_, text.data(), text.size(),
				    &sent, 0, CURLWS_TEXT);
	if (res != CURLE_OK) {
		connected_ = false;
		return false;
	}
	return true;
}

WsClient::RecvResult WsClient::recv(std::string &out)
{
	out.clear();
	if (!curl_ || !connected_)
		return RecvResult::Error;

	curl_socket_t sockfd;
	CURLcode res = curl_easy_getinfo(curl_, CURLINFO_ACTIVESOCKET,
					 &sockfd);
	if (res != CURLE_OK || sockfd == CURL_SOCKET_BAD) {
		connected_ = false;
		return RecvResult::Error;
	}

	struct pollfd pfd;
	pfd.fd = sockfd;
	pfd.events = POLLIN;
	int pr = poll(&pfd, 1, 1000);

	if (pr == 0)
		return RecvResult::Timeout;
	if (pr < 0) {
		connected_ = false;
		return RecvResult::Error;
	}

	char buf[8192];
	for (;;) {
		size_t rlen = 0;
		const struct curl_ws_frame *meta = nullptr;
		res = curl_ws_recv(curl_, buf, sizeof(buf), &rlen, &meta);

		if (res == CURLE_AGAIN)
			break;
		if (res != CURLE_OK) {
			connected_ = false;
			return RecvResult::Error;
		}

		if (meta && (meta->flags & CURLWS_CLOSE)) {
			connected_ = false;
			return RecvResult::Closed;
		}

		out.append(buf, rlen);

		if (!meta || meta->bytesleft == 0)
			break;
	}

	if (out.empty())
		return RecvResult::Timeout;

	return RecvResult::Message;
}

bool WsClient::isConnected() const
{
	return connected_;
}

} // namespace BitrateSwitch

#endif
