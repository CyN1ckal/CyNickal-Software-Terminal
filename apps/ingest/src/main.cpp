#include "market_data/MboumIngest.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Secrets.h"
#include "market_data/Store.h"
#include "market_data/Time.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

constexpr const char* kUserAgentHeader =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/129.0.0.0 Safari/537.36";

[[nodiscard]] std::filesystem::path findRepoRoot()
{
    auto dir = std::filesystem::current_path();
    for (int i = 0; i < 16; ++i)
    {
        if (std::filesystem::exists(dir / "secrets.json") || std::filesystem::exists(dir / "CMakeLists.txt"))
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

[[nodiscard]] myapp::HttpResponse curlGet(const std::string& url, const std::string& bearer)
{
    const auto body_path = std::filesystem::temp_directory_path() /
                           ("mboum-body-" + std::to_string(::getpid()) + ".json");
    const std::string auth = "Authorization: Bearer " + bearer;
    int pipefd[2];
    if (pipe(pipefd) != 0)
    {
        throw std::runtime_error("pipe failed");
    }
    const pid_t pid = fork();
    if (pid < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        throw std::runtime_error("fork failed");
    }
    if (pid == 0)
    {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
        {
            _exit(127);
        }
        close(pipefd[1]);
        execlp("curl",
               "curl",
               "-sS",
               "--max-time",
               "30",
               "-o",
               body_path.c_str(),
               "-w",
               "%{http_code}",
               "-H",
               auth.c_str(),
               "-H",
               "Accept: application/json",
               "-H",
               kUserAgentHeader,
               "--get",
               url.c_str(),
               static_cast<char*>(nullptr));
        _exit(127);
    }
    close(pipefd[1]);
    std::string status_text;
    char buf[32];
    ssize_t nread = 0;
    while ((nread = read(pipefd[0], buf, sizeof(buf))) > 0)
    {
        status_text.append(buf, static_cast<std::size_t>(nread));
    }
    close(pipefd[0]);
    int wstatus = 0;
    waitpid(pid, &wstatus, 0);

    myapp::HttpResponse response;
    std::error_code ec;
    if (std::filesystem::exists(body_path))
    {
        response.body = [](const std::filesystem::path& path) {
            std::ifstream in(path);
            return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        }(body_path);
        std::filesystem::remove(body_path, ec);
    }
    if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
    {
        response.status = 0;
        response.error = "curl failed";
        return response;
    }
    try
    {
        response.status = std::stoi(status_text);
    }
    catch (const std::exception&)
    {
        response.status = 0;
        response.error = "curl returned no HTTP status";
    }
    return response;
}

[[nodiscard]] myapp::HttpResponse curlGetWithRetry(const std::string& url, const std::string& bearer)
{
    myapp::HttpResponse last;
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        if (attempt > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
        last = curlGet(url, bearer);
        if (last.status != 429)
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
                return curlGetWithRetry(std::string(url), key);
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
