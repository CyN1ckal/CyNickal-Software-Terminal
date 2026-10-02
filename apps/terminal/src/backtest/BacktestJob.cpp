// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "backtest/BacktestJob.h"

#include "chart/CChartTransform.h"

#include "market_data/Adjust.h"
#include "market_data/Time.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace terminal {
namespace {

[[nodiscard]] const char* sizingToken(BacktestSizing sizing) noexcept
{
    switch (sizing)
    {
    case BacktestSizing::FixedShares:
        return "shares";
    case BacktestSizing::FixedNotional:
        return "notional";
    case BacktestSizing::PercentEquity:
        return "percent_equity";
    }
    return "percent_equity";
}

[[nodiscard]] bool readNumber(const nlohmann::json& object, const char* key, double& out)
{
    const auto found = object.find(key);
    if (found == object.end())
    {
        return true;
    }
    if (!found->is_number() || !std::isfinite(found->get<double>()))
    {
        return false;
    }
    out = found->get<double>();
    return true;
}

[[nodiscard]] bool readOptionalNumber(const nlohmann::json& object, const char* key, std::optional<double>& out)
{
    const auto found = object.find(key);
    if (found == object.end() || found->is_null())
    {
        return true;
    }
    double value = 0.0;
    if (!readNumber(object, key, value))
    {
        return false;
    }
    out = value;
    return true;
}

[[nodiscard]] bool readFlag(const nlohmann::json& object, const char* key, bool& out)
{
    const auto found = object.find(key);
    if (found == object.end())
    {
        return true;
    }
    if (!found->is_boolean())
    {
        return false;
    }
    out = found->get<bool>();
    return true;
}

}  // namespace

std::optional<ChartBarPeriod> backtestPeriodFromCode(std::string_view code) noexcept
{
    constexpr ChartBarPeriod kPeriods[] = {
        ChartBarPeriod::Minute1, ChartBarPeriod::Minute5, ChartBarPeriod::Minute15,
        ChartBarPeriod::Hour1,   ChartBarPeriod::Day1,
    };
    for (const ChartBarPeriod period : kPeriods)
    {
        if (code == chartPeriodCode(period))
        {
            return period;
        }
    }
    return std::nullopt;
}

std::string backtestConfigJson(const BacktestConfig& config)
{
    nlohmann::ordered_json object = nlohmann::ordered_json::object();
    object["initial_cash"] = config.initial_cash;
    object["sizing"] = sizingToken(config.sizing);
    object["sizing_value"] = config.sizing_value;
    object["commission_per_share"] = config.commission_per_share;
    object["commission_minimum"] = config.commission_minimum;
    object["slippage_bps"] = config.slippage_bps;
    object["stop_loss_pct"] = config.stop_loss_pct.has_value() ? nlohmann::ordered_json(*config.stop_loss_pct)
                                                               : nlohmann::ordered_json(nullptr);
    object["take_profit_pct"] = config.take_profit_pct.has_value() ? nlohmann::ordered_json(*config.take_profit_pct)
                                                                   : nlohmann::ordered_json(nullptr);
    object["allow_short"] = config.allow_short;
    object["flatten_at_session_end"] = config.flatten_at_session_end;
    object["close_at_end"] = config.close_at_end;
    object["timezone"] = config.timezone;
    return object.dump();
}

std::optional<BacktestConfig> backtestConfigFromJson(std::string_view json)
{
    const nlohmann::json object = nlohmann::json::parse(json, nullptr, false);
    if (object.is_discarded() || !object.is_object())
    {
        return std::nullopt;
    }
    BacktestConfig config;
    if (const auto sizing = object.find("sizing"); sizing != object.end())
    {
        if (!sizing->is_string())
        {
            return std::nullopt;
        }
        const std::string token = sizing->get<std::string>();
        bool known = false;
        for (const BacktestSizing candidate :
             {BacktestSizing::FixedShares, BacktestSizing::FixedNotional, BacktestSizing::PercentEquity})
        {
            if (token == sizingToken(candidate))
            {
                config.sizing = candidate;
                known = true;
            }
        }
        if (!known)
        {
            return std::nullopt;
        }
    }
    if (const auto timezone = object.find("timezone"); timezone != object.end())
    {
        if (!timezone->is_string())
        {
            return std::nullopt;
        }
        config.timezone = timezone->get<std::string>();
    }
    const bool ok = readNumber(object, "initial_cash", config.initial_cash) &&
                    readNumber(object, "sizing_value", config.sizing_value) &&
                    readNumber(object, "commission_per_share", config.commission_per_share) &&
                    readNumber(object, "commission_minimum", config.commission_minimum) &&
                    readNumber(object, "slippage_bps", config.slippage_bps) &&
                    readOptionalNumber(object, "stop_loss_pct", config.stop_loss_pct) &&
                    readOptionalNumber(object, "take_profit_pct", config.take_profit_pct) &&
                    readFlag(object, "allow_short", config.allow_short) &&
                    readFlag(object, "flatten_at_session_end", config.flatten_at_session_end) &&
                    readFlag(object, "close_at_end", config.close_at_end);
    if (!ok)
    {
        return std::nullopt;
    }
    return config;
}

