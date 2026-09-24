// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "FakeOpenFigi.h"
#include "catch_amalgamated.hpp"
#include "market_data/OpenFigi.h"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Parses one fixture response and returns its elements in request order.
std::vector<terminal::OpenFigiJobResult> fixtureResults(const char* stem)
{
    const nlohmann::json request = FakeOpenFigi::fixture(std::string(stem) + ".request.json");
    const std::string body = FakeOpenFigi::readFile(std::filesystem::path(TERMINAL_MARKET_DATA_FIXTURE_DIR) /
                                                    "openfigi" / (std::string(stem) + ".response.json"));
    auto parsed = terminal::parseOpenFigiMappingResponse(body, request.size());
    REQUIRE(parsed.has_value());
    return *parsed;
}

}  // namespace

TEST_CASE("forward fixture reduces to the recorded securities")
{
    const auto results = fixtureResults("forward_equity");
    REQUIRE(results.size() == 9);
    const auto aapl = terminal::reduceForward(results[0], false);
    CHECK(aapl.kind == terminal::ForwardKind::Confirmed);
    CHECK(aapl.figi == "BBG000B9XRY4");
    CHECK(aapl.asset_class == terminal::AssetClass::Equity);
    CHECK(aapl.name == "APPLE INC");
    const auto qqq = terminal::reduceForward(results[1], false);
    CHECK(qqq.figi == "BBG000BSWKH7");
    CHECK(qqq.asset_class == terminal::AssetClass::Etf);
    const auto brk = terminal::reduceForward(results[2], false);
    CHECK(brk.figi == "BBG000DWG505");
    CHECK(brk.asset_class == terminal::AssetClass::Equity);
    CHECK(terminal::reduceForward(results[3], false).figi == "BBG000MM2P62");
    const auto fb = terminal::reduceForward(results[4], false);
    CHECK(fb.figi == "BBG01VRMNFB1");
    CHECK(fb.asset_class == terminal::AssetClass::Etf);
    for (const std::size_t no_match : {5U, 6U, 7U, 8U})  // APPL, SPX as an index, BRK.B, BRK-B
    {
        CHECK(results[no_match].answer == terminal::OpenFigiAnswer::NoMatch);
        CHECK(results[no_match].message == "No identifier found.");
        CHECK(terminal::reduceForward(results[no_match], false).kind == terminal::ForwardKind::NoMatch);
    }
}

TEST_CASE("index fixtures need the Index ticker form")
{
    const auto shapes = fixtureResults("index_shapes");
    REQUIRE(shapes.size() == 8);
    CHECK(shapes[0].hits.size() == 60);  // plain SPX: Spirax listings
    CHECK(terminal::reduceForward(shapes[0], false).kind == terminal::ForwardKind::Ambiguous);
    CHECK(shapes[2].answer == terminal::OpenFigiAnswer::Unreachable);
    CHECK(shapes[2].message == "securityType2 required with BASE_TICKER(idType).");
    const auto spx = terminal::reduceForward(shapes[3], true);
    CHECK(spx.kind == terminal::ForwardKind::Confirmed);
    CHECK(spx.figi == "BBG000H4FSM0");
    CHECK(spx.asset_class == terminal::AssetClass::Index);
    CHECK(spx.name == "S&P 500 INDEX");

    const auto reverse = fixtureResults("reverse");
    CHECK(terminal::reduceForward(reverse[0], true).figi == "BBG000JW9B77");  // VIX
    CHECK(terminal::reduceForward(reverse[1], true).figi == "BBG000KKFC45");  // NDX
    // An equity-shaped job that returns an index is not accepted as an equity.
    CHECK(terminal::reduceForward(shapes[3], false).kind == terminal::ForwardKind::Unsupported);
}

TEST_CASE("reverse fixtures return the current or last ticker")
{
    const auto reverse = fixtureResults("reverse");
    REQUIRE(reverse.size() == 10);
    CHECK(reverse[4].hits.size() == 21);  // COMPOSITE_ID_BB_GLOBAL without exchCode: every venue
    const auto meta = terminal::reduceReverse(reverse[5]);
    CHECK(meta.kind == terminal::ReverseKind::Ticker);
    CHECK(meta.ticker == "META");
    const auto spx = terminal::reduceReverse(reverse[6]);
    CHECK(spx.ticker == "$SPX");
    CHECK(terminal::reduceReverse(reverse[9]).kind == terminal::ReverseKind::NoMatch);  // malformed FIGI

    const auto delisted = fixtureResults("delisted_reverse");
    const auto twtr = terminal::reduceReverse(delisted[0]);
    CHECK(twtr.kind == terminal::ReverseKind::Ticker);
    CHECK(twtr.ticker == "TWTR");

    const auto forward = fixtureResults("delisted_forward");
    CHECK(terminal::reduceForward(forward[2], false).kind == terminal::ForwardKind::NoMatch);  // TWTR
    CHECK(terminal::reduceForward(forward[3], false).kind == terminal::ForwardKind::NoMatch);  // ATVI
}

