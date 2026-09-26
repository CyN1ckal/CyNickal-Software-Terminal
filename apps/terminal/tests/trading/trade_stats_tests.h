// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "trading/EquityCurve.h"
#include "trading/Ledger.h"
#include "trading/TradeStats.h"

#include <cmath>
#include <vector>

namespace {

constexpr terminal::UnixSeconds kStatsDay = 86'400;

terminal::RoundTrip statsTrip(double net, double fees, terminal::UnixSeconds held)
{
    terminal::RoundTrip trip;
    trip.opened_at = 1'000;
    trip.closed_at = 1'000 + held;
    trip.net_pnl = net;
    trip.fees = fees;
    trip.gross_pnl = net + fees;
    return trip;
}

terminal::EquityPoint statsPoint(terminal::UnixSeconds ts, double equity, double period_return, double growth,
                                 double drawdown, std::size_t open_positions)
{
    terminal::EquityPoint point;
    point.ts = ts;
    point.equity = equity;
    point.contributed = 1'000.0;
    point.period_return = period_return;
    point.growth = growth;
    point.drawdown = drawdown;
    point.open_positions = open_positions;
    return point;
}

}  // namespace

TEST_CASE("trade stats of no trips are empty")
{
    const auto stats = terminal::tradeStats({});
    CHECK(stats.trades == 0);
    CHECK(stats.win_rate == 0.0);
    CHECK_FALSE(stats.profit_factor.has_value());
    CHECK_FALSE(stats.payoff_ratio.has_value());
}

TEST_CASE("trade stats count wins, losses, streaks, and averages on net profit")
{
    const std::vector<terminal::RoundTrip> trips{
        statsTrip(100.0, 1.0, 100), statsTrip(-50.0, 1.0, 200), statsTrip(30.0, 1.0, 300),
        statsTrip(0.0, 1.0, 400),   statsTrip(-20.0, 1.0, 500), statsTrip(-10.0, 1.0, 600),
    };
    const auto stats = terminal::tradeStats(trips);
    CHECK(stats.trades == 6);
    CHECK(stats.wins == 2);
    CHECK(stats.losses == 3);
    CHECK(stats.breakeven == 1);
    CHECK(stats.win_rate == Catch::Approx(2.0 / 6.0));
    CHECK(stats.net_profit == Catch::Approx(50.0));
    CHECK(stats.gross_profit == Catch::Approx(130.0));
    CHECK(stats.gross_loss == Catch::Approx(-80.0));
    CHECK(stats.fees == Catch::Approx(6.0));
    CHECK(stats.average_trade == Catch::Approx(50.0 / 6.0));
    CHECK(stats.average_win == Catch::Approx(65.0));
    CHECK(stats.average_loss == Catch::Approx(-80.0 / 3.0));
    CHECK(stats.largest_win == Catch::Approx(100.0));
    CHECK(stats.largest_loss == Catch::Approx(-50.0));
    REQUIRE(stats.profit_factor.has_value());
    CHECK(*stats.profit_factor == Catch::Approx(130.0 / 80.0));
    REQUIRE(stats.payoff_ratio.has_value());
    CHECK(*stats.payoff_ratio == Catch::Approx(65.0 / (80.0 / 3.0)));
    // W L W B L L: a breakeven trip ends both runs.
    CHECK(stats.max_consecutive_wins == 1);
    CHECK(stats.max_consecutive_losses == 2);
    CHECK(stats.average_holding_seconds == Catch::Approx(350.0));
}

TEST_CASE("only winners leave the profit factor empty")
{
    const std::vector<terminal::RoundTrip> trips{statsTrip(5.0, 0.0, 1), statsTrip(7.0, 0.0, 1)};
    const auto stats = terminal::tradeStats(trips);
    CHECK(stats.max_consecutive_wins == 2);
    CHECK_FALSE(stats.profit_factor.has_value());
    CHECK_FALSE(stats.payoff_ratio.has_value());
}

