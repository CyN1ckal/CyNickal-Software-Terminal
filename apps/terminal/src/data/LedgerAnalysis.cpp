// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "data/LedgerAnalysis.h"

#include "data/PortfolioFetch.h"

#include "market_data/Adjust.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace terminal {
namespace {

constexpr UnixSeconds kAllTime = std::numeric_limits<UnixSeconds>::max();

void noteReceived(std::optional<UnixSeconds>& newest, const Store& store, InstrumentId id)
{
    for (const CoverageDay& day : store.queryCoverageDays(id, kTimeframe1d))
    {
        if (day.bar_count > 0 && (!newest.has_value() || day.ingested_at > *newest))
        {
            newest = day.ingested_at;
        }
    }
}

// Price return when nothing is credited. Otherwise one adjusted share funded at the
// first mark: dividend cash sits in equity, and the curve's growth is the return.
// Prices are already split-adjusted, so splits are not replayed. A dividend is
// as-traded cash; it is divided by every later split adjustBarsForSplits applied.
[[nodiscard]] std::optional<double> benchmarkTotalReturn(InstrumentId instrument,
                                                         std::span<const Mark> marks,
                                                         std::span<const CorporateAction> actions)
{
    const Mark* first = nullptr;
    const Mark* last = nullptr;
    for (const Mark& mark : marks)
    {
        if (first == nullptr || mark.ts < first->ts)
        {
            first = &mark;
        }
        if (last == nullptr || mark.ts >= last->ts)
        {
            last = &mark;
        }
    }
    if (first == nullptr || last == nullptr)
    {
        return std::nullopt;
    }
    const std::optional<double> price_return = buyAndHoldReturn(marks, first->ts, last->ts);
    if (!price_return.has_value())
    {
        return std::nullopt;
    }

    std::vector<CorporateAction> dividends;
    for (const CorporateAction& action : actions)
    {
        if (action.type != CorporateActionType::Dividend || !action.amount.has_value() || action.ex_ts <= first->ts ||
            action.ex_ts > last->ts)
        {
            continue;
        }
        const double amount = *action.amount;
        if (!std::isfinite(amount) || amount < 0.0)
        {
            continue;
        }
        const double factor = splitFactorBetween(actions, instrument, action.ex_ts, kAllTime);
        CorporateAction scaled = action;
        scaled.amount = amount / factor;
        dividends.push_back(std::move(scaled));
    }
    if (dividends.empty())
    {
        return price_return;
    }

    TradeFill buy;
    buy.id = 1;
    buy.kind = TradeAssetKind::Equity;
    buy.instrument_id = instrument;
    buy.ts = first->ts;
    buy.quantity = 1.0;
    buy.price = first->price;
    LedgerCashFlow funding;
    funding.ts = first->ts;
    funding.amount = first->price;

    std::vector<UnixSeconds> points;
    points.reserve(marks.size());
    for (const Mark& mark : marks)
    {
        points.push_back(mark.ts);
    }
    std::ranges::sort(points);
    const auto [duplicate, end] = std::ranges::unique(points);
    points.erase(duplicate, end);
    if (points.size() < 2)
    {
        return price_return;
    }

    MarkSeries series;
    series.instrument_id = instrument;
    series.marks.assign(marks.begin(), marks.end());
    const std::vector<TradeFill> fills{buy};
    const std::vector<LedgerCashFlow> flows{funding};
    const std::vector<MarkSeries> series_list{std::move(series)};
    const std::vector<EquityPoint> curve = equityCurve(fills, flows, dividends, series_list, points);
    if (curve.empty())
    {
        return price_return;
    }
    return curve.back().growth - 1.0;
}

}  // namespace

