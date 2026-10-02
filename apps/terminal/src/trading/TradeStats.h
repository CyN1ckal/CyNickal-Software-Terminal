// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "risk/HistoricalRisk.h"
#include "trading/EquityCurve.h"
#include "trading/Ledger.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace terminal {

// Statistics over closed round trips, on net P&L (after fees). A position closed
// in three sells is three round trips. A trip that nets exactly zero is neither a
// win nor a loss. Money is in the ledger currency. gross_loss and average_loss
// and largest_loss are negative or zero.
struct TradeStats
{
    std::size_t trades{};
    std::size_t wins{};
    std::size_t losses{};
    std::size_t breakeven{};
    double win_rate{};
    double net_profit{};
    double gross_profit{};
    double gross_loss{};
    double fees{};
    double average_trade{};
    double average_win{};
    double average_loss{};
    double largest_win{};
    double largest_loss{};
    // gross_profit / -gross_loss. Empty with no losing trade.
    std::optional<double> profit_factor;
    // average_win / -average_loss. Empty without both a win and a loss.
    std::optional<double> payoff_ratio;
    std::size_t max_consecutive_wins{};
    std::size_t max_consecutive_losses{};
    // Mean of closed_at - opened_at.
    double average_holding_seconds{};
};

// Trips in closing order, as LotBook::roundTrips returns them. Streaks follow that order.
[[nodiscard]] TradeStats tradeStats(std::span<const RoundTrip> trips);

// Ratios over an equity curve's time-weighted returns. Periods whose return is
// NaN are skipped. Annualizing assumes one point per period_per_year of a year,
// so pass session-close points for daily figures.
struct CurveStatsSpec
{
    double periods_per_year{252.0};
    double risk_free_annual{0.0};
    double var_confidence{0.95};
};

struct CurveStats
{
    // Points with a defined return.
    std::size_t periods{};
    // Last growth - 1.
    double total_return{};
    // Most negative drawdown (0 when the index never fell).
    double max_drawdown{};
    // Longest time the index spent below a peak: from the peak to the point that
    // regained it, or to the last point if it never did. 0 when it never fell.
    UnixSeconds max_drawdown_seconds{};
    // Share of points that held at least one position.
    double exposure{};
    // Last equity less everything contributed.
    double net_pnl{};
    // Compound annual growth over the calendar time between the first and last point.
    std::optional<double> cagr;
    // Annualized sample standard deviation of returns. Needs two periods.
    std::optional<double> volatility;
    std::optional<double> sharpe;
    std::optional<double> sortino;
    // cagr / -max_drawdown. Empty with no drawdown.
    std::optional<double> calmar;
    // One-period historical VaR of period P&L (equity change less flows).
    std::optional<ValueAtRisk> var;
};

[[nodiscard]] CurveStats curveStats(std::span<const EquityPoint> curve, CurveStatsSpec spec = {});

// Equity change less that period's flows, one per point after the first.
[[nodiscard]] std::vector<double> periodPnl(std::span<const EquityPoint> curve);

// last / first - 1 of the marks in [from, to]. Empty with fewer than two marks or a non-positive first mark.
// Pass split-adjusted marks.
[[nodiscard]] std::optional<double> buyAndHoldReturn(std::span<const Mark> marks, UnixSeconds from, UnixSeconds to);

}  // namespace terminal