TEST_CASE("mapping body round-trips the job fields")
{
    const std::vector<terminal::OpenFigiJob> jobs = {terminal::forwardJob("brk.b"), terminal::forwardJob("$spx"),
                                                     terminal::reverseJob("BBG000MM2P62")};
    const nlohmann::json body = nlohmann::json::parse(terminal::buildOpenFigiMappingBody(jobs));
    REQUIRE(body.size() == 3);
    CHECK(body[0] == nlohmann::json({{"idType", "TICKER"}, {"idValue", "BRK/B"}, {"exchCode", "US"},
                                     {"marketSecDes", "Equity"}}));
    CHECK(body[1] == nlohmann::json({{"idType", "TICKER"}, {"idValue", "SPX Index"}, {"marketSecDes", "Index"}}));
    CHECK(body[2] == nlohmann::json({{"idType", "ID_BB_GLOBAL"}, {"idValue", "BBG000MM2P62"}}));
    CHECK(terminal::buildOpenFigiMappingBody(jobs).find("includeUnlistedEquities") == std::string::npos);
}

TEST_CASE("mapping response shape errors are unreachable, not answers")
{
    CHECK_FALSE(terminal::parseOpenFigiMappingResponse("not json", 1).has_value());
    CHECK_FALSE(terminal::parseOpenFigiMappingResponse("{}", 1).has_value());
    CHECK_FALSE(terminal::parseOpenFigiMappingResponse("[]", 1).has_value());
    CHECK_FALSE(terminal::parseOpenFigiMappingResponse(R"([{"data":[]},{"data":[]}])", 1).has_value());
    const auto odd = terminal::parseOpenFigiMappingResponse(R"([{"surprise":1}])", 1);
    REQUIRE(odd.has_value());
    CHECK(odd->front().answer == terminal::OpenFigiAnswer::Unreachable);
}

TEST_CASE("ticker translation between the store and OpenFIGI")
{
    CHECK(terminal::toOpenFigiTicker("BRK.B") == "BRK/B");
    CHECK(terminal::fromOpenFigiTicker("BRK/B") == "BRK.B");
    CHECK(terminal::toOpenFigiTicker("$SPX") == "SPX Index");
    CHECK(terminal::fromOpenFigiTicker("SPX Index") == "$SPX");
    CHECK(terminal::toOpenFigiTicker("aapl") == "AAPL");
    CHECK(terminal::sameTicker("meta", "META"));
    CHECK_FALSE(terminal::sameTicker("META", "MET"));
    CHECK(terminal::isIndexSymbol("$SPX"));
    CHECK_FALSE(terminal::isIndexSymbol("SPX"));
}

TEST_CASE("rate-limit headers from the captured response")
{
    std::istringstream lines(
        FakeOpenFigi::readFile(std::filesystem::path(TERMINAL_MARKET_DATA_FIXTURE_DIR) / "openfigi" /
                               "ratelimit.headers.txt"));
    terminal::HttpResponse response;
    std::string line;
    while (std::getline(lines, line))
    {
        const auto colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }
        std::string value = line.substr(colon + 1);
        while (!value.empty() && (value.front() == ' '))
        {
            value.erase(value.begin());
        }
        while (!value.empty() && (value.back() == '\r' || value.back() == ' '))
        {
            value.pop_back();
        }
        response.headers.emplace_back(line.substr(0, colon), value);
    }
    const auto limit = terminal::parseOpenFigiRateLimit(response);
    CHECK(limit.limit == 25);
    CHECK(limit.window_s == 60);
    CHECK(limit.remaining == 24);
    CHECK(limit.reset_s == 60);
}

TEST_CASE("client batches 10 jobs without a key and 100 with one")
{
    std::vector<terminal::OpenFigiJob> jobs;
    for (int i = 0; i < 25; ++i)
    {
        jobs.push_back(terminal::forwardJob("T" + std::to_string(i)));
    }
    FakeOpenFigiClient keyless;
    CHECK(keyless.client.batchSize() == 10);
    CHECK(keyless.client.map(jobs).size() == 25);
    CHECK(keyless.fake.requests == 3);
    FakeOpenFigiClient keyed(false, true);
    CHECK(keyed.client.batchSize() == 100);
    CHECK(keyed.client.map(jobs).size() == 25);
    CHECK(keyed.fake.requests == 1);
}

