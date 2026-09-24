// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "CurlClient.h"
#include "IngestDefaults.h"
#include "OpenFigiSession.h"
#include "RepoRoot.h"

#include "market_data/Identity.h"
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
    std::cerr << "Usage: ingest [--timeframe 1m|1d] [--from YYYYMMDD] [--to YYYYMMDD] "
                 "[--db PATH] [--secrets PATH] SYMBOL [SYMBOL...]\n"
                 "       ingest [--db PATH] [--secrets PATH] --verify [--all]\n"
                 "       ingest [--db PATH] --delist SYMBOL\n"
                 "Reads the MBoum API key from secrets.json key \"mboum\" and the optional OpenFIGI key\n"
                 "from key \"openfigi\". Never pass a key on the CLI.\n"
                 "Every symbol is confirmed with OpenFIGI before it is written. --verify re-checks\n"
                 "stored tickers that are 24 h old or older (--all: every one) and applies renames\n"
                 "and delistings. --delist closes an open listing by hand.\n";
}

}  // namespace

int main(int argc, char** argv) // NOLINT(bugprone-exception-escape)
{
    try
    {
        std::filesystem::path secrets = terminal::defaultSecretsPath();
        std::filesystem::path db = terminal::defaultMarketDataDbPath();
        std::optional<terminal::SessionDate> from;
        std::optional<terminal::SessionDate> to;
        int timeframe = terminal::kTimeframe1m;
        std::vector<std::string> symbols;
        bool verify = false;
        bool verify_all = false;
        std::optional<std::string> delist;

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
                from = terminal::parseSessionDate(need("--from"));
            }
            else if (arg == "--timeframe")
            {
                const std::string_view value = need("--timeframe");
                if (value == "1m")
                {
                    timeframe = terminal::kTimeframe1m;
                }
                else if (value == "1d")
                {
                    timeframe = terminal::kTimeframe1d;
                }
                else
                {
                    throw std::runtime_error("timeframe must be 1m or 1d");
                }
            }
            else if (arg == "--to")
            {
                to = terminal::parseSessionDate(need("--to"));
            }
            else if (arg == "--db")
            {
                db = need("--db");
            }
            else if (arg == "--secrets")
            {
                secrets = need("--secrets");
            }
            else if (arg == "--verify")
            {
                verify = true;
            }
            else if (arg == "--all")
            {
                verify_all = true;
            }
            else if (arg == "--delist")
            {
                delist = std::string(need("--delist"));
            }
            else if (arg == "-h" || arg == "--help")
            {
                usage();
                return 0;
            }
            else if (arg.starts_with('-'))
            {
                throw std::runtime_error("unknown flag: " + std::string(arg));
            }
            else
            {
                symbols.emplace_back(arg);
            }
        }
        if (verify_all && !verify)
        {
            throw std::runtime_error("--all needs --verify");
        }
        if (delist.has_value())
        {
            if (verify || !symbols.empty())
            {
                throw std::runtime_error("--delist takes one symbol and nothing else");
            }
            terminal::Store store(db);
            const auto open = store.findOpenListing(*delist);
            if (!open.has_value())
            {
                throw std::runtime_error(*delist + " has no open listing");
            }
            store.closeListing(open->id, terminal::nowUtc(), terminal::ListingCloseReason::Manual);
            std::clog << "delisted " << open->symbol << " (" << open->figi.value_or("no FIGI") << ")\n";
            return 0;
        }
        if (verify)
        {
            if (!symbols.empty())
            {
                throw std::runtime_error("--verify takes no symbols");
            }
            std::filesystem::create_directories(db.parent_path());
            terminal::Store store(db);
            terminal::OpenFigiSession figi(secrets);
            const terminal::UnixSeconds now = terminal::nowUtc();
            const auto ids = terminal::instrumentsDueForVerification(store, now, verify_all);
            const auto reports = terminal::verifyIdentities(store, figi.client(), now, ids);
            int failures = 0;
            for (const terminal::VerifyReport& report : reports)
            {
                std::clog << report.symbol << " " << terminal::describe(report) << '\n';
                if (report.outcome == terminal::VerifyOutcome::Conflict ||
                    report.outcome == terminal::VerifyOutcome::Unreachable ||
                    report.outcome == terminal::VerifyOutcome::Unresolved)
                {
                    ++failures;
                }
            }
            std::clog << reports.size() << " checked\n";
            return failures == 0 ? 0 : 1;
        }
        if (symbols.empty())
        {
            usage();
            return 2;
        }

        const terminal::SessionDate today = terminal::utcToSessionDate("America/New_York", terminal::nowUtc());
        const terminal::SessionDate to_date = to.value_or(today);
        terminal::SessionDate from_date = from.value_or(to_date);
        if (!from.has_value())
        {
            const auto ymd = terminal::sessionDateToYmd(to_date);
            if (timeframe == terminal::kTimeframe1d)
            {
                from_date = terminal::toSessionDate(std::chrono::sys_days{ymd} -
                                                        std::chrono::days{terminal::kIngestDefaultDailyDays});
            }
            else
            {
                from_date = terminal::toSessionDate(
                    std::chrono::sys_days{ymd} - std::chrono::days{terminal::kIngestDefaultIntradayDays});
            }
        }

        const std::string key = terminal::loadMboumApiKey(secrets);
        terminal::CurlClient http(key);
        std::filesystem::create_directories(db.parent_path());
        terminal::Store store(db);
        terminal::OpenFigiSession figi(secrets);

        for (const auto& symbol : symbols)
        {
            std::clog << "ingest " << symbol << " "
                      << (timeframe == terminal::kTimeframe1d ? "1d " : "1m ") << from_date << ".."
                      << to_date << '\n';
            bool first = true;
            auto get = [&](std::string_view url) {
                if (!first)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(80));
                }
                first = false;
                return http.getWithRetry(url);
            };
            const auto result = timeframe == terminal::kTimeframe1d
                                    ? terminal::ingestDailySymbol(store, get, figi.client(), symbol, from_date, to_date)
                                    : terminal::ingestSymbol(store, get, figi.client(), symbol, from_date, to_date);
            if (!result.identity_notice.empty())
            {
                std::clog << "  " << result.identity_notice << '\n';
            }
            for (const auto& day : result.days)
            {
                std::clog << "  " << day.session_date << " " << terminal::toSql(day.status)
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
