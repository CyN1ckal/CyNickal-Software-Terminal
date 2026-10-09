// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "trading/Ledger.h"

#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace {

constexpr terminal::InstrumentId kLedgerAapl = 1;
constexpr terminal::InstrumentId kLedgerMsft = 2;

terminal::TradeFill lotFill(terminal::TradeFillId id,
                            terminal::InstrumentId instrument,
                            terminal::UnixSeconds ts,
                            double quantity,
                            double price,
                            double fees = 0.0)
{
    terminal::TradeFill fill;
    fill.id = id;
    fill.kind = terminal::TradeAssetKind::Equity;
    fill.instrument_id = instrument;
    fill.symbol = instrument == kLedgerAapl ? "AAPL" : "MSFT";
    fill.ts = ts;
    fill.quantity = quantity;
    fill.price = price;
    fill.fees = fees;
    return fill;
}

terminal::TradeFill lotOptionFill(terminal::TradeFillId id,
                                  terminal::UnixSeconds ts,
                                  double strike,
                                  terminal::OptionRight right,
                                  double quantity,
                                  double price,
                                  double fees = 0.0)
{
    terminal::TradeFill fill = lotFill(id, kLedgerAapl, ts, quantity, price, fees);
    fill.kind = terminal::TradeAssetKind::Option;
    fill.expiration = 20261016;
    fill.expiration_type = terminal::OptionExpirationType::Monthly;
    fill.strike = strike;
    fill.right = right;
    return fill;
}

terminal::CorporateAction lotSplit(terminal::InstrumentId instrument, terminal::UnixSeconds ex_ts, double ratio)
{
    terminal::CorporateAction action;
    action.instrument_id = instrument;
    action.ex_ts = ex_ts;
    action.type = terminal::CorporateActionType::Split;
    action.split_ratio = ratio;
    return action;
}

terminal::CorporateAction lotDividend(terminal::InstrumentId instrument, terminal::UnixSeconds ex_ts, double amount)
{
    terminal::CorporateAction action;
    action.instrument_id = instrument;
    action.ex_ts = ex_ts;
    action.type = terminal::CorporateActionType::Dividend;
    action.amount = amount;
    return action;
}

constexpr terminal::UnixSeconds kLedgerEnd = 1'000'000;

}  // namespace

TEST_CASE("a buy and a full sell make one round trip with both fees")
{
    const std::vector<terminal::TradeFill> fills{
        lotFill(1, kLedgerAapl, 100, 10, 100.0, 1.0),
        lotFill(2, kLedgerAapl, 200, -10, 110.0, 1.5),
    };
    const auto book = terminal::matchLots(fills, {}, kLedgerEnd);
    CHECK(book.open_lots.empty());
    CHECK(book.positions.empty());
    REQUIRE(book.round_trips.size() == 1);
    const auto& trip = book.round_trips[0];
    CHECK(trip.symbol == "AAPL");
    CHECK(trip.open_fill_id == 1);
    CHECK(trip.close_fill_id == 2);
    CHECK(trip.opened_at == 100);
    CHECK(trip.closed_at == 200);
    CHECK(trip.quantity == 10);
    CHECK(trip.entry_price == 100.0);
    CHECK(trip.exit_price == 110.0);
    CHECK(trip.multiplier == 1.0);
    CHECK(trip.gross_pnl == Catch::Approx(100.0));
    CHECK(trip.fees == Catch::Approx(2.5));
    CHECK(trip.net_pnl == Catch::Approx(97.5));
    CHECK(book.realized_pnl == Catch::Approx(97.5));
    CHECK(book.fees_paid == Catch::Approx(2.5));
    CHECK(book.trade_cash == Catch::Approx(-1001.0 + 1098.5));
}

