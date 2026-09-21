#include "CurlClient.h"
#include "RepoRoot.h"

#include "market_data/MboumIngest.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Secrets.h"
#include "market_data/Store.h"
#include "market_data/Time.h"

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

void usage()
{
    std::cerr << "Usage: ingest [--from YYYYMMDD] [--to YYYYMMDD] [--db PATH] [--secrets PATH] SYMBOL [SYMBOL...]\n"
                 "Reads the MBoum API key from secrets.json key \"mboum\". Never pass the key on the CLI.\n";
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        std::filesystem::path secrets = myapp::defaultSecretsPath();
        std::filesystem::path db = myapp::defaultMarketDataDbPath();
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
                from = myapp::parseSessionDate(need("--from"));
            }
            else if (arg == "--to")
            {
                to = myapp::parseSessionDate(need("--to"));
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
            from_date = myapp::toSessionDate(std::chrono::sys_days{ymd} - std::chrono::days{14});
        }

        const std::string key = myapp::loadMboumApiKey(secrets);
        myapp::CurlClient http(key);
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
                return http.getWithRetry(url);
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
