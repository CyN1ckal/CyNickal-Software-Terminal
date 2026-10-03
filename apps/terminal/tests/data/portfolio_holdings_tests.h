// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "data/PortfolioHoldings.h"

#include <string>
#include <vector>

namespace {

terminal::PortfolioHolding shareLine(std::string figi, double quantity, terminal::PortfolioAssetKind kind)
{
    terminal::PortfolioHolding holding;
    holding.kind = kind;
    holding.figi = std::move(figi);
    holding.quantity = quantity;
    return holding;
}

terminal::PortfolioHolding cashLine(double quantity)
{
    terminal::PortfolioHolding holding;
    holding.kind = terminal::PortfolioAssetKind::Cash;
    holding.quantity = quantity;
    return holding;
}

terminal::Instrument listing(std::string symbol, std::string figi, terminal::AssetClass asset_class,
                             terminal::InstrumentId id)
{
    terminal::Instrument instrument;
    instrument.id = id;
    instrument.symbol = std::move(symbol);
    instrument.figi = std::move(figi);
    instrument.asset_class = asset_class;
    instrument.listing_open = true;
    return instrument;
}

terminal::PortfolioHolding optionLine(std::string figi,
                                      terminal::SessionDate expiration,
                                      terminal::OptionExpirationType expiration_type,
                                      double strike,
                                      terminal::OptionRight right,
                                      double quantity)
{
    terminal::PortfolioHolding holding;
    holding.kind = terminal::PortfolioAssetKind::Option;
    holding.figi = std::move(figi);
    holding.expiration = expiration;
    holding.expiration_type = expiration_type;
    holding.strike = strike;
    holding.right = right;
    holding.quantity = quantity;
    return holding;
}

}  // namespace

TEST_CASE("a second share of the same instrument combines into one line")
{
    std::vector<terminal::PortfolioHolding> book = {
        shareLine("BBG000BB5006", 10.0, terminal::PortfolioAssetKind::Equity),
        shareLine("BBG000BDTBL9", 4.0, terminal::PortfolioAssetKind::Equity),
    };
    book[0].symbol = "ADBE";
    book[0].instrument_id = 7;

    terminal::PortfolioHolding more = shareLine("BBG000BB5006", 2.5, terminal::PortfolioAssetKind::Equity);
    more.symbol = "ADBE";
    const terminal::HoldingAggregate combined = terminal::aggregateHolding(book, more);

    CHECK(combined.action == terminal::HoldingAggregateAction::Combined);
    CHECK(combined.index == 0);
    REQUIRE(book.size() == 2);
    CHECK(book[0].quantity == 12.5);
    CHECK(book[0].kind == terminal::PortfolioAssetKind::Equity);
    REQUIRE(book[0].symbol.has_value());
    CHECK(*book[0].symbol == "ADBE");
    CHECK(book[1].quantity == 4.0);

    terminal::PortfolioHolding as_etf = shareLine("BBG000BB5006", 1.0, terminal::PortfolioAssetKind::Etf);
    const terminal::HoldingAggregate still_one = terminal::aggregateHolding(book, as_etf);
    CHECK(still_one.action == terminal::HoldingAggregateAction::Combined);
    CHECK(book.size() == 2);
    CHECK(book[0].quantity == 13.5);
    CHECK(book[0].kind == terminal::PortfolioAssetKind::Equity);
}

TEST_CASE("offsetting the whole position removes that line")
{
    std::vector<terminal::PortfolioHolding> book = {
        shareLine("BBG000BB5006", 10.0, terminal::PortfolioAssetKind::Equity),
        shareLine("BBG000BDTBL9", 4.0, terminal::PortfolioAssetKind::Equity),
    };
    const terminal::HoldingAggregate removed =
        terminal::aggregateHolding(book, shareLine("BBG000BB5006", -10.0, terminal::PortfolioAssetKind::Equity));
    CHECK(removed.action == terminal::HoldingAggregateAction::Removed);
    CHECK(removed.index == 0);
    REQUIRE(book.size() == 1);
    REQUIRE(book[0].figi.has_value());
    CHECK(*book[0].figi == "BBG000BDTBL9");
}