TEST_CASE("a partial sell closes the oldest lots first and splits their fees")
{
    const std::vector<terminal::TradeFill> fills{
        lotFill(1, kLedgerAapl, 100, 10, 100.0, 1.0),
        lotFill(2, kLedgerAapl, 200, 10, 110.0, 1.0),
        lotFill(3, kLedgerAapl, 300, -15, 120.0, 1.5),
    };
    const auto book = terminal::matchLots(fills, {}, kLedgerEnd);
    REQUIRE(book.round_trips.size() == 2);
    CHECK(book.round_trips[0].open_fill_id == 1);
    CHECK(book.round_trips[0].quantity == 10);
    CHECK(book.round_trips[0].gross_pnl == Catch::Approx(200.0));
    CHECK(book.round_trips[0].fees == Catch::Approx(1.0 + 1.0));
    CHECK(book.round_trips[1].open_fill_id == 2);
    CHECK(book.round_trips[1].quantity == 5);
    CHECK(book.round_trips[1].gross_pnl == Catch::Approx(50.0));
    CHECK(book.round_trips[1].fees == Catch::Approx(0.5 + 0.5));
    CHECK(book.realized_pnl == Catch::Approx(198.0 + 49.0));

    REQUIRE(book.open_lots.size() == 1);
    CHECK(book.open_lots[0].fill_id == 2);
    CHECK(book.open_lots[0].quantity == Catch::Approx(5.0));
    CHECK(book.open_lots[0].price == 110.0);
    CHECK(book.open_lots[0].fees == Catch::Approx(0.5));
    CHECK(book.fees_paid == Catch::Approx(3.5));
}

TEST_CASE("a short is covered with a positive profit when the price falls")
{
    const std::vector<terminal::TradeFill> fills{
        lotFill(1, kLedgerAapl, 100, -5, 50.0),
        lotFill(2, kLedgerAapl, 200, 5, 40.0),
    };
    const auto book = terminal::matchLots(fills, {}, kLedgerEnd);
    REQUIRE(book.round_trips.size() == 1);
    CHECK(book.round_trips[0].quantity == -5);
    CHECK(book.round_trips[0].entry_price == 50.0);
    CHECK(book.round_trips[0].exit_price == 40.0);
    CHECK(book.round_trips[0].gross_pnl == Catch::Approx(50.0));
    CHECK(book.open_lots.empty());
}

TEST_CASE("one fill can close a long and open a short")
{
    const std::vector<terminal::TradeFill> fills{
        lotFill(1, kLedgerAapl, 100, 10, 10.0),
        lotFill(2, kLedgerAapl, 200, -15, 12.0, 3.0),
    };
    const auto book = terminal::matchLots(fills, {}, kLedgerEnd);
    REQUIRE(book.round_trips.size() == 1);
    CHECK(book.round_trips[0].quantity == 10);
    CHECK(book.round_trips[0].gross_pnl == Catch::Approx(20.0));
    CHECK(book.round_trips[0].fees == Catch::Approx(2.0));
    REQUIRE(book.open_lots.size() == 1);
    CHECK(book.open_lots[0].fill_id == 2);
    CHECK(book.open_lots[0].quantity == Catch::Approx(-5.0));
    CHECK(book.open_lots[0].price == 12.0);
    CHECK(book.open_lots[0].fees == Catch::Approx(1.0));
    REQUIRE(book.positions.size() == 1);
    CHECK(book.positions[0].quantity == Catch::Approx(-5.0));
    CHECK(book.positions[0].cost_basis == Catch::Approx(-60.0));
}

TEST_CASE("a split rescales the lot held across it")
{
    const std::vector<terminal::TradeFill> fills{
        lotFill(1, kLedgerAapl, 100, 10, 400.0),
        lotFill(2, kLedgerAapl, 300, -40, 110.0),
    };
    // A 4-for-1 on AAPL between the fills. The MSFT split never touches AAPL.
    const std::vector<terminal::CorporateAction> actions{lotSplit(kLedgerAapl, 200, 4.0),
                                                         lotSplit(kLedgerMsft, 150, 2.0)};
    const auto book = terminal::matchLots(fills, actions, kLedgerEnd);
    REQUIRE(book.round_trips.size() == 1);
    CHECK(book.round_trips[0].quantity == Catch::Approx(40.0));
    CHECK(book.round_trips[0].entry_price == Catch::Approx(100.0));
    CHECK(book.round_trips[0].gross_pnl == Catch::Approx(400.0));
    CHECK(book.open_lots.empty());
}

