// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartTransform.h"

#include "market_data/Time.h"

namespace terminal {
namespace {

void foldMinute(Bar& bucket, const Bar& minute) noexcept
{
    if (minute.high > bucket.high)
    {
        bucket.high = minute.high;
    }
    if (minute.low < bucket.low)
    {
        bucket.low = minute.low;
    }
    bucket.close = minute.close;
    bucket.volume += minute.volume;
}

[[nodiscard]] Bar startBucket(const Bar& minute, UnixSeconds ts, int timeframe_s) noexcept
{
    Bar bucket = minute;
    bucket.ts = ts;
    bucket.timeframe_s = timeframe_s;
    return bucket;
}

}  // namespace

std::vector<Bar> transformChartBars(const std::vector<Bar>& bars_1m,
                                    ChartBarPeriod period,
                                    std::string_view timezone)
{
    if (!chartNeedsBarTransform(period))
    {
        return bars_1m;
    }

    (void)utcToSessionDate(timezone, 0);

    const int target_tf = timeframeSeconds(period);
    std::vector<Bar> out;
    out.reserve(bars_1m.size());

    Bar bucket{};
    bool open_bucket = false;
    UnixSeconds bucket_ts = 0;
    SessionDate bucket_session = 0;
    // queryBars is ascending; classify RTH with a cached [start, end) so TZ
    // conversion happens on session changes, not every 1-minute bar.
    bool have_window = false;
    SessionDate cached_session = 0;
    UnixSeconds cached_open = 0;
    UnixSeconds cached_end = 0;

    for (const Bar& minute : bars_1m)
    {
        if (minute.timeframe_s != kTimeframe1m)
        {
            continue;
        }
        if (!have_window || minute.ts < cached_open || minute.ts >= cached_end)
        {
            const SessionDate session = utcToSessionDate(timezone, minute.ts);
            if (have_window && session == cached_session)
            {
                continue;
            }
            const UtcWindow win = usRthUtcWindow(timezone, session);
            cached_session = session;
            cached_open = win.start;
            cached_end = win.end;
            have_window = true;
            if (minute.ts < cached_open || minute.ts >= cached_end)
            {
                continue;
            }
        }

        UnixSeconds aligned = cached_open;
        if (period != ChartBarPeriod::Day1)
        {
            aligned = cached_open + ((minute.ts - cached_open) / target_tf) * target_tf;
        }

        const bool new_bucket =
            !open_bucket || cached_session != bucket_session || aligned != bucket_ts;
        if (new_bucket)
        {
            if (open_bucket)
            {
                out.push_back(bucket);
            }
            bucket = startBucket(minute, aligned, target_tf);
            bucket_ts = aligned;
            bucket_session = cached_session;
            open_bucket = true;
        }
        else
        {
            foldMinute(bucket, minute);
        }
    }
    if (open_bucket)
    {
        out.push_back(bucket);
    }
    return out;
}

}  // namespace terminal