TEST_CASE("curve stats annualize time-weighted returns")
{
    const std::vector<terminal::EquityPoint> curve{
        statsPoint(0, 1'000.0, 0.0, 1.0, 0.0, 0),
        statsPoint(kStatsDay, 1'100.0, 0.1, 1.1, 0.0, 1),
        statsPoint(2 * kStatsDay, 990.0, -0.1, 0.99, -0.1, 1),
        statsPoint(3 * kStatsDay, 1'188.0, 0.2, 1.188, 0.0, 0),
    };
    const auto stats = terminal::curveStats(curve);
    CHECK(stats.periods == 4);
    CHECK(stats.total_return == Catch::Approx(0.188));
    CHECK(stats.net_pnl == Catch::Approx(188.0));
    CHECK(stats.max_drawdown == Catch::Approx(-0.1));
    CHECK(stats.max_drawdown_seconds == 2 * kStatsDay);
    CHECK(stats.exposure == Catch::Approx(0.5));

    // Returns 0, .1, -.1, .2: mean .05, sample deviation sqrt(.05 / 3).
    const double deviation = std::sqrt(0.05 / 3.0);
    const double annualize = std::sqrt(252.0);
    REQUIRE(stats.volatility.has_value());
    CHECK(*stats.volatility == Catch::Approx(deviation * annualize));
    REQUIRE(stats.sharpe.has_value());
    CHECK(*stats.sharpe == Catch::Approx(0.05 / deviation * annualize));
    // Downside deviation sqrt(.01 / 4) = .05.
    REQUIRE(stats.sortino.has_value());
    CHECK(*stats.sortino == Catch::Approx(0.05 / 0.05 * annualize));

    const double years = 3.0 / 365.25;
    REQUIRE(stats.cagr.has_value());
    CHECK(*stats.cagr == Catch::Approx(std::pow(1.188, 1.0 / years) - 1.0));
    REQUIRE(stats.calmar.has_value());
    CHECK(*stats.calmar == Catch::Approx(*stats.cagr / 0.1));

    const auto pnl = terminal::periodPnl(curve);
    const std::vector<double> expected{100.0, -110.0, 198.0};
    REQUIRE(pnl.size() == 3);
    for (std::size_t index = 0; index < pnl.size(); ++index)
    {
        CHECK(pnl[index] == Catch::Approx(expected[index]));
    }
    REQUIRE(stats.var.has_value());
    CHECK(stats.var->observations == 3);
}

TEST_CASE("curve stats of a flat or empty curve have no ratios")
{
    CHECK(terminal::curveStats({}).periods == 0);
    const std::vector<terminal::EquityPoint> flat{statsPoint(0, 1'000.0, 0.0, 1.0, 0.0, 0),
                                                  statsPoint(kStatsDay, 1'000.0, 0.0, 1.0, 0.0, 0)};
    const auto stats = terminal::curveStats(flat);
    CHECK(stats.max_drawdown == 0.0);
    CHECK(stats.max_drawdown_seconds == 0);
    REQUIRE(stats.volatility.has_value());
    CHECK(*stats.volatility == 0.0);
    CHECK_FALSE(stats.sharpe.has_value());
    CHECK_FALSE(stats.sortino.has_value());
    CHECK_FALSE(stats.calmar.has_value());
}

TEST_CASE("buy and hold uses the first and last mark in range")
{
    const std::vector<terminal::Mark> marks{{300, 150.0}, {100, 100.0}, {200, 90.0}, {400, 1.0}};
    const auto held = terminal::buyAndHoldReturn(marks, 100, 300);
    REQUIRE(held.has_value());
    CHECK(*held == Catch::Approx(0.5));
    CHECK_FALSE(terminal::buyAndHoldReturn(marks, 150, 250).has_value());
    CHECK_FALSE(terminal::buyAndHoldReturn({}, 0, 1).has_value());
}

TEST_CASE("curve profit equals realized plus unrealized less open fees")
{
    constexpr terminal::InstrumentId instrument = 3;
    const auto fill = [&](terminal::TradeFillId id, terminal::UnixSeconds ts, double quantity, double price,
                          double fees) {
        terminal::TradeFill row;
        row.id = id;
        row.instrument_id = instrument;
        row.ts = ts;
        row.quantity = quantity;
        row.price = price;
        row.fees = fees;
        return row;
    };
    const std::vector<terminal::TradeFill> fills{
        fill(1, kStatsDay, 20, 50.0, 1.0),
        fill(2, 2 * kStatsDay, -5, 55.0, 0.5),
        fill(3, 3 * kStatsDay, -25, 52.0, 1.5),
        fill(4, 4 * kStatsDay, 5, 49.0, 0.75),
    };
    terminal::LedgerCashFlow deposit;
    deposit.ts = 0;
    deposit.amount = 5'000.0;
    const std::vector<terminal::LedgerCashFlow> flows{deposit};
    terminal::MarkSeries series;
    series.instrument_id = instrument;
    for (terminal::UnixSeconds day = 1; day <= 5; ++day)
    {
        series.marks.push_back({(day * kStatsDay) + 10, 50.0 + static_cast<double>(day)});
    }
    const std::vector<terminal::MarkSeries> marks{series};
    const auto points = terminal::markTimes(marks, 0, 10 * kStatsDay);
    const auto curve = terminal::equityCurve(fills, flows, {}, marks, points);
    const auto stats = terminal::curveStats(curve);

    // 20 - 5 - 25 + 5 leaves a 5-share short open.
    const auto book = terminal::matchLots(fills, {}, points.back());
    REQUIRE(book.positions.size() == 1);
    CHECK(book.positions[0].quantity == Catch::Approx(-5.0));
    const double last_mark = series.marks.back().price;
    const double expected =
        book.realized_pnl + terminal::unrealizedPnl(book.positions[0], last_mark) - book.positions[0].open_fees;
    CHECK(stats.net_pnl == Catch::Approx(expected));
    CHECK(terminal::tradeStats(book.round_trips).net_profit == Catch::Approx(book.realized_pnl));
}
