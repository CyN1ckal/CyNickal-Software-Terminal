// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "CurlClient.h"

#include <curl/curl.h>

#include <cctype>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace terminal {
namespace {

constexpr const char* kUserAgent =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/129.0.0.0 Safari/537.36";

extern "C" size_t writeResponseBody(char const* ptr, size_t size, size_t nmemb, void* userdata)
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

extern "C" size_t captureResponseHeader(char const* buffer, size_t size, size_t nitems, void* userdata)
{
    auto* headers = static_cast<std::vector<std::pair<std::string, std::string>>*>(userdata);
    const size_t n = size * nitems;
    try
    {
        const std::string_view line(buffer, n);
        const auto colon = line.find(':');
        if (colon != std::string_view::npos)
        {
            std::string name(line.substr(0, colon));
            for (char& ch : name)
            {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            std::string_view value = line.substr(colon + 1);
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
            {
                value.remove_prefix(1);
            }
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' '))
            {
                value.remove_suffix(1);
            }
            headers->emplace_back(std::move(name), std::string(value));
        }
        else if (line.starts_with("HTTP/"))
        {
            headers->clear();  // a redirect or 100-continue starts a new header block
        }
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
        if (!bearer.empty())
        {
            auth_ = "Authorization: Bearer " + std::string(bearer);
            appendHeader(auth_.c_str());
        }
        appendHeader("Accept: application/json");
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, static_cast<curl_slist*>(headers_));
        curl_easy_setopt(easy, CURLOPT_USERAGENT, kUserAgent);
        curl_easy_setopt(easy, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "https");
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, writeResponseBody);
        curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, captureResponseHeader);
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
    HttpResponse response;
    curl_easy_setopt(easy, CURLOPT_URL, owned.c_str());
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(easy, CURLOPT_HEADERDATA, &response.headers);
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

HttpResponse CurlClient::post(std::string_view url,
                              std::string_view body,
                              const std::vector<std::string>& extra_headers)
{
    auto* easy = static_cast<CURL*>(easy_);
    curl_slist* headers = nullptr;
    auto append = [&headers](const char* line) {
        curl_slist* next = curl_slist_append(headers, line); // NOLINT(misc-const-correctness)
        if (next == nullptr)
        {
            curl_slist_free_all(headers);
            throw std::runtime_error("curl_slist_append failed");
        }
        headers = next;
    };
    append("Accept: application/json");
    append("Content-Type: application/json");
    for (const std::string& line : extra_headers)
    {
        append(line.c_str());
    }

    std::string response_body;
    const std::string owned_url(url);
    const std::string owned_body(body);
    HttpResponse response;
    curl_easy_setopt(easy, CURLOPT_URL, owned_url.c_str());
    curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(easy, CURLOPT_POSTFIELDS, owned_body.c_str());
    curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(owned_body.size()));
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(easy, CURLOPT_HEADERDATA, &response.headers);
    const CURLcode rc = curl_easy_perform(easy);
    // Restore the GET setup so the next get() is unaffected.
    curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(easy, CURLOPT_HTTPHEADER, static_cast<curl_slist*>(headers_));
    curl_slist_free_all(headers);
    if (rc != CURLE_OK)
    {
        response.status = 0;
        response.error = curl_easy_strerror(rc);
        response.headers.clear();
        return response;
    }
    long http = 0;
    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &http);
    response.status = static_cast<int>(http);
    response.body = std::move(response_body);
    return response;
}

void CurlClient::appendHeader(const char* line)
{
    // headers_ is void* and must keep a mutable curl_slist.
    curl_slist* next = curl_slist_append(static_cast<curl_slist*>(headers_), line); // NOLINT(misc-const-correctness)
    if (next == nullptr)
    {
        throw std::runtime_error("curl_slist_append failed");
    }
    headers_ = next;
}

}  // namespace terminal
