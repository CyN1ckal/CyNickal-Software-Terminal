// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "FakeOpenFigi.h"
#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/MboumIngest.h"
#include "market_data/MboumJson.h"
#include "market_data/Store.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

[[nodiscard]] std::string readAll(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("parse v3 options keeps fractions, sentinels, and the SPX weekly suffix")
{
    constexpr std::string_view json = R"({
      "meta": {
        "expirations": {
          "weekly": ["2026-09-25", "2026-09-23"],
          "monthly": ["2026-10-16"]
        }
      },
      "body": {
        "Call": [{
          "symbol": "AAPL|20260925|110.00C",
          "baseSymbol": "AAPL",
          "strikePrice": "110.00",
          "expirationDate": "09/25/26",
          "moneyness": "+67.67%",
          "bidPrice": "229.50",
          "midpoint": "230.50",
          "askPrice": "231.50",
          "lastPrice": "0.00",
          "priceChange": "unch",
          "percentChange": "unch",
          "volume": "0",
          "openInterest": "0",
          "openInterestChange": "unch",
          "volatility": "511.86%",
          "delta": "0.9961",
          "rho": "0.0089",
          "vega": "0.0035",
          "theta": "-0.3087",
          "optionType": "Call",
          "daysToExpiration": "3",
          "tradeTime": "N/A",
          "averageVolatility": "24.94%",
          "historicVolatility30d": "20.25%",
          "baseNextEarningsDate": "10/29/26",
          "dividendExDate": "N/A",
          "baseTimeCode": "--",
          "expirationType": "weekly",
          "impliedVolatilityRank1y": "41.84%",
          "symbolType": "Call"
        }],
        "Put": [{
          "symbol": "$SPX|20261016|3200.00WC",
          "baseSymbol": "$SPX",
          "strikePrice": "3,200.00",
          "expirationDate": "10/16/26",
          "moneyness": "-1.25%",
          "bidPrice": "4,565.70",
          "midpoint": "4,575.00",
          "askPrice": "4,584.30",
          "lastPrice": "4,475.72",
          "priceChange": "-9.44",
          "percentChange": "-74.62%",
          "volume": "1,023",
          "openInterest": "30,435",
          "openInterestChange": "-1,068",
          "volatility": "17.88%",
          "delta": "-0.4438",
          "rho": "0.0000",
          "vega": "0.2116",
          "theta": "-0.6105",
          "optionType": "Call",
          "daysToExpiration": "24",
          "tradeTime": "12:02 ET",
          "averageVolatility": "11.25%",
          "historicVolatility30d": "20.25%",
          "baseNextEarningsDate": "10/29/26",
          "dividendExDate": "N/A",
          "baseTimeCode": "--",
          "expirationType": "weekly",
          "impliedVolatilityRank1y": "41.84%"
        }]
      }
    })";
    CHECK_THROWS_AS(terminal::parseMboumV3Options(json), std::runtime_error);

    constexpr std::string_view chain = R"({
      "meta": {"expirations": {"weekly": ["2026-09-25"], "monthly": ["2026-10-16"]}},
      "body": {
        "Call": [{
          "symbol": "AAPL|20260925|110.00C",
          "baseSymbol": "AAPL",
          "strikePrice": "110.00",
          "expirationDate": "09/25/26",
          "moneyness": "+67.67%",
          "bidPrice": "229.50",
          "midpoint": "230.50",
          "askPrice": "231.50",
          "lastPrice": "0.00",
          "priceChange": "unch",
          "percentChange": "unch",
          "volume": "0",
          "openInterest": "0",
          "openInterestChange": "unch",
          "volatility": "511.86%",
          "delta": "0.9961",
          "rho": "0.0089",
          "vega": "0.0035",
          "theta": "-0.3087",
          "optionType": "Call",
          "daysToExpiration": "3",
          "tradeTime": "N/A",
          "averageVolatility": "24.94%",
          "historicVolatility30d": "20.25%",
          "baseNextEarningsDate": "10/29/26",
          "dividendExDate": "N/A",
          "baseTimeCode": "--",
          "expirationType": "weekly",
          "impliedVolatilityRank1y": "41.84%"
        }, {
          "symbol": "SPY|20260925|600.00C",
          "baseSymbol": "AAPL",
          "strikePrice": "600.00",
          "expirationDate": "09/25/26",
          "moneyness": "+22.47%",
          "bidPrice": "172.57",
          "midpoint": "174.43",
          "askPrice": "176.28",
          "lastPrice": "175.33",
          "priceChange": "+1.57",
          "percentChange": "+0.90%",
          "volume": "1,023",
          "openInterest": "8",
          "openInterestChange": "+4",
          "volatility": "143.43%",
          "delta": "0.9787",
          "rho": "0.0479",
          "vega": "0.0358",
          "theta": "-0.8945",
          "optionType": "Call",
          "daysToExpiration": "3",
          "tradeTime": "09/21/26",
          "averageVolatility": "24.94%",
          "historicVolatility30d": "20.25%",
          "baseNextEarningsDate": "10/29/26",
          "dividendExDate": "N/A",
          "baseTimeCode": "--",
          "expirationType": "weekly",
          "impliedVolatilityRank1y": "41.84%"
        }]
      }
    })";
    CHECK_THROWS_AS(terminal::parseMboumV3Options(chain), std::runtime_error);

    constexpr std::string_view good = R"({
      "meta": {"expirations": {"weekly": ["2026-09-25"], "monthly": ["2026-10-16"]}},
      "body": {
        "Call": [{
          "symbol": "AAPL|20260925|110.00C",
          "baseSymbol": "AAPL",
          "strikePrice": "110.00",
          "expirationDate": "09/25/26",
          "moneyness": "+67.67%",
          "bidPrice": "229.50",
          "midpoint": "230.50",
          "askPrice": "231.50",
          "lastPrice": "0.00",
          "priceChange": "unch",
          "percentChange": "unch",
          "volume": "0",
          "openInterest": "0",
          "openInterestChange": "unch",
          "volatility": "511.86%",
          "delta": "0.9961",
          "rho": "0.0089",
          "vega": "0.0035",
          "theta": "-0.3087",
          "optionType": "Call",
          "daysToExpiration": "3",
          "tradeTime": "N/A",
          "averageVolatility": "24.94%",
          "historicVolatility30d": "20.25%",
          "baseNextEarningsDate": "10/29/26",
          "dividendExDate": "N/A",
          "baseTimeCode": "--",
          "expirationType": "weekly",
          "impliedVolatilityRank1y": "41.84%"
        }],
        "Put": [{
          "symbol": "AAPL|20260925|110.00P",
          "baseSymbol": "AAPL",
          "strikePrice": "110.00",
          "expirationDate": "09/25/26",
          "moneyness": "-67.67%",
          "bidPrice": "0.00",
          "midpoint": "0.01",
          "askPrice": "0.01",
          "lastPrice": "0.02",
          "priceChange": "unch",
          "percentChange": "unch",
          "volume": "0",
          "openInterest": "100",
          "openInterestChange": "unch",
          "volatility": "363.74%",
          "delta": "-0.0002",
          "rho": "0.0000",
          "vega": "0.0002",
          "theta": "-0.0119",
          "optionType": "Put",
          "daysToExpiration": "3",
          "tradeTime": "09/21/26",
          "averageVolatility": "24.94%",
          "historicVolatility30d": "20.25%",
          "baseNextEarningsDate": "10/29/26",
          "dividendExDate": "N/A",
          "baseTimeCode": "--",
          "expirationType": "weekly",
          "impliedVolatilityRank1y": "41.84%"
        }, {
          "symbol": "$SPX|20261016|3200.00WC",
          "baseSymbol": "AAPL",
          "strikePrice": "3,200.00",
          "expirationDate": "10/16/26",
          "moneyness": "+1.00%",
          "bidPrice": "1.00",
          "midpoint": "1.00",
          "askPrice": "1.00",
          "lastPrice": "1.00",
          "priceChange": "unch",
          "percentChange": "unch",
          "volume": "0",
          "openInterest": "0",
          "openInterestChange": "unch",
          "volatility": "10.00%",
          "delta": "0.5000",
          "rho": "0.0000",
          "vega": "0.1000",
          "theta": "-0.1000",
          "optionType": "Call",
          "daysToExpiration": "24",
          "tradeTime": "12:02 ET",
          "averageVolatility": "11.25%",
          "historicVolatility30d": "20.25%",
          "baseNextEarningsDate": "10/29/26",
          "dividendExDate": "N/A",
          "baseTimeCode": "--",
          "expirationType": "weekly",
          "impliedVolatilityRank1y": "41.84%"
        }]
      }
    })";
    CHECK_THROWS_AS(terminal::parseMboumV3Options(good), std::runtime_error);
}