TEST_CASE("the same ticker on a different instrument stays its own line")
{
    std::vector<terminal::PortfolioHolding> book = {
        shareLine("BBG000BB5006", 10.0, terminal::PortfolioAssetKind::Equity)};
    book[0].symbol = "ADBE";
    terminal::PortfolioHolding other = shareLine("BBG000OTHER1", 3.0, terminal::PortfolioAssetKind::Equity);
    other.symbol = "ADBE";
    const terminal::HoldingAggregate added = terminal::aggregateHolding(book, other);
    CHECK(added.action == terminal::HoldingAggregateAction::Added);
    CHECK(added.index == 1);
    CHECK(book.size() == 2);
}

TEST_CASE("a share and an option on that share are different positions")
{
    std::vector<terminal::PortfolioHolding> book = {
        shareLine("BBG000BB5006", 100.0, terminal::PortfolioAssetKind::Equity)};
    const terminal::HoldingAggregate added = terminal::aggregateHolding(
        book, optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly, 500.0,
                         terminal::OptionRight::Call, 1.0));
    CHECK(added.action == terminal::HoldingAggregateAction::Added);
    CHECK(book.size() == 2);
}

TEST_CASE("option contracts combine only when the contract matches")
{
    std::vector<terminal::PortfolioHolding> book = {
        optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly, 500.0,
                   terminal::OptionRight::Call, 1.0),
        optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly, 520.0,
                   terminal::OptionRight::Call, -2.0),
    };

    terminal::PortfolioHolding same = optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly,
                                                 500.00005, terminal::OptionRight::Call, 3.0);
    same.vendor_symbol = "ADBE  260918C00500000";
    const terminal::HoldingAggregate combined = terminal::aggregateHolding(book, same);
    CHECK(combined.action == terminal::HoldingAggregateAction::Combined);
    CHECK(combined.index == 0);
    REQUIRE(book.size() == 2);
    CHECK(book[0].quantity == 4.0);
    REQUIRE(book[0].vendor_symbol.has_value());
    CHECK(*book[0].vendor_symbol == "ADBE  260918C00500000");
    CHECK(book[0].strike == 500.0);

    terminal::PortfolioHolding other_vendor =
        optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly, 500.0,
                   terminal::OptionRight::Call, 1.0);
    other_vendor.vendor_symbol = "OTHER";
    CHECK(terminal::aggregateHolding(book, other_vendor).action == terminal::HoldingAggregateAction::Combined);
    REQUIRE(book[0].vendor_symbol.has_value());
    CHECK(*book[0].vendor_symbol == "ADBE  260918C00500000");
    CHECK(book[0].quantity == 5.0);

    CHECK(terminal::aggregateHolding(book, optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly,
                                                      500.001, terminal::OptionRight::Call, 1.0))
              .action == terminal::HoldingAggregateAction::Added);
    CHECK(terminal::aggregateHolding(book, optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly,
                                                      500.0, terminal::OptionRight::Put, 1.0))
              .action == terminal::HoldingAggregateAction::Added);
    CHECK(terminal::aggregateHolding(book, optionLine("BBG000BB5006", 20261016, terminal::OptionExpirationType::Monthly,
                                                      500.0, terminal::OptionRight::Call, 1.0))
              .action == terminal::HoldingAggregateAction::Added);
    CHECK(terminal::aggregateHolding(book, optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Weekly,
                                                      500.0, terminal::OptionRight::Call, 1.0))
              .action == terminal::HoldingAggregateAction::Added);
    CHECK(book.size() == 6);
}

TEST_CASE("cash combines into the one cash line")
{
    std::vector<terminal::PortfolioHolding> book = {
        shareLine("BBG000BB5006", 10.0, terminal::PortfolioAssetKind::Equity),
        cashLine(25.0),
    };
    const terminal::HoldingAggregate combined = terminal::aggregateHolding(book, cashLine(-5.0));
    CHECK(combined.action == terminal::HoldingAggregateAction::Combined);
    CHECK(combined.index == 1);
    REQUIRE(book.size() == 2);
    CHECK(book[1].quantity == 20.0);

    CHECK(terminal::aggregateHolding(book, cashLine(-20.0)).action == terminal::HoldingAggregateAction::Removed);
    REQUIRE(book.size() == 1);
    CHECK(book[0].kind == terminal::PortfolioAssetKind::Equity);
}

