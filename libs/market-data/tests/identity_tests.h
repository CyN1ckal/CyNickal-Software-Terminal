// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "FakeOpenFigi.h"
#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/Figi.h"
#include "market_data/Identity.h"
#include "market_data/MboumIngest.h"
#include "market_data/Store.h"
#include "market_data/Time.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr terminal::UnixSeconds kIdentityNow = 2000000000;
constexpr terminal::UnixSeconds kDay = 24 * 60 * 60;

terminal::InstrumentId insertKnown(terminal::Store& store,
                                   const std::string& symbol,
                                   const std::string& figi,
                                   std::optional<terminal::UnixSeconds> verified_at,
                                   const std::string& name = {})
{
    terminal::Instrument inst;
    inst.symbol = symbol;
    inst.figi = figi;
    inst.verified_at = verified_at;
    if (!name.empty())
    {
        inst.name = name;
    }
    return store.insertInstrument(inst, kIdentityNow - (30 * kDay));
}

std::string refusal(terminal::Store& store, terminal::OpenFigiClient& figi, const std::string& symbol)
{
    try
    {
        (void)terminal::ensureInstrument(store, figi, symbol, kIdentityNow);
    }
    catch (const std::runtime_error& ex)
    {
        return ex.what();
    }
    return "accepted";
}

terminal::Bar identityBar(terminal::InstrumentId id)
{
    terminal::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = terminal::kTimeframe1d;
    bar.ts = 1600000000;
    bar.open = 10;
    bar.high = 11;
    bar.low = 9;
    bar.close = 10;
    bar.volume = 100;
    return bar;
}

}  // namespace

TEST_CASE("a new ticker inserts the recorded FIGI, class, and name")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const auto aapl = terminal::ensureInstrument(store, figi.client, "AAPL", kIdentityNow);
    CHECK(aapl.notice.empty());
    const auto row = store.findInstrumentById(aapl.id);
    REQUIRE(row.has_value());
    CHECK(row->figi == "BBG000B9XRY4");
    CHECK(row->asset_class == terminal::AssetClass::Equity);
    CHECK(row->name == "APPLE INC");
    CHECK(row->verified_at == kIdentityNow);

    const auto qqq = terminal::ensureInstrument(store, figi.client, "QQQ", kIdentityNow);
    CHECK(store.findInstrumentById(qqq.id)->asset_class == terminal::AssetClass::Etf);
    const auto spx = terminal::ensureInstrument(store, figi.client, "$SPX", kIdentityNow);
    const auto index = store.findInstrumentById(spx.id);
    CHECK(index->figi == "BBG000H4FSM0");
    CHECK(index->asset_class == terminal::AssetClass::Index);
    CHECK(index->symbol == "$SPX");
    const auto brk = terminal::ensureInstrument(store, figi.client, "BRK.B", kIdentityNow);
    CHECK(store.findInstrumentById(brk.id)->figi == "BBG000DWG505");
}

TEST_CASE("refused tickers write nothing")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    CHECK(refusal(store, figi.client, "APPL") == "APPL has no US listing in OpenFIGI");
    CHECK(refusal(store, figi.client, "TWTR") == "TWTR has no US listing in OpenFIGI");
    CHECK(refusal(store, figi.client, "BRK-B") == "BRK-B has no US listing in OpenFIGI");
    figi.fake.set(terminal::forwardJob("SPX"), FakeOpenFigi::fixture("index_shapes.response.json")[0]);
    CHECK(refusal(store, figi.client, "SPX").find("more than one security") != std::string::npos);
    figi.fake.set(terminal::forwardJob("NDX"), FakeOpenFigi::fixture("reverse.response.json")[1]);
    CHECK(refusal(store, figi.client, "NDX").find("not an equity, ETF, or index") != std::string::npos);
    figi.fake.failWith(0);
    CHECK(refusal(store, figi.client, "AAPL").find("OpenFIGI unreachable") != std::string::npos);
    figi.fake.recover();
    figi.fake.throttle(10);
    CHECK(refusal(store, figi.client, "AAPL").find("OpenFIGI unreachable") != std::string::npos);
    CHECK(store.listInstruments().empty());
}

TEST_CASE("a verified listing makes no OpenFIGI request")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const auto id = insertKnown(store, "AAPL", "BBG000B9XRY4", kIdentityNow - kDay + 60);
    CHECK(terminal::ensureInstrument(store, figi.client, "AAPL", kIdentityNow).id == id);
    CHECK(figi.fake.requests == 0);
}

