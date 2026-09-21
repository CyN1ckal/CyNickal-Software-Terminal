#include "market_data/MboumMap.h"

#include "market_data/Time.h"

#include <string>

namespace myapp {
namespace {

void fillOhlcv(Bar& bar, double open, double high, double low, double close, double volume)
{
    bar.open = open;
    bar.high = high;
    bar.low = low;
    bar.close = close;
    bar.volume = volume;
}

[[nodiscard]] std::optional<Bar> finishBar(const Instrument& inst, Bar bar, UnixSeconds now_utc)
{
    bar.instrument_id = inst.id;
    bar.timeframe_s = kTimeframe1m;
    if (isFormingBar(bar, now_utc) || !isValidBar(bar) || !isUsRthAt(inst.timezone, bar.ts))
    {
        return std::nullopt;
    }
    return bar;
}

}  // namespace

std::optional<Bar> mapV3Bar(const Instrument& inst, const MboumV3BarRow& row, UnixSeconds now_utc)
{
    // v3 ingest must request splits=false; callers discard pages with meta.splits==true.
    const auto ts = naiveLocalToUtc(inst.timezone, row.datetime);
    if (!ts.has_value())
    {
        return std::nullopt;
    }
    Bar bar;
    bar.ts = *ts;
    fillOhlcv(bar, row.open, row.high, row.low, row.close, row.volume);
    return finishBar(inst, bar, now_utc);
}

std::optional<Bar> mapV2Bar(const Instrument& inst, const MboumV2BarRow& row, UnixSeconds now_utc)
{
    UnixSeconds ts = 0;
    if (row.timestamp_unix.has_value())
    {
        ts = *row.timestamp_unix;
    }
    else
    {
        const auto parsed = naiveLocalToUtc(inst.timezone, row.timestamp);
        if (!parsed.has_value())
        {
            return std::nullopt;
        }
        ts = *parsed;
    }
    Bar bar;
    bar.ts = ts;
    fillOhlcv(bar, row.open, row.high, row.low, row.close, row.volume);
    return finishBar(inst, bar, now_utc);
}

std::optional<CorporateAction> mapSplit(InstrumentId id, const MboumSplitRow& row)
{
    if (!row.old_share_worth.has_value() || *row.old_share_worth <= 0.0 || !row.share_worth.has_value())
    {
        return std::nullopt;
    }
    const auto ex = parseRfc3339Utc(row.startdatetime);
    if (!ex.has_value())
    {
        return std::nullopt;
    }
    CorporateAction action;
    action.instrument_id = id;
    action.ex_ts = *ex;
    action.type = CorporateActionType::Split;
    action.split_ratio = *row.share_worth / *row.old_share_worth;
    action.source = "mboum";
    return action;
}

std::optional<CorporateAction> mapDividend(InstrumentId id, const MboumDividendRow& row)
{
    const auto ex = parseUsDateToUtcMidnight(row.ex_date);
    const auto amount = parseMoneyAmount(row.amount);
    if (!ex.has_value() || !amount.has_value())
    {
        return std::nullopt;
    }
    CorporateAction action;
    action.instrument_id = id;
    action.ex_ts = *ex;
    action.type = CorporateActionType::Dividend;
    action.amount = amount;
    action.currency = row.currency;
    action.source = "mboum";
    return action;
}

}  // namespace myapp