TEST_CASE("dividend cash is share quantity times amount after splits applied by then")
{
    const auto buy = lotFill(1, kLedgerAapl, 100, 10, 80.0);
    const std::vector<terminal::CorporateAction> before{
        lotSplit(kLedgerAapl, 300, 4.0),
        lotDividend(kLedgerAapl, 200, 2.0),
        lotDividend(kLedgerMsft, 200, 9.0),
    };
    terminal::LotBook early(before);
    early.apply(buy);
    early.advanceTo(200);
    CHECK(early.dividendCash() == Catch::Approx(20.0));
    early.advanceTo(400);
    CHECK(early.dividendCash() == Catch::Approx(20.0));

    // The split at the dividend's own ex_ts is applied first, so the quantity is post-split.
    const std::vector<terminal::CorporateAction> together{
        lotDividend(kLedgerAapl, 200, 2.0),
        lotSplit(kLedgerAapl, 200, 4.0),
    };
    terminal::LotBook same_time(together);
    same_time.apply(buy);
    same_time.advanceTo(200);
    CHECK(same_time.dividendCash() == Catch::Approx(80.0));

    terminal::CorporateAction missing = lotDividend(kLedgerAapl, 200, 2.0);
    missing.amount.reset();
    terminal::CorporateAction not_finite = lotDividend(kLedgerAapl, 200, 2.0);
    not_finite.amount = std::numeric_limits<double>::quiet_NaN();
    const std::vector<terminal::CorporateAction> ignored{
        lotDividend(kLedgerAapl, 200, -1.0), missing, not_finite,
    };
    terminal::LotBook skipped(ignored);
    skipped.apply(buy);
    skipped.advanceTo(200);
    CHECK(skipped.dividendCash() == 0.0);

    const std::vector<terminal::CorporateAction> on_shares{lotDividend(kLedgerAapl, 200, 2.0)};
    terminal::LotBook option_only(on_shares);
    option_only.apply(lotOptionFill(1, 100, 100.0, terminal::OptionRight::Call, 1, 5.0));
    option_only.advanceTo(200);
    CHECK(option_only.dividendCash() == 0.0);
}

TEST_CASE("a split on the fill's own timestamp is applied before the fill")
{
    const std::vector<terminal::TradeFill> fills{
        lotFill(1, kLedgerAapl, 100, 10, 400.0),
        lotFill(2, kLedgerAapl, 200, -40, 100.0),
    };
    const std::vector<terminal::CorporateAction> actions{lotSplit(kLedgerAapl, 200, 4.0)};
    const auto book = terminal::matchLots(fills, actions, kLedgerEnd);
    CHECK(book.open_lots.empty());
    REQUIRE(book.round_trips.size() == 1);
    CHECK(book.round_trips[0].gross_pnl == Catch::Approx(0.0));
}

TEST_CASE("as_of carries later splits to open lots, and earlier splits leave new lots alone")
{
    const std::vector<terminal::TradeFill> fills{lotFill(1, kLedgerAapl, 100, 3, 90.0)};
    const std::vector<terminal::CorporateAction> actions{
        lotSplit(kLedgerAapl, 50, 10.0),  // before the buy
        lotSplit(kLedgerAapl, 500, 2.0),  // after the buy
    };
    const auto before = terminal::matchLots(fills, actions, 400);
    REQUIRE(before.positions.size() == 1);
    CHECK(before.positions[0].quantity == Catch::Approx(3.0));
    CHECK(before.positions[0].average_price == Catch::Approx(90.0));

    const auto after = terminal::matchLots(fills, actions, 500);
    REQUIRE(after.positions.size() == 1);
    CHECK(after.positions[0].quantity == Catch::Approx(6.0));
    CHECK(after.positions[0].average_price == Catch::Approx(45.0));
    CHECK(after.positions[0].cost_basis == Catch::Approx(270.0));
}