TEST_CASE("a stale listing that still maps is re-verified with one request")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const auto id = insertKnown(store, "AAPL", "BBG000B9XRY4", kIdentityNow - (2 * kDay));
    const auto resolved = terminal::ensureInstrument(store, figi.client, "AAPL", kIdentityNow);
    CHECK(resolved.id == id);
    CHECK(resolved.notice.empty());
    CHECK(figi.fake.requests == 1);
    CHECK(store.findInstrumentById(id)->verified_at == kIdentityNow);
}

TEST_CASE("Facebook's FB row moves to META and FB becomes the ProShares ETF")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const auto meta_id = insertKnown(store, "FB", "BBG000MM2P62", kIdentityNow - (2 * kDay), "FACEBOOK INC-A");
    REQUIRE(store.upsertBars(std::vector<terminal::Bar>{identityBar(meta_id)}).written == 1);

    const auto resolved = terminal::ensureInstrument(store, figi.client, "FB", kIdentityNow);
    CHECK(resolved.id != meta_id);
    CHECK(resolved.notice == "FB now names PROSHARES S&P DYNAMIC BUFFER; the former FB trades as META");
    const auto etf = store.findInstrumentById(resolved.id);
    CHECK(etf->figi == "BBG01VRMNFB1");
    CHECK(etf->asset_class == terminal::AssetClass::Etf);
    CHECK(store.queryBars(resolved.id, terminal::kTimeframe1d, 0, 4000000000).empty());

    const auto meta = store.findInstrumentById(meta_id);
    CHECK(meta->symbol == "META");
    CHECK(meta->listing_open);
    CHECK(store.queryBars(meta_id, terminal::kTimeframe1d, 0, 4000000000).size() == 1);
    const auto history = store.listingHistory(meta_id);
    REQUIRE(history.size() == 2);
    CHECK(history[0].close_reason == terminal::ListingCloseReason::Renamed);
}

TEST_CASE("an old ticker that maps to nothing is refused with the new ticker")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    figi.fake.setNoMatch(terminal::forwardJob("FB"));
    const auto meta_id = insertKnown(store, "FB", "BBG000MM2P62", kIdentityNow - (2 * kDay));
    CHECK(refusal(store, figi.client, "FB") == "FB is no longer listed; that security now trades as META");
    CHECK(store.listInstruments().size() == 1);
    CHECK(store.findOpenListing("META")->id == meta_id);
    // A second attempt has no open FB listing left and gives the same answer.
    CHECK(refusal(store, figi.client, "FB") == "FB is no longer listed; that security now trades as META");
}

TEST_CASE("Twitter's listing closes as delisted")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const auto id = insertKnown(store, "TWTR", "BBG000H6HNW3", kIdentityNow - (2 * kDay));
    CHECK(refusal(store, figi.client, "TWTR") == "TWTR is delisted");
    const auto row = store.findInstrumentById(id);
    CHECK_FALSE(row->listing_open);
    CHECK(row->delisted_at == kIdentityNow);
    CHECK(store.listingHistory(id)[0].close_reason == terminal::ListingCloseReason::Delisted);
}

TEST_CASE("a rename to a ticker that is itself gone closes as delisted")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const std::string old_figi = terminal::testingFigiFor("OLDCO");
    const auto id = insertKnown(store, "OLD", old_figi, std::nullopt);
    figi.fake.setReverse(old_figi, "NEW");
    const std::vector<terminal::InstrumentId> ids = {id};
    const auto reports = terminal::verifyIdentities(store, figi.client, kIdentityNow, ids);
    REQUIRE(reports.size() == 1);
    CHECK(reports[0].outcome == terminal::VerifyOutcome::Delisted);
    CHECK(reports[0].detail == "renamed to NEW, which is no longer listed");
    CHECK_FALSE(store.findOpenListing("NEW").has_value());
    CHECK_FALSE(store.findInstrumentById(id)->listing_open);
}

TEST_CASE("a FIGI OpenFIGI no longer knows is a conflict")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const auto id = insertKnown(store, "GONE", terminal::testingFigiFor("GONE"), kIdentityNow - (2 * kDay));
    const std::string refused = refusal(store, figi.client, "GONE");
    CHECK(refused.find("identity conflict") != std::string::npos);
    CHECK(store.findOpenListing("GONE")->id == id);
    CHECK_FALSE(store.findInstrumentById(id)->verified_at.has_value());
}