TradeFill unadjustFill(TradeFill fill, std::span<const CorporateAction> actions, UnixSeconds through)
{
    if (!fill.instrument_id.has_value())
    {
        return fill;
    }
    const double factor = splitFactorBetween(actions, *fill.instrument_id, fill.ts, through);
    fill.price *= factor;
    fill.quantity /= factor;
    return fill;
}

std::vector<Bar> backtestBars(const Store& store, const BacktestRequest& request)
{
    const std::optional<Instrument> instrument = store.resolveSymbol(request.symbol);
    if (!instrument.has_value())
    {
        throw std::runtime_error(request.symbol + " is not in the store");
    }
    const UnixSeconds begin = sessionUtcWindow(instrument->timezone, request.from).start;
    const UnixSeconds end = sessionUtcWindow(instrument->timezone, request.to).end;
    std::vector<Bar> bars;
    if (request.period == ChartBarPeriod::Day1)
    {
        bars = store.queryBars(instrument->id, kTimeframe1d, begin, end);
    }
    else
    {
        bars = transformChartBars(store.queryBars(instrument->id, kTimeframe1m, begin, end), request.period,
                                  instrument->timezone);
    }
    if (!bars.empty())
    {
        const std::vector<CorporateAction> actions = store.queryCorporateActions(instrument->id, 0, bars.back().ts);
        bars = adjustBarsForSplits(std::move(bars), actions);
    }
    return bars;
}

BacktestOutcome runAndRecordBacktest(Store& writer, const BacktestRequest& request)
{
    BacktestOutcome outcome;
    try
    {
        const StrategyType* strategy = findStrategy(request.strategy_id);
        if (strategy == nullptr)
        {
            outcome.error = "unknown strategy " + request.strategy_id;
            return outcome;
        }
        const std::optional<Instrument> instrument = writer.resolveSymbol(request.symbol);
        if (!instrument.has_value())
        {
            outcome.error = request.symbol + " is not in the store";
            return outcome;
        }
        if (!instrument->figi.has_value() ||
            (instrument->asset_class != AssetClass::Equity && instrument->asset_class != AssetClass::Etf))
        {
            outcome.error = instrument->symbol + " is not an equity or ETF";
            return outcome;
        }
        if (request.to < request.from)
        {
            outcome.error = "the range ends before it starts";
            return outcome;
        }
        const std::vector<Bar> bars = backtestBars(writer, request);
        outcome.bars = bars.size();
        if (bars.size() < 2)
        {
            outcome.error = "not enough " + std::string(chartPeriodCode(request.period)) + " bars for " +
                            instrument->symbol + " in that range";
            return outcome;
        }

        const TradeAssetKind kind =
            instrument->asset_class == AssetClass::Etf ? TradeAssetKind::Etf : TradeAssetKind::Equity;
        const std::vector<int> options = clampStrategyOptions(*strategy, request.options);
        // Sessions end where the bars were composited, in the instrument's timezone.
        BacktestConfig config = request.config;
        config.timezone = instrument->timezone;
        const BacktestResult result = runBacktest(bars, *strategy, options, config, instrument->id, kind);

        // The bars were adjusted only for splits through the last bar (backtestBars), so
        // only those are undone; a later split never touched these prices.
        const UnixSeconds adjusted_through = bars.back().ts;
        const std::vector<CorporateAction> actions =
            writer.queryCorporateActions(instrument->id, 0, adjusted_through);
        std::vector<TradeFill> fills;
        fills.reserve(result.fills.size());
        for (const TradeFill& fill : result.fills)
        {
            TradeFill traded = unadjustFill(fill, actions, adjusted_through);
            traded.figi = instrument->figi;
            fills.push_back(std::move(traded));
        }
        LedgerCashFlow start;
        start.ts = bars.front().ts;
        start.amount = config.initial_cash;
        start.note = "starting capital";

        BacktestRun run;
        run.strategy_id = strategy->id;
        run.params_json = strategyParamsJson(*strategy, options);
        run.config_json = backtestConfigJson(config);
        run.figi = instrument->figi;
        run.timeframe_s = timeframeSeconds(request.period);
        run.ts_begin = bars.front().ts;
        run.ts_end = barCloseTime(bars.back());
        run.engine_version = kBacktestEngineVersion;

        outcome.ledger_name = std::string(strategy->display_name) + " " + instrument->symbol + " " +
                              chartPeriodCode(request.period);
        const std::span<const LedgerCashFlow> cash(&start, config.initial_cash != 0.0 ? 1 : 0);
        outcome.recorded = writer.recordBacktestRun(outcome.ledger_name, run, fills, cash);
        outcome.fills = fills.size();
        outcome.final_equity = result.equity.back();
        outcome.total_return = config.initial_cash != 0.0 ? (outcome.final_equity / config.initial_cash) - 1.0 : 0.0;
        outcome.ok = true;
    }
    catch (const std::exception& ex)
    {
        outcome.ok = false;
        outcome.error = ex.what();
    }
    return outcome;
}

}  // namespace terminal