TEST_CASE("parse v3 options maps one AAPL expiration and an empty calendar")
{
    constexpr std::string_view json = R"({
      "meta": {"expirations": {"weekly": ["2026-09-25", "2026-09-23"], "monthly": ["2026-10-16"]}},
      "body": {
        "Call": [{
          "symbol": "AAPL|20260925|110.00C",
          "baseSymbol": "AAPL",
          "strikePrice": "110.00",
          "expirationDate": "09/25/26",
          "moneyness": "+67.67%",
          "bidPrice": "229.50",
          "midpoint": "230.50",
          "askPrice": "231.50",
          "lastPrice": "0.00",
          "priceChange": "unch",
          "percentChange": "unch",
          "volume": "0",
          "openInterest": "1,248",
          "openInterestChange": "+2,108",
          "volatility": "511.86%",
          "delta": "0.9961",
          "rho": "0.0089",
          "vega": "0.0035",
          "theta": "-0.3087",
          "optionType": "Call",
          "daysToExpiration": "3",
          "tradeTime": "N/A",
          "averageVolatility": "24.94%",
          "historicVolatility30d": "20.25%",
          "baseNextEarningsDate": "10/29/26",
          "dividendExDate": "N/A",
          "baseTimeCode": "--",
          "expirationType": "weekly",
          "impliedVolatilityRank1y": "41.84%"
        }],
        "Put": [{
          "symbol": "AAPL|20260925|110.00P",
          "baseSymbol": "AAPL",
          "strikePrice": "110.00",
          "expirationDate": "09/25/26",
          "moneyness": "-67.67%",
          "bidPrice": "0.00",
          "midpoint": "0.01",
          "askPrice": "0.01",
          "lastPrice": "0.02",
          "priceChange": "+1.57",
          "percentChange": "+0.90%",
          "volume": "1,023",
          "openInterest": "100",
          "openInterestChange": "-281",
          "volatility": "363.74%",
          "delta": "-0.0002",
          "rho": "0.0000",
          "vega": "0.0002",
          "theta": "-0.0119",
          "optionType": "Put",
          "daysToExpiration": "3",
          "tradeTime": "12:02 ET",
          "averageVolatility": "24.94%",
          "historicVolatility30d": "20.25%",
          "baseNextEarningsDate": "10/29/26",
          "dividendExDate": "N/A",
          "baseTimeCode": "--",
          "expirationType": "weekly",
          "impliedVolatilityRank1y": "41.84%"
        }]
      }
    })";
    const auto page = terminal::parseMboumV3Options(json);
    CHECK_FALSE(page.no_data);
    CHECK(page.has_calendar);
    CHECK(page.calendar.size() == 3);
    CHECK(page.base_symbol == "AAPL");
    CHECK(page.next_earnings == 20261029);
    CHECK_FALSE(page.dividend_ex.has_value());
    CHECK_FALSE(page.earnings_time.has_value());
    REQUIRE(page.historic_vol_30d.has_value());
    CHECK(*page.historic_vol_30d == Catch::Approx(0.2025));
    REQUIRE(page.iv_rank_1y.has_value());
    CHECK(*page.iv_rank_1y == Catch::Approx(0.4184));
    REQUIRE(page.groups.size() == 1);
    CHECK(page.groups[0].expiration == 20260925);
    CHECK(page.groups[0].expiration_type == terminal::OptionExpirationType::Weekly);
    REQUIRE(page.groups[0].average_iv.has_value());
    CHECK(*page.groups[0].average_iv == Catch::Approx(0.2494));
    REQUIRE(page.groups[0].contracts.size() == 2);
    const auto& call = page.groups[0].contracts[0];
    CHECK(call.right == terminal::OptionRight::Call);
    CHECK(call.strike == Catch::Approx(110.0));
    CHECK(call.price_change == 0.0);
    CHECK(call.percent_change == 0.0);
    CHECK(call.open_interest == 1248);
    CHECK(call.open_interest_change == 2108);
    CHECK(call.implied_vol == Catch::Approx(5.1186));
    CHECK(call.moneyness == Catch::Approx(0.6767));
    CHECK_FALSE(call.trade_date.has_value());
    CHECK_FALSE(call.trade_minute.has_value());
    const auto& put = page.groups[0].contracts[1];
    CHECK(put.right == terminal::OptionRight::Put);
    CHECK(put.volume == 1023);
    CHECK(put.open_interest_change == -281);
    CHECK(put.percent_change == Catch::Approx(0.009));
    CHECK(put.trade_minute == (12 * 60) + 2);
    CHECK(put.bid == 0.0);

    const auto unknown = terminal::parseMboumV3Options(R"({"meta":{"expirations":[]},"body":[]})");
    CHECK(unknown.no_data);
    CHECK_FALSE(unknown.has_calendar);

    const auto calendar = terminal::parseMboumV3Options(
        R"({"meta":{"expirations":{"weekly":["2026-09-25"],"monthly":["2026-10-16"]}},"body":[]})");
    CHECK_FALSE(calendar.no_data);
    CHECK(calendar.has_calendar);
    CHECK(calendar.groups.empty());
    CHECK(calendar.calendar.size() == 2);

    const auto url = terminal::mboumV3OptionsUrl("$SPX", 20261016);
    CHECK(url.find("ticker=%24SPX") != std::string::npos);
    CHECK(url.find("expiration=2026-10-16") != std::string::npos);
    CHECK(terminal::mboumV3OptionsUrl("AAPL").find("expiration=") == std::string::npos);
}

