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

#endif // _WIN32