TEST_CASE("OpenFIGI outages are tolerated for 7 days after the last verification")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    figi.fake.failWith(0);
    const auto recent = insertKnown(store, "AAPL", "BBG000B9XRY4", kIdentityNow - (6 * kDay));
    const auto resolved = terminal::ensureInstrument(store, figi.client, "AAPL", kIdentityNow);
    CHECK(resolved.id == recent);
    CHECK(resolved.notice.starts_with("warning: could not confirm AAPL with OpenFIGI"));
    CHECK(store.findInstrumentById(recent)->verified_at == kIdentityNow - (6 * kDay));

    (void)insertKnown(store, "QQQ", "BBG000BSWKH7", kIdentityNow - (8 * kDay));
    CHECK(refusal(store, figi.client, "QQQ").starts_with("cannot confirm QQQ: OpenFIGI unreachable"));
    (void)insertKnown(store, "META", "BBG000MM2P62", std::nullopt);
    CHECK(refusal(store, figi.client, "META") == "cannot confirm META: OpenFIGI unreachable, last verified never");
}

TEST_CASE("two instruments that swap tickers resolve in one run")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const std::string fa = terminal::testingFigiFor("A-CO");
    const std::string fb = terminal::testingFigiFor("B-CO");
    const auto a = insertKnown(store, "AAA", fa, std::nullopt);
    const auto b = insertKnown(store, "BBB", fb, std::nullopt);
    figi.fake.setForward("AAA", fb, "B CO");
    figi.fake.setForward("BBB", fa, "A CO");
    figi.fake.setReverse(fa, "BBB");
    figi.fake.setReverse(fb, "AAA");
    const std::vector<terminal::InstrumentId> ids = {a, b};
    const auto reports = terminal::verifyIdentities(store, figi.client, kIdentityNow, ids);
    REQUIRE(reports.size() == 2);
    CHECK(terminal::describe(reports[0]) == "renamed AAA -> BBB");
    CHECK(terminal::describe(reports[1]) == "renamed BBB -> AAA");
    CHECK(store.findOpenListing("BBB")->id == a);
    CHECK(store.findOpenListing("AAA")->id == b);
    CHECK(figi.fake.requests == 3);  // forward, reverse, confirm; both instruments in each batch
}

TEST_CASE("a rename onto a ticker another instrument still holds is a conflict")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const std::string fa = terminal::testingFigiFor("A-CO");
    const auto a = insertKnown(store, "AAA", fa, std::nullopt);
    const auto c = store.testingInsertInstrument("CCC");
    figi.fake.setReverse(fa, "CCC");
    figi.fake.setForward("CCC", fa, "A CO");
    const std::vector<terminal::InstrumentId> ids = {a};
    const auto reports = terminal::verifyIdentities(store, figi.client, kIdentityNow, ids);
    CHECK(reports[0].outcome == terminal::VerifyOutcome::Conflict);
    CHECK(reports[0].detail == "renamed to CCC, which is open on another instrument");
    CHECK(store.findOpenListing("CCC")->id == c);
    CHECK_FALSE(store.findInstrumentById(a)->listing_open);
    CHECK_FALSE(store.findInstrumentById(a)->verified_at.has_value());
    CHECK(refusal(store, figi.client, "AAA").find("identity conflict") != std::string::npos);
}

TEST_CASE("a new ticker whose FIGI is stored under another ticker relinks it")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const auto id = insertKnown(store, "FB", "BBG000MM2P62", kIdentityNow);
    const auto resolved = terminal::ensureInstrument(store, figi.client, "META", kIdentityNow);
    CHECK(resolved.id == id);
    CHECK(resolved.notice == "META: FB was renamed to META");
    CHECK(store.listInstruments().size() == 1);
    CHECK(store.findOpenListing("META")->id == id);
}

TEST_CASE("a new ticker whose FIGI is stored with no open listing reopens it")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const auto id = insertKnown(store, "META", "BBG000MM2P62", kIdentityNow);
    store.closeListing(id, kIdentityNow - kDay, terminal::ListingCloseReason::Manual);
    const auto resolved = terminal::ensureInstrument(store, figi.client, "META", kIdentityNow);
    CHECK(resolved.id == id);
    CHECK(store.findOpenListing("META")->id == id);
    CHECK(store.listingHistory(id).size() == 2);
}

TEST_CASE("instruments due for verification skip fresh and closed rows")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto fresh = insertKnown(store, "AAPL", "BBG000B9XRY4", kIdentityNow - 60);
    const auto stale = insertKnown(store, "QQQ", "BBG000BSWKH7", kIdentityNow - (2 * kDay));
    const auto never = insertKnown(store, "META", "BBG000MM2P62", std::nullopt);
    const auto closed = insertKnown(store, "TWTR", "BBG000H6HNW3", std::nullopt);
    store.closeListing(closed, kIdentityNow, terminal::ListingCloseReason::Delisted);
    CHECK(terminal::instrumentsDueForVerification(store, kIdentityNow, false) ==
          std::vector<terminal::InstrumentId>{stale, never});
    CHECK(terminal::instrumentsDueForVerification(store, kIdentityNow, true) ==
          std::vector<terminal::InstrumentId>{fresh, stale, never});
}