LedgerAnalysis analyzeLedger(const Store& store,
                             LedgerId id,
                             std::optional<InstrumentId> benchmark,
                             UnixSeconds now,
                             CurveStatsSpec spec)
{
    LedgerAnalysis analysis;
    // A backtest ends where its run ended; days after that are not part of its record.
    if (const std::optional<BacktestRun> run = store.findBacktestRunForLedger(id); run.has_value())
    {
        now = std::min(now, run->ts_end);
    }
    const std::vector<TradeFill> fills = store.queryFills(id);
    const std::vector<LedgerCashFlow> flows = store.queryCashFlows(id);

    // Share instruments in first-traded order, with the symbol of their newest fill.
    std::map<InstrumentId, std::string> symbols;
    std::vector<InstrumentId> shares;
    for (const TradeFill& fill : fills)
    {
        if (fill.kind == TradeAssetKind::Option || !fill.instrument_id.has_value())
        {
            continue;
        }
        const InstrumentId instrument = *fill.instrument_id;
        // One lookup: a new instrument joins `shares` in first-traded order, and every fill
        // leaves its own symbol as the newest one for that instrument.
        if (const auto added = symbols.insert_or_assign(instrument, fill.symbol.value_or(std::string{}));
            added.second)
        {
            shares.push_back(instrument);
        }
    }

    std::vector<CorporateAction> actions;
    std::vector<MarkSeries> marks;
    for (const InstrumentId instrument : shares)
    {
        const std::vector<CorporateAction> found = store.queryCorporateActions(instrument, 0, kAllTime);
        actions.insert(actions.end(), found.begin(), found.end());
        MarkSeries series = closeMarks(instrument, store.queryBars(instrument, kTimeframe1d, 0, kAllTime));
        if (series.marks.empty())
        {
            analysis.unpriced_symbols.push_back(symbols[instrument]);
        }
        else
        {
            noteReceived(analysis.received_at, store, instrument);
        }
        marks.push_back(std::move(series));
    }

    std::optional<UnixSeconds> first_activity;
    const auto noteActivity = [&first_activity](UnixSeconds ts) {
        if (!first_activity.has_value() || ts < *first_activity)
        {
            first_activity = ts;
        }
    };
    for (const TradeFill& fill : fills)
    {
        noteActivity(fill.ts);
    }
    for (const LedgerCashFlow& flow : flows)
    {
        noteActivity(flow.ts);
    }
    if (!first_activity.has_value())
    {
        return analysis;
    }

    std::vector<UnixSeconds> points = markTimes(marks, *first_activity, now);
    if (points.empty())
    {
        for (const TradeFill& fill : fills)
        {
            points.push_back(fill.ts);
        }
        for (const LedgerCashFlow& flow : flows)
        {
            points.push_back(flow.ts);
        }
        std::ranges::sort(points);
        const auto [first, last] = std::ranges::unique(points);
        points.erase(first, last);
    }

    analysis.curve = equityCurve(fills, flows, actions, marks, points);
    analysis.curve_stats = curveStats(analysis.curve, spec);
    const LedgerBook book = matchLots(fills, actions, now);
    analysis.round_trips = book.round_trips;
    analysis.trade_stats = tradeStats(book.round_trips);

    if (benchmark.has_value() && !analysis.curve.empty())
    {
        std::vector<Bar> bars = store.queryBars(*benchmark, kTimeframe1d, 0, kAllTime);
        std::vector<CorporateAction> benchmark_actions;
        if (!bars.empty())
        {
            benchmark_actions = store.queryCorporateActions(*benchmark, 0, bars.back().ts);
            bars = adjustBarsForSplits(std::move(bars), benchmark_actions);
        }
        const UnixSeconds from = analysis.curve.front().ts;
        const UnixSeconds to = analysis.curve.back().ts;
        for (const Mark& mark : closeMarks(*benchmark, bars).marks)
        {
            if (mark.ts >= from && mark.ts <= to)
            {
                analysis.benchmark_marks.push_back(mark);
            }
        }
        analysis.benchmark_return = benchmarkTotalReturn(*benchmark, analysis.benchmark_marks, benchmark_actions);
    }
    return analysis;
}

std::vector<IngestJob> ledgerHistoryFetchJobs(const Store& store,
                                                      std::span<const TradeFill> fills,
                                                      std::string_view benchmark_symbol,
                                                      SessionDate today,
                                                      bool missing_only)
{
    std::map<InstrumentId, PortfolioHolding> holdings;
    for (const TradeFill& fill : fills)
    {
        if (fill.kind == TradeAssetKind::Option || !fill.instrument_id.has_value())
        {
            continue;
        }
        PortfolioHolding& holding = holdings[*fill.instrument_id];
        holding.kind = fill.kind == TradeAssetKind::Etf ? PortfolioAssetKind::Etf : PortfolioAssetKind::Equity;
        holding.instrument_id = fill.instrument_id;
        holding.symbol = fill.symbol;
        holding.listing_open = fill.listing_open;
        holding.quantity = 1.0;
    }
    std::vector<PortfolioHolding> list;
    list.reserve(holdings.size());
    for (auto& [instrument, holding] : holdings)
    {
        list.push_back(std::move(holding));
    }
    std::vector<IngestJob> jobs = portfolioFetchJobs(store, list, today, missing_only);
    if (benchmark_symbol.empty())
    {
        return jobs;
    }
    const bool planned = std::ranges::any_of(jobs, [&](const IngestJob& job) {
        return job.symbol == benchmark_symbol;
    });
    if (planned)
    {
        return jobs;
    }
    const std::optional<Instrument> instrument = store.resolveSymbol(benchmark_symbol);
    if (instrument.has_value())
    {
        if (!instrument->listing_open)
        {
            return jobs;
        }
        if (missing_only)
        {
            const std::vector<CoverageDay> days = store.queryCoverageDays(instrument->id, kTimeframe1d);
            if (std::ranges::any_of(days, [](const CoverageDay& day) { return day.bar_count > 0; }))
            {
                return jobs;
            }
        }
    }
    PortfolioHolding probe;
    probe.kind = PortfolioAssetKind::Equity;
    probe.instrument_id = instrument.has_value() ? instrument->id : 0;
    probe.symbol = std::string(benchmark_symbol);
    probe.listing_open = true;
    probe.quantity = 1.0;
    // A symbol not in the store yet has no coverage, so plan it without asking.
    const std::vector<IngestJob> benchmark_jobs =
        portfolioFetchJobs(store, std::span<const PortfolioHolding>(&probe, 1), today, false);
    jobs.insert(jobs.end(), benchmark_jobs.begin(), benchmark_jobs.end());
    return jobs;
}

}  // namespace terminal
