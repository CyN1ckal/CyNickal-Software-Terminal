// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <cstddef>
#include <span>
#include <vector>

namespace terminal {

// A price known at ts. For a bar that is its close, known when the bar ends.
struct Mark
{
    UnixSeconds ts{};
    double price{};
};

// As-traded marks for one instrument's shares, in time order.
struct MarkSeries
{
    InstrumentId instrument_id{};
    std::vector<Mark> marks;
};

// A daily bar's close is known at the end of its RTH session. Any other bar's
// close is known at ts + timeframe_s. Invalid bars are skipped.
[[nodiscard]] MarkSeries closeMarks(InstrumentId instrument_id, std::span<const Bar> bars);

// Every mark time in [from, to] across the series, sorted and unique. The usual
// evaluation points for equityCurve: one per session close of daily marks.
[[nodiscard]] std::vector<UnixSeconds> markTimes(std::span<const MarkSeries> series,
                                                 UnixSeconds from,
                                                 UnixSeconds to);

// The account at one evaluation time.
//
// cash is every cash flow, every fill's fillCashFlow, and every dividend credited
// up to ts. A dividend is account return: it raises cash and equity and does not
// raise net_flow or contributed. market_value is the open positions marked to
// market. net_flow is the cash flows since the previous point, and contributed
// their running sum.
//
// period_return is time-weighted: cash flows are taken to arrive at the start of
// the period, so r = (equity - previous equity - net_flow) / (previous equity + net_flow).
// It is NaN when that base is not positive (an empty account), and the index
// then does not move. growth is the compounded index, 1 before the first
// defined return. drawdown is growth / peak growth - 1 and is never positive.
struct EquityPoint
{
    UnixSeconds ts{};
    double cash{};
    double market_value{};
    double equity{};
    double net_flow{};
    double contributed{};
    double period_return{};
    double growth{1.0};
    double drawdown{};
    std::size_t open_positions{};
};

// Replays the ledger to each point in `points` (ascending; a repeated or earlier
// point throws). Fills and cash flows at or before a point count toward it. A
// dividend is credited on the first point whose time reaches its ex_ts: after the
// previous point, or from the beginning on the first. The quantity is the share
// position LotBook holds at that ex_ts.
//
// A share position is marked at its instrument's latest mark at or before the
// point. One with no mark yet is valued at its average price, so it adds no
// unrealized profit. An option position is marked at the last price its own
// contract traded in this ledger; the store keeps no option price history.
[[nodiscard]] std::vector<EquityPoint> equityCurve(std::span<const TradeFill> fills,
                                                   std::span<const LedgerCashFlow> cash_flows,
                                                   std::span<const CorporateAction> actions,
                                                   std::span<const MarkSeries> marks,
                                                   std::span<const UnixSeconds> points);

}  // namespace terminal