TEST_CASE("option chain replace keeps the other slice and drops a date the calendar loses")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");

    terminal::OptionQuote call;
    call.instrument_id = id;
    call.expiration = 20260925;
    call.expiration_type = terminal::OptionExpirationType::Weekly;
    call.vendor_symbol = "AAPL|20260925|110.00C";
    call.strike = 110.0;
    call.right = terminal::OptionRight::Call;
    call.bid = 1.0;
    call.ask = 1.1;
    call.mid = 1.05;
    call.last = 1.0;
    call.implied_vol = 0.2;
    call.days_to_expiration = 3;
    call.fetched_at = 10;

    terminal::OptionQuote later = call;
    later.expiration = 20261016;
    later.expiration_type = terminal::OptionExpirationType::Monthly;
    later.vendor_symbol = "AAPL|20261016|110.00C";
    later.days_to_expiration = 24;

    terminal::OptionChainWrite write;
    write.instrument_id = id;
    write.fetched_at = 10;
    write.replace_calendar = true;
    write.has_underlying = true;
    write.underlying.instrument_id = id;
    write.underlying.fetched_at = 10;
    write.underlying.historic_vol_30d = 0.2025;
    write.underlying.next_earnings = 20261029;
    terminal::OptionExpiry weekly;
    weekly.expiration = 20260925;
    weekly.expiration_type = terminal::OptionExpirationType::Weekly;
    terminal::OptionExpiry monthly;
    monthly.expiration = 20261016;
    monthly.expiration_type = terminal::OptionExpirationType::Monthly;
    write.calendar = {weekly, monthly};
    terminal::OptionQuoteBatch near;
    near.expiration = 20260925;
    near.expiration_type = terminal::OptionExpirationType::Weekly;
    near.average_iv = 0.2494;
    near.quotes = {call};
    terminal::OptionQuoteBatch far;
    far.expiration = 20261016;
    far.expiration_type = terminal::OptionExpirationType::Monthly;
    far.average_iv = 0.1109;
    far.quotes = {later};
    write.batches = {near, far};
    store.replaceOptionChain(write);

    call.last = 2.0;
    near.quotes = {call};
    write.fetched_at = 11;
    write.underlying.fetched_at = 11;
    write.batches = {near};
    write.calendar = {weekly};
    store.replaceOptionChain(write);

    const auto quotes = store.queryOptionQuotes(id, 20260925, terminal::OptionExpirationType::Weekly);
    REQUIRE(quotes.size() == 1);
    CHECK(quotes[0].last == 2.0);
    CHECK(quotes[0].fetched_at == 11);
    CHECK(store.queryOptionQuotes(id, 20261016, terminal::OptionExpirationType::Monthly).empty());
    const auto expiries = store.queryOptionExpiries(id);
    REQUIRE(expiries.size() == 1);
    CHECK(expiries[0].expiration == 20260925);
    REQUIRE(expiries[0].average_iv.has_value());
    CHECK(*expiries[0].average_iv == Catch::Approx(0.2494));
    const auto underlying = store.findOptionUnderlying(id);
    REQUIRE(underlying.has_value());
    CHECK(underlying->next_earnings == 20261029);
    CHECK(underlying->fetched_at == 11);

    call.bid = -1.0;
    near.quotes = {call};
    write.batches = {near};
    CHECK_THROWS_AS(store.replaceOptionChain(write), std::runtime_error);
    CHECK(store.queryOptionQuotes(id, 20260925, terminal::OptionExpirationType::Weekly)[0].last == 2.0);
}

