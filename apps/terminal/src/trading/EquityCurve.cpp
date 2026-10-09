// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "trading/EquityCurve.h"

#include "trading/Ledger.h"

#include "market_data/Time.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <span>
#include <stdexcept>
#include <vector>

namespace terminal {
namespace {

// Latest mark at or before a time that only moves forward.
class MarkCursor
{
public:
    explicit MarkCursor(std::vector<Mark> marks) : marks_(std::move(marks))
    {
        std::ranges::stable_sort(marks_, {}, &Mark::ts);
    }

    void advanceTo(UnixSeconds ts) noexcept
    {
        while (next_ < marks_.size() && marks_[next_].ts <= ts)
        {
            ++next_;
        }
    }

    [[nodiscard]] const Mark* latest() const noexcept
    {
        return next_ == 0 ? nullptr : &marks_[next_ - 1];
    }

private:
    std::vector<Mark> marks_;
    std::size_t next_{0};
};

template <typename Row>
[[nodiscard]] std::vector<const Row*> byTime(std::span<const Row> rows)
{
    std::vector<const Row*> ordered;
    ordered.reserve(rows.size());
    for (const Row& row : rows)
    {
        ordered.push_back(&row);
    }
    std::ranges::stable_sort(ordered, {}, [](const Row* row) { return row->ts; });
    return ordered;
}

}  // namespace

MarkSeries closeMarks(InstrumentId instrument_id, std::span<const Bar> bars)
{
    MarkSeries series;
    series.instrument_id = instrument_id;
    series.marks.reserve(bars.size());
    for (const Bar& bar : bars)
    {
        if (!isValidBar(bar))
        {
            continue;
        }
        series.marks.push_back(Mark{.ts = barCloseTime(bar), .price = bar.close});
    }
    return series;
}

std::vector<UnixSeconds> markTimes(std::span<const MarkSeries> series, UnixSeconds from, UnixSeconds to)
{
    std::vector<UnixSeconds> times;
    for (const MarkSeries& one : series)
    {
        for (const Mark& mark : one.marks)
        {
            if (mark.ts >= from && mark.ts <= to)
            {
                times.push_back(mark.ts);
            }
        }
    }
    std::ranges::sort(times);
    const auto [first, last] = std::ranges::unique(times);
    times.erase(first, last);
    return times;
}

std::vector<EquityPoint> equityCurve(std::span<const TradeFill> fills,
                                     std::span<const LedgerCashFlow> cash_flows,
                                     std::span<const CorporateAction> actions,
                                     std::span<const MarkSeries> marks,
                                     std::span<const UnixSeconds> points)
{
    const auto ordered_fills = byTime(fills);
    const auto ordered_flows = byTime(cash_flows);
    std::map<InstrumentId, MarkCursor> cursors;
    for (const MarkSeries& series : marks)
    {
        cursors.insert_or_assign(series.instrument_id, MarkCursor(series.marks));
    }

    LotBook book(actions);
    std::map<PositionKey, double> option_last;
    std::size_t next_fill = 0;
    std::size_t next_flow = 0;
    double contributed = 0.0;
    double previous_equity = 0.0;
    double growth = 1.0;
    double peak = 1.0;
    std::vector<EquityPoint> curve;
    curve.reserve(points.size());

    for (const UnixSeconds ts : points)
    {
        if (!curve.empty() && ts <= curve.back().ts)
        {
            throw std::runtime_error("equity curve points are not ascending");
        }
        EquityPoint point;
        point.ts = ts;
        while (next_flow < ordered_flows.size() && ordered_flows[next_flow]->ts <= ts)
        {
            point.net_flow += ordered_flows[next_flow]->amount;
            ++next_flow;
        }
        while (next_fill < ordered_fills.size() && ordered_fills[next_fill]->ts <= ts)
        {
            const TradeFill& fill = *ordered_fills[next_fill];
            book.apply(fill);
            if (fill.kind == TradeAssetKind::Option)
            {
                option_last.insert_or_assign(positionKey(fill), fill.price);
            }
            ++next_fill;
        }
        if (ts > book.time())
        {
            book.advanceTo(ts);
        }
        for (auto& [id, cursor] : cursors)
        {
            cursor.advanceTo(ts);
        }

        const auto positions = book.positions();
        for (const Position& position : positions)
        {
            double mark = position.average_price;
            if (position.key.kind == TradeAssetKind::Option)
            {
                const auto found = option_last.find(position.key);
                if (found != option_last.end())
                {
                    mark = found->second;
                }
            }
            else if (const auto found = cursors.find(position.key.instrument_id); found != cursors.end())
            {
                if (const Mark* latest = found->second.latest(); latest != nullptr)
                {
                    mark = latest->price;
                }
            }
            point.market_value += position.quantity * mark * position.multiplier;
        }
        point.open_positions = positions.size();

        contributed += point.net_flow;
        point.contributed = contributed;
        point.cash = contributed + book.tradeCash() + book.dividendCash();
        point.equity = point.cash + point.market_value;

        const double base = previous_equity + point.net_flow;
        point.period_return = base > 0.0 ? (point.equity - base) / base : std::numeric_limits<double>::quiet_NaN();
        if (std::isfinite(point.period_return))
        {
            growth *= 1.0 + point.period_return;
        }
        peak = std::max(peak, growth);
        point.growth = growth;
        point.drawdown = (growth / peak) - 1.0;
        previous_equity = point.equity;
        curve.push_back(point);
    }
    return curve;
}

}  // namespace terminal
