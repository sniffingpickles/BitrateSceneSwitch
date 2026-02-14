#include "http-client.hpp"
#include <obs-module.h>

namespace BitrateSwitch {

HttpClient::HttpClient()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient()
{
    curl_global_cleanup();
}

size_t HttpClient::writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    std::string *response = static_cast<std::string *>(userdata);
    size_t totalSize = size * nmemb;
    response->append(ptr, totalSize);
    return totalSize;
}

HttpResponse HttpClient::get(const std::string &url, int timeoutMs)
{
    HttpResponse response;
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        blog(LOG_ERROR, "[BitrateSceneSwitch] Failed to initialize CURL");
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMs);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeoutMs / 2);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "BitrateSceneSwitch/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
        response.success = (response.statusCode >= 200 && response.statusCode < 300);
    }

    curl_easy_cleanup(curl);
    return response;
}

} // namespace BitrateSwitch