TEST_CASE("ingestOptions stores the vendor symbol and leaves an unknown ticker untouched")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi(true);
    constexpr std::string_view chain = R"({
      "meta": {"expirations": {"weekly": ["2026-09-25"], "monthly": ["2026-10-16", "2026-09-25"]}},
      "body": {
        "Call": [{
          "symbol": "$SPX|20260925|3200.00WC",
          "baseSymbol": "$SPX",
          "strikePrice": "3,200.00",
          "expirationDate": "09/25/26",
          "moneyness": "+58.81%",
          "bidPrice": "4,565.70",
          "midpoint": "4,575.00",
          "askPrice": "4,584.30",
          "lastPrice": "4,475.72",
          "priceChange": "unch",
          "percentChange": "unch",
          "volume": "0",
          "openInterest": "0",
          "openInterestChange": "unch",
          "volatility": "239.84%",
          "delta": "1.0000",
          "rho": "0.2629",
          "vega": "0.0004",
          "theta": "-0.3355",
          "optionType": "Call",
          "daysToExpiration": "3",
          "tradeTime": "N/A",
          "averageVolatility": "9.94%",
          "historicVolatility30d": "9.61%",
          "baseNextEarningsDate": "N/A",
          "dividendExDate": "N/A",
          "baseTimeCode": "--",
          "expirationType": "weekly",
          "impliedVolatilityRank1y": "7.19%"
        }],
        "Put": [{
          "symbol": "$SPX|20260925|3200.00C",
          "baseSymbol": "$SPX",
          "strikePrice": "3,200.00",
          "expirationDate": "09/25/26",
          "moneyness": "+58.81%",
          "bidPrice": "4,771.00",
          "midpoint": "4,780.00",
          "askPrice": "4,790.00",
          "lastPrice": "4,700.00",
          "priceChange": "unch",
          "percentChange": "unch",
          "volume": "0",
          "openInterest": "0",
          "openInterestChange": "unch",
          "volatility": "92.86%",
          "delta": "1.0000",
          "rho": "0.2600",
          "vega": "0.0004",
          "theta": "-0.3000",
          "optionType": "Call",
          "daysToExpiration": "3",
          "tradeTime": "09/21/26",
          "averageVolatility": "11.09%",
          "historicVolatility30d": "9.61%",
          "baseNextEarningsDate": "N/A",
          "dividendExDate": "N/A",
          "baseTimeCode": "--",
          "expirationType": "monthly",
          "impliedVolatilityRank1y": "7.19%"
        }]
      }
    })";
    auto get = [&](std::string_view url) {
        CHECK(std::string(url).find("ticker=SPX") != std::string::npos);
        CHECK(std::string(url).find("expiration=2026-09-25") != std::string::npos);
        terminal::HttpResponse response;
        response.status = 200;
        response.body = chain;
        return response;
    };
    const auto stored = terminal::ingestOptions(store, get, figi.client, "SPX", 20260925);
    CHECK(stored.symbol == "$SPX");
    CHECK(stored.quote_count == 2);
    CHECK_FALSE(stored.no_data);
    const auto instrument = store.findOpenListing("$SPX");
    REQUIRE(instrument.has_value());
    CHECK(instrument->asset_class == terminal::AssetClass::Index);
    CHECK(store.findOpenListing("SPX") == std::nullopt);
    const auto weekly = store.queryOptionQuotes(instrument->id, 20260925, terminal::OptionExpirationType::Weekly);
    const auto monthly = store.queryOptionQuotes(instrument->id, 20260925, terminal::OptionExpirationType::Monthly);
    REQUIRE(weekly.size() == 1);
    REQUIRE(monthly.size() == 1);
    CHECK(weekly[0].vendor_symbol == "$SPX|20260925|3200.00WC");
    CHECK(weekly[0].bid == Catch::Approx(4565.70));
    CHECK(monthly[0].vendor_symbol == "$SPX|20260925|3200.00C");
    const auto expiries = store.queryOptionExpiries(instrument->id);
    REQUIRE(expiries.size() == 3);
    REQUIRE(expiries[0].average_iv.has_value());
    CHECK(*expiries[0].average_iv == Catch::Approx(0.1109));

    auto missing = [](std::string_view) {
        terminal::HttpResponse response;
        response.status = 200;
        response.body = R"({"meta":{"expirations":[]},"body":[]})";
        return response;
    };
    const auto none = terminal::ingestOptions(store, missing, figi.client, "ZZZZ");
    CHECK(none.no_data);
    CHECK(store.findOpenListing("ZZZZ") == std::nullopt);
    CHECK(store.queryOptionQuotes(instrument->id, 20260925, terminal::OptionExpirationType::Weekly).size() == 1);
}