TEST_CASE("option lots use the contract multiplier and key on the contract")
{
    const std::vector<terminal::TradeFill> fills{
        lotOptionFill(1, 100, 100.0, terminal::OptionRight::Call, 1, 5.0, 0.65),
        lotOptionFill(2, 100, 110.0, terminal::OptionRight::Call, -1, 2.0, 0.65),
        lotOptionFill(3, 200, 100.0, terminal::OptionRight::Call, -1, 8.0, 0.65),
        lotFill(4, kLedgerAapl, 200, 100, 105.0),
    };
    // A split does not rescale an option lot.
    const std::vector<terminal::CorporateAction> actions{lotSplit(kLedgerAapl, 150, 2.0)};
    const auto book = terminal::matchLots(fills, actions, kLedgerEnd);
    REQUIRE(book.round_trips.size() == 1);
    CHECK(book.round_trips[0].key.kind == terminal::TradeAssetKind::Option);
    CHECK(book.round_trips[0].key.strike == 100.0);
    CHECK(book.round_trips[0].multiplier == 100.0);
    CHECK(book.round_trips[0].gross_pnl == Catch::Approx(300.0));
    CHECK(book.round_trips[0].net_pnl == Catch::Approx(300.0 - 1.3));

    // The short 110 call and the 100 shares stay open as separate positions.
    REQUIRE(book.positions.size() == 2);
    const auto& shares = book.positions[0];
    const auto& call = book.positions[1];
    CHECK(shares.key.kind == terminal::TradeAssetKind::Equity);
    CHECK(shares.quantity == Catch::Approx(100.0));
    CHECK(call.key.kind == terminal::TradeAssetKind::Option);
    CHECK(call.key.strike == 110.0);
    CHECK(call.quantity == Catch::Approx(-1.0));
    CHECK(call.average_price == Catch::Approx(2.0));
    CHECK(call.multiplier == 100.0);
    CHECK(terminal::unrealizedPnl(call, 0.5) == Catch::Approx(150.0));
    CHECK(terminal::fillCashFlow(fills[1]) == Catch::Approx(200.0 - 0.65));
}

TEST_CASE("fills are matched in time order and ties keep their input order")
{
    const std::vector<terminal::TradeFill> fills{
        lotFill(3, kLedgerAapl, 300, -1, 30.0),
        lotFill(1, kLedgerAapl, 100, 1, 10.0),
        lotFill(2, kLedgerAapl, 100, 1, 20.0),
    };
    const auto book = terminal::matchLots(fills, {}, kLedgerEnd);
    REQUIRE(book.round_trips.size() == 1);
    CHECK(book.round_trips[0].open_fill_id == 1);
    REQUIRE(book.open_lots.size() == 1);
    CHECK(book.open_lots[0].fill_id == 2);
}

TEST_CASE("positions average their lots and fractional dust closes")
{
    const std::vector<terminal::TradeFill> averaged{
        lotFill(1, kLedgerMsft, 100, 10, 100.0, 1.0),
        lotFill(2, kLedgerMsft, 200, 30, 120.0, 2.0),
    };
    const auto book = terminal::matchLots(averaged, {}, kLedgerEnd);
    REQUIRE(book.positions.size() == 1);
    CHECK(book.positions[0].symbol == "MSFT");
    CHECK(book.positions[0].lots == 2);
    CHECK(book.positions[0].quantity == Catch::Approx(40.0));
    CHECK(book.positions[0].average_price == Catch::Approx(115.0));
    CHECK(book.positions[0].open_fees == Catch::Approx(3.0));
    CHECK(terminal::unrealizedPnl(book.positions[0], 125.0) == Catch::Approx(400.0));

    const std::vector<terminal::TradeFill> fractional{
        lotFill(1, kLedgerAapl, 100, 0.1, 10.0),
        lotFill(2, kLedgerAapl, 200, 0.2, 10.0),
        lotFill(3, kLedgerAapl, 300, -0.3, 10.0),
    };
    const auto dust = terminal::matchLots(fractional, {}, kLedgerEnd);
    CHECK(dust.open_lots.empty());
    CHECK(dust.round_trips.size() == 2);
}

