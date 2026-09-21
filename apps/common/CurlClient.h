#pragma once

#include "market_data/MboumIngest.h"

#include <string>
#include <string_view>

namespace myapp {

class CurlClient
{
public:
    explicit CurlClient(std::string_view bearer);
    ~CurlClient();

    CurlClient(const CurlClient&) = delete;
    CurlClient& operator=(const CurlClient&) = delete;
    CurlClient(CurlClient&&) = delete;
    CurlClient& operator=(CurlClient&&) = delete;

    [[nodiscard]] HttpResponse get(std::string_view url);
    [[nodiscard]] HttpResponse getWithRetry(std::string_view url);

private:
    void appendHeader(const char* line);

    void* easy_ = nullptr;
    void* headers_ = nullptr;
    std::string auth_;
    bool global_inited_ = false;
};

}  // namespace myapp
