#include "market_data/MboumIngest.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Secrets.h"
#include "market_data/Store.h"
#include "market_data/Time.h"

#include <curl/curl.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr const char* kUserAgent =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/129.0.0.0 Safari/537.36";

[[nodiscard]] bool isSuperprojectRoot(const std::filesystem::path& dir)
{
    return std::filesystem::is_regular_file(dir / "CMakeLists.txt") &&
           std::filesystem::is_directory(dir / "libs" / "market-data");
}

[[nodiscard]] std::filesystem::path findRepoRoot()
{
    auto dir = std::filesystem::current_path();
    for (int i = 0; i < 16; ++i)
    {
        if (isSuperprojectRoot(dir))
        {
            return dir;
        }
        if (!dir.has_parent_path() || dir == dir.parent_path())
        {
            break;
        }
        dir = dir.parent_path();
    }
    return std::filesystem::current_path();
}

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

class CurlClient
{
public:
    explicit CurlClient(std::string_view bearer)
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
        try
        {
            auth_ = "Authorization: Bearer " + std::string(bearer);
            appendHeader(auth_.c_str());
            appendHeader("Accept: application/json");
            curl_easy_setopt(easy_, CURLOPT_HTTPHEADER, headers_);
            curl_easy_setopt(easy_, CURLOPT_USERAGENT, kUserAgent);
            curl_easy_setopt(easy_, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(easy_, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(easy_, CURLOPT_HTTPGET, 1L);
            curl_easy_setopt(easy_, CURLOPT_PROTOCOLS_STR, "https");
            curl_easy_setopt(easy_, CURLOPT_WRITEFUNCTION, writeResponseBody);
        }
        catch (...)
        {
            curl_slist_free_all(headers_);
            headers_ = nullptr;
            curl_easy_cleanup(easy_);
            easy_ = nullptr;
            curl_global_cleanup();
            global_inited_ = false;
            throw;
        }
    }

    ~CurlClient()
    {
        if (headers_ != nullptr)
        {
            curl_slist_free_all(headers_);
        }
        if (easy_ != nullptr)
        {
            curl_easy_cleanup(easy_);
        }
        if (global_inited_)
        {
            curl_global_cleanup();
        }
    }

    CurlClient(const CurlClient&) = delete;
    CurlClient& operator=(const CurlClient&) = delete;
    CurlClient(CurlClient&&) = delete;
    CurlClient& operator=(CurlClient&&) = delete;

    [[nodiscard]] myapp::HttpResponse get(std::string_view url)
    {
        std::string body;
        const std::string owned(url);
        curl_easy_setopt(easy_, CURLOPT_URL, owned.c_str());
        curl_easy_setopt(easy_, CURLOPT_WRITEDATA, &body);
        myapp::HttpResponse response;
        const CURLcode rc = curl_easy_perform(easy_);
        if (rc != CURLE_OK)
        {
            response.status = 0;
            response.error = curl_easy_strerror(rc);
            return response;
        }
        long http = 0;
        curl_easy_getinfo(easy_, CURLINFO_RESPONSE_CODE, &http);
        response.status = static_cast<int>(http);
        response.body = std::move(body);
        return response;
    }

private:
    void appendHeader(const char* line)
    {
        curl_slist* next = curl_slist_append(headers_, line);
        if (next == nullptr)
        {
            throw std::runtime_error("curl_slist_append failed");
        }
        headers_ = next;
    }

    CURL* easy_ = nullptr;
    curl_slist* headers_ = nullptr;
    std::string auth_;
    bool global_inited_ = false;
};

[[nodiscard]] myapp::SessionDate parseDateArg(std::string_view text)
{
    std::string digits;
    digits.reserve(8);
    for (const char c : text)
    {
        if (c >= '0' && c <= '9')
        {
            digits.push_back(c);
        }
    }
    if (digits.size() != 8)
    {
        throw std::runtime_error("dates must be YYYYMMDD or YYYY-MM-DD");
    }
    const int value = std::stoi(digits);
    (void)myapp::sessionDateToYmd(static_cast<myapp::SessionDate>(value));
    return static_cast<myapp::SessionDate>(value);
}

void usage()
{
    std::cerr << "Usage: ingest [--from YYYYMMDD] [--to YYYYMMDD] [--db PATH] [--secrets PATH] SYMBOL [SYMBOL...]\n"
                 "Reads the MBoum API key from secrets.json key \"mboum\". Never pass the key on the CLI.\n";
}

[[nodiscard]] bool retryableHttpStatus(int status)
{
    return status == 0 || status == 429 || (status >= 500 && status < 600);
}

[[nodiscard]] myapp::HttpResponse curlGetWithRetry(CurlClient& client, std::string_view url)
{
    myapp::HttpResponse last;
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        last = client.get(url);
        if (!retryableHttpStatus(last.status) || attempt == 3)
        {
            return last;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1 << attempt));
    }
    return last;
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto root = findRepoRoot();
        std::filesystem::path secrets = root / "secrets.json";
        std::filesystem::path db = root / "data" / "market-data.sqlite";
        std::optional<myapp::SessionDate> from;
        std::optional<myapp::SessionDate> to;
        std::vector<std::string> symbols;

        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg{argv[i]};
            auto need = [&](const char* name) -> std::string_view {
                if (i + 1 >= argc)
                {
                    throw std::runtime_error(std::string("missing value for ") + name);
                }
                return argv[++i];
            };
            if (arg == "--from")
            {
                from = parseDateArg(need("--from"));
            }
            else if (arg == "--to")
            {
                to = parseDateArg(need("--to"));
            }
            else if (arg == "--db")
            {
                db = need("--db");
            }
            else if (arg == "--secrets")
            {
                secrets = need("--secrets");
            }
            else if (arg == "-h" || arg == "--help")
            {
                usage();
                return 0;
            }
            else if (arg.starts_with("-"))
            {
                throw std::runtime_error("unknown flag: " + std::string(arg));
            }
            else
            {
                symbols.emplace_back(arg);
            }
        }
        if (symbols.empty())
        {
            usage();
            return 2;
        }

        const myapp::SessionDate today = myapp::utcToSessionDate("America/New_York", myapp::nowUtc());
        const myapp::SessionDate to_date = to.value_or(today);
        myapp::SessionDate from_date = from.value_or(to_date);
        if (!from.has_value())
        {
            const auto ymd = myapp::sessionDateToYmd(to_date);
            from_date = myapp::toSessionDate(std::chrono::year_month_day{
                std::chrono::sys_days{ymd} - std::chrono::days{14}});
        }

        const std::string key = myapp::loadMboumApiKey(secrets);
        CurlClient http(key);
        std::filesystem::create_directories(db.parent_path());
        myapp::Store store(db);

        for (const auto& symbol : symbols)
        {
            std::clog << "ingest " << symbol << " " << from_date << ".." << to_date << '\n';
            bool first = true;
            auto get = [&](std::string_view url) {
                if (!first)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(80));
                }
                first = false;
                return curlGetWithRetry(http, url);
            };
            const auto result = myapp::ingestSymbol(store, get, symbol, from_date, to_date);
            for (const auto& day : result.days)
            {
                std::clog << "  " << day.session_date << " " << myapp::toSql(day.status)
                          << " bars=" << day.bar_count << " http=" << day.http_status << '\n';
            }
        }
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