TEST_CASE("holdings with no figi combine on the instrument id")
{
    terminal::PortfolioHolding first;
    first.kind = terminal::PortfolioAssetKind::Equity;
    first.instrument_id = 11;
    first.quantity = 2.0;
    terminal::PortfolioHolding second = first;
    second.quantity = 3.0;
    second.symbol = "ADBE";
    second.listing_open = true;

    std::vector<terminal::PortfolioHolding> book = {first};
    CHECK(terminal::aggregateHolding(book, second).action == terminal::HoldingAggregateAction::Combined);
    REQUIRE(book.size() == 1);
    CHECK(book[0].quantity == 5.0);
    REQUIRE(book[0].symbol.has_value());
    CHECK(*book[0].symbol == "ADBE");
    CHECK(book[0].listing_open);
}

TEST_CASE("aggregateHoldings keeps the first line of each position")
{
    const std::vector<terminal::PortfolioHolding> raw = {
        shareLine("BBG000BB5006", 10.0, terminal::PortfolioAssetKind::Equity),
        shareLine("BBG000BDTBL9", 2.0, terminal::PortfolioAssetKind::Equity),
        shareLine("BBG000BB5006", -3.0, terminal::PortfolioAssetKind::Equity),
        cashLine(5.0),
        cashLine(1.0),
        shareLine("BBG000BB5006", 3.0, terminal::PortfolioAssetKind::Equity),
        optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly, 500.0,
                   terminal::OptionRight::Call, 1.0),
        optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly, 500.0,
                   terminal::OptionRight::Call, 2.0),
    };
    const std::vector<terminal::PortfolioHolding> book = terminal::aggregateHoldings(raw);
    REQUIRE(book.size() == 4);
    CHECK(book[0].quantity == 10.0);
    CHECK(book[1].quantity == 2.0);
    CHECK(book[2].kind == terminal::PortfolioAssetKind::Cash);
    CHECK(book[2].quantity == 6.0);
    CHECK(book[3].kind == terminal::PortfolioAssetKind::Option);
    CHECK(book[3].quantity == 3.0);
}

TEST_CASE("retargeting a share keeps its quantity and place")
{
    std::vector<terminal::PortfolioHolding> book = {
        shareLine("BBG000BB5006", 10.0, terminal::PortfolioAssetKind::Equity),
        shareLine("BBG000BDTBL9", 4.0, terminal::PortfolioAssetKind::Etf),
    };
    book[0].symbol = "AAPL";
    book[0].instrument_id = 1;
    book[1].symbol = "QQQ";

    const terminal::HoldingRetarget moved =
        terminal::retargetHolding(book, 0, listing("MSFT", "BBG000BPH459", terminal::AssetClass::Equity, 9));
    CHECK(moved.action == terminal::HoldingRetargetAction::Updated);
    CHECK(moved.index == 0);
    REQUIRE(book.size() == 2);
    CHECK(book[0].quantity == 10.0);
    CHECK(book[0].kind == terminal::PortfolioAssetKind::Equity);
    REQUIRE(book[0].symbol.has_value());
    CHECK(*book[0].symbol == "MSFT");
    REQUIRE(book[0].figi.has_value());
    CHECK(*book[0].figi == "BBG000BPH459");
    CHECK(book[0].instrument_id == 9);
    CHECK(book[0].listing_open);
    REQUIRE(book[1].symbol.has_value());
    CHECK(*book[1].symbol == "QQQ");
    CHECK(book[1].quantity == 4.0);
}