TEST_CASE("client waits on 429 and gives up after three retries")
{
    FakeOpenFigiClient figi;
    figi.fake.throttle(2);
    CHECK(figi.client.forward("AAPL").kind == terminal::ForwardKind::Confirmed);
    CHECK(figi.fake.requests == 3);
    REQUIRE(figi.slept.size() == 2);  // one ratelimit-reset wait per 429
    CHECK(figi.slept[0] == std::chrono::seconds(7));
    CHECK(figi.slept[1] == std::chrono::seconds(7));

    FakeOpenFigiClient hopeless;
    hopeless.fake.throttle(10);
    const auto outcome = hopeless.client.forward("AAPL");
    CHECK(outcome.kind == terminal::ForwardKind::Unreachable);
    CHECK(hopeless.fake.requests == 4);
}

TEST_CASE("client maps transport and HTTP failures to unreachable and throws on 413")
{
    FakeOpenFigiClient figi;
    figi.fake.failWith(0);
    CHECK(figi.client.forward("AAPL").kind == terminal::ForwardKind::Unreachable);
    figi.fake.failWith(500);
    CHECK(figi.client.reverse("BBG000B9XRY4").kind == terminal::ReverseKind::Unreachable);
    figi.fake.failWith(413);
    CHECK_THROWS_AS((void)figi.client.forward("AAPL"), std::runtime_error);
    figi.fake.recover();
    CHECK(figi.client.forward("AAPL").figi == "BBG000B9XRY4");
}

TEST_CASE("only an empty data array or the warning is no match")
{
    terminal::OpenFigiJobResult empty;
    empty.answer = terminal::OpenFigiAnswer::NoMatch;
    CHECK(terminal::reduceForward(empty, false).kind == terminal::ForwardKind::NoMatch);
    CHECK(terminal::reduceReverse(empty).kind == terminal::ReverseKind::NoMatch);

    const auto parsed = terminal::parseOpenFigiMappingResponse(
        R"([{"data":[]},{"data":[{"name":"X","figi":"","compositeFIGI":null},7]},{"data":[{"figi":"BBG000B9XRY4"}]}])", 3);
    REQUIRE(parsed.has_value());
    CHECK(terminal::reduceForward((*parsed)[0], false).kind == terminal::ForwardKind::NoMatch);
    const auto blank = terminal::reduceForward((*parsed)[1], false);
    CHECK(blank.kind == terminal::ForwardKind::Unreachable);
    CHECK(blank.message == "OpenFIGI returned hits without a FIGI");
    const auto no_ticker = terminal::reduceReverse((*parsed)[2]);
    CHECK(no_ticker.kind == terminal::ReverseKind::Unreachable);
    CHECK(no_ticker.message == "OpenFIGI returned hits without a ticker");
}

TEST_CASE("an equity job never keys on a venue FIGI")
{
    const auto parsed = terminal::parseOpenFigiMappingResponse(
        R"([
          {"data":[{"figi":"BBG000BLNNV0","compositeFIGI":null,"ticker":"IBM","name":"IBM","exchCode":"UN",
                    "marketSector":"Equity","securityType":"Common Stock"}]},
          {"data":[{"figi":"BBG000BLNNH6","compositeFIGI":"BBG000BLNNH6","ticker":"IBM","name":"IBM","exchCode":"US",
                    "marketSector":"Equity","securityType":"Common Stock"},
                   {"figi":"BBG000BLNNV0","compositeFIGI":null,"ticker":"IBM","name":"IBM","exchCode":"UN",
                    "marketSector":"Equity","securityType":"Common Stock"}]}
        ])",
        2);
    REQUIRE(parsed.has_value());
    // A venue row alone is not a composite: unreadable, not a key and not "not listed".
    const auto venue_only = terminal::reduceForward((*parsed)[0], false);
    CHECK(venue_only.kind == terminal::ForwardKind::Unreachable);
    CHECK(venue_only.figi.empty());
    // A composite row beside a venue row without a composite is one security, not two.
    const auto mixed = terminal::reduceForward((*parsed)[1], false);
    CHECK(mixed.kind == terminal::ForwardKind::Confirmed);
    CHECK(mixed.figi == "BBG000BLNNH6");
}