TEST_CASE("a lot book rejects bad fills and time that moves backward")
{
    terminal::LotBook book;
    auto no_instrument = lotFill(1, kLedgerAapl, 100, 1, 10.0);
    no_instrument.instrument_id.reset();
    CHECK_THROWS_WITH(book.apply(no_instrument), "ledger fill has no instrument");

    auto nan_quantity = lotFill(1, kLedgerAapl, 100, 1, 10.0);
    nan_quantity.quantity = std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS_WITH(book.apply(nan_quantity), "ledger fill quantity is zero or not finite");

    auto negative_fees = lotFill(1, kLedgerAapl, 100, 1, 10.0, -1.0);
    CHECK_THROWS_WITH(book.apply(negative_fees), "ledger fill fees are negative or not finite");

    auto no_contract = lotOptionFill(1, 100, 100.0, terminal::OptionRight::Put, 1, 1.0);
    no_contract.strike.reset();
    CHECK_THROWS_WITH(book.apply(no_contract), "ledger option fill has no contract");

    book.apply(lotFill(1, kLedgerAapl, 200, 1, 10.0));
    CHECK(book.time() == 200);
    CHECK_THROWS_WITH(book.apply(lotFill(2, kLedgerAapl, 199, -1, 10.0)), "ledger time moved backward");
    CHECK_THROWS_WITH(book.advanceTo(100), "ledger time moved backward");
    CHECK(book.openLots().size() == 1);
}

TEST_CASE("trade cash plus open value equals realized plus unrealized less open fees")
{
    const std::vector<terminal::TradeFill> fills{
        lotFill(1, kLedgerAapl, 100, 10, 100.0, 1.0),
        lotFill(2, kLedgerMsft, 110, -4, 300.0, 0.5),
        lotFill(3, kLedgerAapl, 120, -3, 104.0, 0.25),
        lotFill(4, kLedgerAapl, 130, -12, 98.0, 1.25),
        lotFill(5, kLedgerMsft, 140, 6, 290.0, 0.75),
        lotOptionFill(6, 150, 95.0, terminal::OptionRight::Put, 2, 1.2, 1.3),
        lotFill(7, kLedgerAapl, 160, 4, 99.0),
        lotOptionFill(8, 170, 95.0, terminal::OptionRight::Put, -1, 2.0, 0.65),
    };
    const std::vector<terminal::CorporateAction> actions{lotSplit(kLedgerMsft, 125, 3.0)};
    const auto book = terminal::matchLots(fills, actions, kLedgerEnd);

    const std::map<terminal::InstrumentId, double> share_marks{{kLedgerAapl, 101.0}, {kLedgerMsft, 97.0}};
    constexpr double kPutMark = 1.5;
    double open_value = 0.0;
    double unrealized = 0.0;
    double open_fees = 0.0;
    for (const auto& position : book.positions)
    {
        const double mark =
            position.key.kind == terminal::TradeAssetKind::Option ? kPutMark : share_marks.at(position.key.instrument_id);
        open_value += position.quantity * mark * position.multiplier;
        unrealized += terminal::unrealizedPnl(position, mark);
        open_fees += position.open_fees;
    }
    CHECK(book.trade_cash + open_value == Catch::Approx(book.realized_pnl + unrealized - open_fees));
    CHECK(book.fees_paid == Catch::Approx(1.0 + 0.5 + 0.25 + 1.25 + 0.75 + 1.3 + 0.65));
}