TEST_CASE("retargeting a share onto an existing line keeps one quantity")
{
    std::vector<terminal::PortfolioHolding> book = {
        shareLine("BBG000BB5006", 10.0, terminal::PortfolioAssetKind::Equity),
        shareLine("BBG000BPH459", 4.0, terminal::PortfolioAssetKind::Equity),
    };
    book[0].symbol = "AAPL";
    book[0].instrument_id = 1;
    book[1].instrument_id = 9;

    const terminal::HoldingRetarget combined =
        terminal::retargetHolding(book, 0, listing("MSFT", "BBG000BPH459", terminal::AssetClass::Equity, 9));
    CHECK(combined.action == terminal::HoldingRetargetAction::Combined);
    CHECK(combined.index == 0);
    REQUIRE(book.size() == 1);
    CHECK(book[0].quantity == 14.0);
    REQUIRE(book[0].symbol.has_value());
    CHECK(*book[0].symbol == "MSFT");
    REQUIRE(book[0].figi.has_value());
    CHECK(*book[0].figi == "BBG000BPH459");

    book = {
        shareLine("BBG000BB5006", 10.0, terminal::PortfolioAssetKind::Equity),
        shareLine("BBG000BPH459", -10.0, terminal::PortfolioAssetKind::Equity),
    };
    const terminal::HoldingRetarget removed =
        terminal::retargetHolding(book, 0, listing("MSFT", "BBG000BPH459", terminal::AssetClass::Equity, 9));
    CHECK(removed.action == terminal::HoldingRetargetAction::Removed);
    CHECK(book.empty());

    book = {
        shareLine("BBG000BPH459", 4.0, terminal::PortfolioAssetKind::Equity),
        shareLine("BBG000BB5006", 10.0, terminal::PortfolioAssetKind::Equity),
    };
    book[0].symbol = "MSFT";
    const terminal::HoldingRetarget earlier =
        terminal::retargetHolding(book, 1, listing("MSFT", "BBG000BPH459", terminal::AssetClass::Equity, 9));
    CHECK(earlier.action == terminal::HoldingRetargetAction::Combined);
    CHECK(earlier.index == 0);
    REQUIRE(book.size() == 1);
    CHECK(book[0].quantity == 14.0);
    REQUIRE(book[0].symbol.has_value());
    CHECK(*book[0].symbol == "MSFT");
}

TEST_CASE("retargeting a share follows the resolved asset class")
{
    std::vector<terminal::PortfolioHolding> book = {
        shareLine("BBG000BB5006", 8.0, terminal::PortfolioAssetKind::Equity),
    };
    book[0].symbol = "AAPL";

    const terminal::HoldingRetarget as_etf =
        terminal::retargetHolding(book, 0, listing("SPY", "BBG000BDTBL9", terminal::AssetClass::Etf, 3));
    CHECK(as_etf.action == terminal::HoldingRetargetAction::Updated);
    REQUIRE(book.size() == 1);
    CHECK(book[0].kind == terminal::PortfolioAssetKind::Etf);
    CHECK(book[0].quantity == 8.0);
    REQUIRE(book[0].symbol.has_value());
    CHECK(*book[0].symbol == "SPY");

    const terminal::HoldingRetarget as_equity =
        terminal::retargetHolding(book, 0, listing("AAPL", "BBG000BB5006", terminal::AssetClass::Equity, 1));
    CHECK(as_equity.action == terminal::HoldingRetargetAction::Updated);
    CHECK(book[0].kind == terminal::PortfolioAssetKind::Equity);
    CHECK(book[0].quantity == 8.0);
    REQUIRE(book[0].symbol.has_value());
    CHECK(*book[0].symbol == "AAPL");
}

TEST_CASE("retargeting rejects an asset class the line cannot hold")
{
    std::vector<terminal::PortfolioHolding> book = {
        shareLine("BBG000BB5006", 8.0, terminal::PortfolioAssetKind::Equity),
    };
    book[0].symbol = "AAPL";
    book[0].instrument_id = 1;

    const terminal::HoldingRetarget index =
        terminal::retargetHolding(book, 0, listing("$SPX", "BBG000H4FSM0", terminal::AssetClass::Index, 4));
    CHECK(index.action == terminal::HoldingRetargetAction::Rejected);
    REQUIRE(book.size() == 1);
    CHECK(book[0].quantity == 8.0);
    REQUIRE(book[0].symbol.has_value());
    CHECK(*book[0].symbol == "AAPL");
    CHECK(book[0].instrument_id == 1);

    terminal::Instrument missing = listing("MSFT", "BBG000BPH459", terminal::AssetClass::Equity, 9);
    missing.figi.reset();
    CHECK(terminal::retargetHolding(book, 0, missing).action == terminal::HoldingRetargetAction::Rejected);
    CHECK(book[0].quantity == 8.0);

    book.push_back(cashLine(25.0));
    CHECK(terminal::retargetHolding(book, 1, listing("MSFT", "BBG000BPH459", terminal::AssetClass::Equity, 9)).action ==
          terminal::HoldingRetargetAction::Rejected);
    CHECK(book[1].kind == terminal::PortfolioAssetKind::Cash);
    CHECK(book[1].quantity == 25.0);

    CHECK(terminal::retargetHolding(book, 4, listing("MSFT", "BBG000BPH459", terminal::AssetClass::Equity, 9)).action ==
          terminal::HoldingRetargetAction::Rejected);
    CHECK(book.size() == 2);

    book[0] = optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly, 500.0,
                         terminal::OptionRight::Call, 2.0);
    book[0].symbol = "AAPL";
    const terminal::HoldingRetarget future =
        terminal::retargetHolding(book, 0, listing("ES", "BBG000FUTURE", terminal::AssetClass::Future, 12));
    CHECK(future.action == terminal::HoldingRetargetAction::Rejected);
    CHECK(book[0].quantity == 2.0);
    REQUIRE(book[0].symbol.has_value());
    CHECK(*book[0].symbol == "AAPL");
}

