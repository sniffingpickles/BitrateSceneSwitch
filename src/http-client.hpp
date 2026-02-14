#pragma once

#include <string>
#include <functional>
#include <curl/curl.h>

namespace BitrateSwitch {

struct HttpResponse {
    long statusCode = 0;
    std::string body;
    bool success = false;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpResponse get(const std::string &url, int timeoutMs = 5000);

private:
    static size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
};

} // namespace BitrateSwitch