TEST_CASE("captured option chains fit the schema")
{
    const char* dir = std::getenv("MBOUM_OPTIONS_DIR");
    if (dir == nullptr || dir[0] == '\0')
    {
        return;
    }
    const std::filesystem::path root{dir};
    int parsed = 0;
    for (const auto& entry : std::filesystem::directory_iterator(root))
    {
        if (entry.path().extension() != ".json")
        {
            continue;
        }
        const auto page = terminal::parseMboumV3Options(readAll(entry.path()));
        const auto name = entry.path().filename().string();
        if (name == "noticker.json" || name == "bad.json")
        {
            CHECK(page.no_data);
        }
        else if (name == "aapl-bad-exp.json")
        {
            CHECK(page.has_calendar);
            CHECK(page.groups.empty());
            CHECK_FALSE(page.no_data);
        }
        else
        {
            CHECK_FALSE(page.groups.empty());
            CHECK(page.has_calendar);
            for (const auto& group : page.groups)
            {
                CHECK_FALSE(group.contracts.empty());
                REQUIRE(group.average_iv.has_value());
                CHECK(std::isfinite(*group.average_iv));
            }
        }
        if (name == "spx-monthly.json")
        {
            CHECK(page.groups.size() == 2);
            CHECK(page.groups[0].expiration == page.groups[1].expiration);
            CHECK(page.groups[0].expiration_type != page.groups[1].expiration_type);
            CHECK(page.base_symbol == "$SPX");
        }
        if (name == "spx-plain.json")
        {
            CHECK(page.base_symbol == "$SPX");
        }
        ++parsed;
    }
    CHECK(parsed >= 8);
}