TEST_CASE("retargeting an option keeps the contract and the quantity")
{
    std::vector<terminal::PortfolioHolding> book = {
        optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly, 500.0,
                   terminal::OptionRight::Call, 2.0),
        shareLine("BBG000BDTBL9", 4.0, terminal::PortfolioAssetKind::Etf),
    };
    book[0].symbol = "AAPL";
    book[0].instrument_id = 1;
    book[0].vendor_symbol = "AAPL  260918C00500000";
    book[0].listing_open = true;

    const terminal::HoldingRetarget moved =
        terminal::retargetHolding(book, 0, listing("MSFT", "BBG000BPH459", terminal::AssetClass::Equity, 9));
    CHECK(moved.action == terminal::HoldingRetargetAction::Updated);
    CHECK(moved.index == 0);
    REQUIRE(book.size() == 2);
    CHECK(book[0].kind == terminal::PortfolioAssetKind::Option);
    CHECK(book[0].quantity == 2.0);
    REQUIRE(book[0].symbol.has_value());
    CHECK(*book[0].symbol == "MSFT");
    REQUIRE(book[0].figi.has_value());
    CHECK(*book[0].figi == "BBG000BPH459");
    CHECK(book[0].expiration == 20260918);
    CHECK(book[0].expiration_type == terminal::OptionExpirationType::Monthly);
    CHECK(book[0].strike == 500.0);
    CHECK(book[0].right == terminal::OptionRight::Call);
    CHECK_FALSE(book[0].vendor_symbol.has_value());
    CHECK(book[1].quantity == 4.0);

    book[0].vendor_symbol = "MSFT  260918C00500000";
    const terminal::HoldingRetarget same =
        terminal::retargetHolding(book, 0, listing("MSFT", "BBG000BPH459", terminal::AssetClass::Equity, 9));
    CHECK(same.action == terminal::HoldingRetargetAction::Updated);
    CHECK(book[0].quantity == 2.0);
    REQUIRE(book[0].vendor_symbol.has_value());
    CHECK(*book[0].vendor_symbol == "MSFT  260918C00500000");

    const terminal::HoldingRetarget index =
        terminal::retargetHolding(book, 0, listing("$SPX", "BBG000H4FSM0", terminal::AssetClass::Index, 4));
    CHECK(index.action == terminal::HoldingRetargetAction::Updated);
    CHECK(book[0].kind == terminal::PortfolioAssetKind::Option);
    CHECK(book[0].quantity == 2.0);
    REQUIRE(book[0].symbol.has_value());
    CHECK(*book[0].symbol == "$SPX");
    CHECK_FALSE(book[0].vendor_symbol.has_value());

    std::vector<terminal::PortfolioHolding> contracts = {
        optionLine("BBG000BB5006", 20260918, terminal::OptionExpirationType::Monthly, 500.0,
                   terminal::OptionRight::Call, 1.0),
        optionLine("BBG000BPH459", 20260918, terminal::OptionExpirationType::Monthly, 500.0,
                   terminal::OptionRight::Call, 3.0),
    };
    const terminal::HoldingRetarget folded =
        terminal::retargetHolding(contracts, 0, listing("MSFT", "BBG000BPH459", terminal::AssetClass::Equity, 9));
    CHECK(folded.action == terminal::HoldingRetargetAction::Combined);
    REQUIRE(contracts.size() == 1);
    CHECK(contracts[0].quantity == 4.0);
    REQUIRE(contracts[0].figi.has_value());
    CHECK(*contracts[0].figi == "BBG000BPH459");
    CHECK(contracts[0].strike == 500.0);
}