TEST_CASE("a forward mismatch with a failed reverse lookup refuses inside the grace window")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const terminal::UnixSeconds verified = kIdentityNow - (2 * kDay);
    const auto meta_id = insertKnown(store, "FB", "BBG000MM2P62", verified);
    // FB forward-maps to the ProShares ETF (fixture); the reverse lookup of Meta's FIGI times out.
    figi.fake.set(terminal::reverseJob("BBG000MM2P62"), nlohmann::json{{"error", "timeout"}});

    const std::vector<terminal::InstrumentId> ids = {meta_id};
    const auto reports = terminal::verifyIdentities(store, figi.client, kIdentityNow, ids);
    CHECK(reports[0].outcome == terminal::VerifyOutcome::Unresolved);
    CHECK(terminal::describe(reports[0]) ==
          "unresolved: FB no longer maps to BBG000MM2P62; the reverse lookup failed (timeout)");

    CHECK(refusal(store, figi.client, "FB") ==
          "cannot confirm FB: FB no longer maps to BBG000MM2P62; the reverse lookup failed (timeout)");
    const auto row = store.findInstrumentById(meta_id);
    CHECK(row->symbol == "FB");
    CHECK(row->listing_open);
    CHECK(row->verified_at == verified);
}

TEST_CASE("a refused ticker gets no MBoum bars appended")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const auto meta_id = insertKnown(store, "FB", "BBG000MM2P62", terminal::nowUtc() - (2 * kDay));
    figi.fake.set(terminal::reverseJob("BBG000MM2P62"), nlohmann::json{{"error", "timeout"}});
    int gets = 0;
    auto get = [&gets](std::string_view) {
        ++gets;
        return terminal::HttpResponse{};
    };
    CHECK_THROWS_AS(terminal::ingestDailySymbol(store, get, figi.client, "FB", 20250115, 20250115),
                    std::runtime_error);
    CHECK(gets == 0);
    CHECK(store.queryBars(meta_id, terminal::kTimeframe1d, 0, 4000000000).empty());
}

TEST_CASE("a failed confirmation of the new ticker is unresolved, not a grace pass")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const terminal::UnixSeconds verified = kIdentityNow - (2 * kDay);
    const auto meta_id = insertKnown(store, "FB", "BBG000MM2P62", verified);
    figi.fake.set(terminal::forwardJob("META"), nlohmann::json{{"error", "rate limited"}});
    CHECK(refusal(store, figi.client, "FB") ==
          "cannot confirm FB: FB no longer maps to BBG000MM2P62; confirming META failed (rate limited)");
    CHECK(store.findOpenListing("FB")->id == meta_id);
    CHECK(store.findInstrumentById(meta_id)->verified_at == verified);
}

TEST_CASE("hits without a FIGI are unreachable and never delist")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const auto id = insertKnown(store, "TWTR", "BBG000H6HNW3", kIdentityNow - (2 * kDay));
    // The reverse fixture still names TWTR, so a NoMatch forward here would delist it.
    figi.fake.set(terminal::forwardJob("TWTR"),
                  nlohmann::json{{"data", nlohmann::json::array({nlohmann::json{{"name", "TWITTER"}}, 5})}});
    const std::vector<terminal::InstrumentId> ids = {id};
    const auto reports = terminal::verifyIdentities(store, figi.client, kIdentityNow, ids);
    CHECK(reports[0].outcome == terminal::VerifyOutcome::Unreachable);
    CHECK(reports[0].detail == "OpenFIGI returned hits without a FIGI");
    const auto row = store.findInstrumentById(id);
    CHECK(row->listing_open);
    CHECK_FALSE(row->delisted_at.has_value());
    const auto resolved = terminal::ensureInstrument(store, figi.client, "TWTR", kIdentityNow);
    CHECK(resolved.id == id);
    CHECK(resolved.notice.starts_with("warning: could not confirm TWTR"));
}

TEST_CASE("listing symbols are stored uppercase")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    FakeOpenFigiClient figi;
    const auto aapl = terminal::ensureInstrument(store, figi.client, "aapl", kIdentityNow);
    CHECK(store.findInstrumentById(aapl.id)->symbol == "AAPL");
    const auto spx = terminal::ensureInstrument(store, figi.client, "$spx", kIdentityNow);
    CHECK(store.findInstrumentById(spx.id)->symbol == "$SPX");
}
