#include "CurlClient.h"

#include <curl/curl.h>

#include <chrono>
#include <stdexcept>
#include <thread>

namespace myapp {
namespace {

constexpr const char* kUserAgent =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/129.0.0.0 Safari/537.36";

extern "C" size_t writeResponseBody(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* body = static_cast<std::string*>(userdata);
    const size_t n = size * nmemb;
    try
    {
        body->append(ptr, n);
    }
    catch (...)
    {
        return 0;
    }
    return n;
}

[[nodiscard]] bool retryableHttpStatus(int status)
{
    return status == 0 || status == 429 || (status >= 500 && status < 600);
}

}  // namespace

CurlClient::CurlClient(std::string_view bearer)
{
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
    {
        throw std::runtime_error("curl_global_init failed");
    }
    global_inited_ = true;
    easy_ = curl_easy_init();
    if (easy_ == nullptr)
    {
        curl_global_cleanup();
        global_inited_ = false;
        throw std::runtime_error("curl_easy_init failed");
    }
    auto* easy = static_cast<CURL*>(easy_);
    try
    {
        auth_ = "Authorization: Bearer " + std::string(bearer);
        appendHeader(auth_.c_str());
        appendHeader("Accept: application/json");
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, static_cast<curl_slist*>(headers_));
        curl_easy_setopt(easy, CURLOPT_USERAGENT, kUserAgent);
        curl_easy_setopt(easy, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "https");
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, writeResponseBody);
    }
    catch (...)
    {
        curl_slist_free_all(static_cast<curl_slist*>(headers_));
        headers_ = nullptr;
        curl_easy_cleanup(easy);
        easy_ = nullptr;
        curl_global_cleanup();
        global_inited_ = false;
        throw;
    }
}

CurlClient::~CurlClient()
{
    if (headers_ != nullptr)
    {
        curl_slist_free_all(static_cast<curl_slist*>(headers_));
    }
    if (easy_ != nullptr)
    {
        curl_easy_cleanup(static_cast<CURL*>(easy_));
    }
    if (global_inited_)
    {
        curl_global_cleanup();
    }
}

HttpResponse CurlClient::get(std::string_view url)
{
    std::string body;
    const std::string owned(url);
    auto* easy = static_cast<CURL*>(easy_);
    curl_easy_setopt(easy, CURLOPT_URL, owned.c_str());
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &body);
    HttpResponse response;
    const CURLcode rc = curl_easy_perform(easy);
    if (rc != CURLE_OK)
    {
        response.status = 0;
        response.error = curl_easy_strerror(rc);
        return response;
    }
    long http = 0;
    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &http);
    response.status = static_cast<int>(http);
    response.body = std::move(body);
    return response;
}

HttpResponse CurlClient::getWithRetry(std::string_view url)
{
    HttpResponse last;
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        last = get(url);
        if (!retryableHttpStatus(last.status) || attempt == 3)
        {
            return last;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1 << attempt));
    }
    return last;
}

void CurlClient::appendHeader(const char* line)
{
    curl_slist* next = curl_slist_append(static_cast<curl_slist*>(headers_), line);
    if (next == nullptr)
    {
        throw std::runtime_error("curl_slist_append failed");
    }
    headers_ = next;
}

}  // namespace myapp
