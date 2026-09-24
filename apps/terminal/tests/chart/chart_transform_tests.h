// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "chart/CChartLoad.h"
#include "chart/CChartSettings.h"
#include "chart/CChartTransform.h"
#include "market_data/Store.h"
#include "market_data/Time.h"
#include "market_data/Types.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kTz{"America/New_York"};

terminal::UnixSeconds ny(const char* naive)
{
    const auto ts = terminal::naiveLocalToUtc(kTz, naive);
    REQUIRE(ts.has_value());
    return *ts;
}

terminal::Bar minuteBar(terminal::InstrumentId id,
                        terminal::UnixSeconds ts,
                        double open,
                        double high,
                        double low,
                        double close,
                        double volume)
{
    terminal::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = terminal::kTimeframe1m;
    bar.ts = ts;
    bar.open = open;
    bar.high = high;
    bar.low = low;
    bar.close = close;
    bar.volume = volume;
    return bar;
}

std::vector<terminal::Bar> sequentialMinutes(terminal::InstrumentId id,
                                             terminal::UnixSeconds open_ts,
                                             int count)
{
    std::vector<terminal::Bar> bars;
    bars.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        const double base = 100.0 + static_cast<double>(i);
        bars.push_back(minuteBar(id,
                                 open_ts + static_cast<terminal::UnixSeconds>(i) * 60,
                                 base,
                                 base + 1.0,
                                 base - 1.0,
                                 base + 0.5,
                                 1.0));
    }
    return bars;
}

terminal::Instrument aaplInstrument()
{
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    inst.figi = "BBG000B9XRY4";
    inst.timezone = "America/New_York";
    return inst;
}

void ingestMinutes(terminal::Store& store,
                   terminal::InstrumentId id,
                   terminal::SessionDate session_date,
                   int count)
{
    const auto open = terminal::usRthUtcWindow(kTz, session_date).start;
    const auto result = store.ingestSession(
        sequentialMinutes(id, open, count), id, terminal::kTimeframe1m, session_date,
        terminal::kUsRthExpected1m);
    CHECK(result.bars.written == count);
}

}  // namespace

TEST_CASE("supported chart periods are 1m 5m 15m 1h 1d")
{
    CHECK(terminal::isChartPeriodSupported(terminal::ChartBarPeriod::Minute1));
    CHECK(terminal::isChartPeriodSupported(terminal::ChartBarPeriod::Minute5));
    CHECK(terminal::isChartPeriodSupported(terminal::ChartBarPeriod::Minute15));
    CHECK(terminal::isChartPeriodSupported(terminal::ChartBarPeriod::Hour1));
    CHECK(terminal::isChartPeriodSupported(terminal::ChartBarPeriod::Day1));
}

TEST_CASE("chart settings support candlestick session-count on every period")
{
    terminal::CChartSettings settings;
    for (const auto period : {terminal::ChartBarPeriod::Minute1, terminal::ChartBarPeriod::Minute5,
                              terminal::ChartBarPeriod::Minute15, terminal::ChartBarPeriod::Hour1,
                              terminal::ChartBarPeriod::Day1})
    {
        settings.period = period;
        CHECK(terminal::isChartSettingsSupported(settings));
    }
}

TEST_CASE("chart settings reject reserved bar type and limiters")
{
    terminal::CChartSettings settings;
    settings.period = terminal::ChartBarPeriod::Minute5;
    CHECK(terminal::isChartSettingsSupported(settings));

    settings.bar_type = terminal::ChartBarType::Ohlc;
    CHECK_FALSE(terminal::isChartSettingsSupported(settings));
    settings.bar_type = terminal::ChartBarType::Candlestick;

    settings.limit_mode = terminal::ChartDataLimitMode::BarCount;
    CHECK_FALSE(terminal::isChartSettingsSupported(settings));
    settings.limit_mode = terminal::ChartDataLimitMode::DateRange;
    CHECK_FALSE(terminal::isChartSettingsSupported(settings));
}

TEST_CASE("isChartSettingsSupported accepts periods isV1Supported still rejects")
{
    terminal::CChartSettings settings;
    settings.period = terminal::ChartBarPeriod::Minute5;
    CHECK_FALSE(terminal::isV1Supported(settings));
    CHECK(terminal::isChartSettingsSupported(settings));
}

TEST_CASE("1-minute charts do not need a bar transform")
{
    terminal::CChartSettings settings;
    CHECK_FALSE(terminal::chartNeedsBarTransform(terminal::ChartBarPeriod::Minute1));
    CHECK_FALSE(terminal::chartNeedsBarTransform(settings));
}

TEST_CASE("higher periods need a bar transform even if other fields are unsupported")
{
    CHECK(terminal::chartNeedsBarTransform(terminal::ChartBarPeriod::Minute5));
    CHECK(terminal::chartNeedsBarTransform(terminal::ChartBarPeriod::Minute15));
    CHECK(terminal::chartNeedsBarTransform(terminal::ChartBarPeriod::Hour1));
    CHECK(terminal::chartNeedsBarTransform(terminal::ChartBarPeriod::Day1));

    terminal::CChartSettings settings;
    settings.period = terminal::ChartBarPeriod::Minute5;
    settings.bar_type = terminal::ChartBarType::Ohlc;
    CHECK(terminal::chartNeedsBarTransform(settings));
    CHECK_FALSE(terminal::isChartSettingsSupported(settings));
}

TEST_CASE("timeframeSeconds maps each period onto a bar timeframe_s")
{
    CHECK(terminal::timeframeSeconds(terminal::ChartBarPeriod::Minute1) == terminal::kTimeframe1m);
    CHECK(terminal::timeframeSeconds(terminal::ChartBarPeriod::Minute5) == 300);
    CHECK(terminal::timeframeSeconds(terminal::ChartBarPeriod::Minute15) == 900);
    CHECK(terminal::timeframeSeconds(terminal::ChartBarPeriod::Hour1) == 3600);
    CHECK(terminal::timeframeSeconds(terminal::ChartBarPeriod::Day1) == 86400);
}

TEST_CASE("1-minute transform is identity")
{
    const auto open = ny("2025-01-15 09:30");
    const auto in = sequentialMinutes(1, open, 3);
    const auto out =
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Minute1, kTz);
    REQUIRE(out.size() == 3);
    CHECK(out[0].ts == in[0].ts);
    CHECK(out[1].ts == in[1].ts);
    CHECK(out[2].ts == in[2].ts);
    CHECK(out[0].timeframe_s == terminal::kTimeframe1m);
    CHECK(out[0].close == Catch::Approx(in[0].close));
}

TEST_CASE("5m composites OHLCV from five 1-minute bars")
{
    const auto open = ny("2025-01-15 09:30");
    const std::vector<terminal::Bar> in{
        minuteBar(7, open + 0 * 60, 10.0, 11.0, 9.0, 10.5, 100.0),
        minuteBar(7, open + 1 * 60, 10.5, 12.0, 10.0, 11.5, 200.0),
        minuteBar(7, open + 2 * 60, 11.5, 11.6, 11.0, 11.2, 50.0),
        minuteBar(7, open + 3 * 60, 11.2, 13.0, 11.0, 12.8, 300.0),
        minuteBar(7, open + 4 * 60, 12.8, 12.9, 12.0, 12.1, 150.0),
    };
    const auto out =
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Minute5, kTz);
    REQUIRE(out.size() == 1);
    CHECK(out[0].instrument_id == 7);
    CHECK(out[0].timeframe_s == 300);
    CHECK(out[0].ts == open);
    CHECK(out[0].open == Catch::Approx(10.0));
    CHECK(out[0].high == Catch::Approx(13.0));
    CHECK(out[0].low == Catch::Approx(9.0));
    CHECK(out[0].close == Catch::Approx(12.1));
    CHECK(out[0].volume == Catch::Approx(800.0));
}

TEST_CASE("5m buckets start at the session open")
{
    const auto open = ny("2025-01-15 09:30");
    const auto in = sequentialMinutes(1, open, 10);
    const auto out =
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Minute5, kTz);
    REQUIRE(out.size() == 2);
    CHECK(out[0].ts == open);
    CHECK(out[1].ts == open + 300);
    CHECK(out[0].timeframe_s == 300);
    CHECK(out[1].timeframe_s == 300);
}

TEST_CASE("5m does not merge minutes across session dates")
{
    const auto day1_last = ny("2025-01-15 15:58");
    const auto day2_open = ny("2025-01-16 09:30");
    const std::vector<terminal::Bar> in{
        minuteBar(1, day1_last, 10.0, 10.0, 10.0, 10.0, 1.0),
        minuteBar(1, day1_last + 60, 11.0, 11.0, 11.0, 11.0, 1.0),
        minuteBar(1, day2_open, 20.0, 20.0, 20.0, 20.0, 1.0),
        minuteBar(1, day2_open + 60, 21.0, 21.0, 21.0, 21.0, 1.0),
    };
    const auto out =
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Minute5, kTz);
    REQUIRE(out.size() == 2);
    CHECK(out[0].ts == ny("2025-01-15 15:55"));
    CHECK(out[0].open == Catch::Approx(10.0));
    CHECK(out[0].close == Catch::Approx(11.0));
    CHECK(out[1].ts == day2_open);
    CHECK(out[1].open == Catch::Approx(20.0));
    CHECK(out[1].close == Catch::Approx(21.0));
}

TEST_CASE("5m still emits a bar when minutes inside the bucket are missing")
{
    const auto open = ny("2025-01-15 09:30");
    const std::vector<terminal::Bar> in{
        minuteBar(1, open + 0 * 60, 10.0, 11.0, 9.0, 10.0, 1.0),
        minuteBar(1, open + 1 * 60, 10.0, 12.0, 10.0, 11.0, 2.0),
        minuteBar(1, open + 4 * 60, 11.0, 13.0, 8.0, 12.0, 3.0),
    };
    const auto out =
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Minute5, kTz);
    REQUIRE(out.size() == 1);
    CHECK(out[0].ts == open);
    CHECK(out[0].open == Catch::Approx(10.0));
    CHECK(out[0].high == Catch::Approx(13.0));
    CHECK(out[0].low == Catch::Approx(8.0));
    CHECK(out[0].close == Catch::Approx(12.0));
    CHECK(out[0].volume == Catch::Approx(6.0));
}

TEST_CASE("empty 5m windows are omitted and empty input stays empty")
{
    CHECK(terminal::transformChartBars({}, terminal::ChartBarPeriod::Minute5, kTz).empty());

    const auto open = ny("2025-01-15 09:30");
    const std::vector<terminal::Bar> in{
        minuteBar(1, open, 10.0, 10.0, 10.0, 10.0, 1.0),
        minuteBar(1, open + 10 * 60, 11.0, 11.0, 11.0, 11.0, 1.0),
    };
    const auto out =
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Minute5, kTz);
    REQUIRE(out.size() == 2);
    CHECK(out[0].ts == open);
    CHECK(out[1].ts == open + 600);
}

TEST_CASE("non-1m and non-RTH source bars are skipped")
{
    const auto pre = ny("2025-01-15 09:29");
    const auto open = ny("2025-01-15 09:30");
    const auto close = ny("2025-01-15 16:00");
    terminal::Bar already_5m = minuteBar(1, open + 60, 9.0, 9.0, 9.0, 9.0, 9.0);
    already_5m.timeframe_s = 300;
    const std::vector<terminal::Bar> in{
        minuteBar(1, pre, 1.0, 1.0, 1.0, 1.0, 1.0),
        minuteBar(1, open, 10.0, 11.0, 9.0, 10.5, 4.0),
        already_5m,
        minuteBar(1, close, 8.0, 8.0, 8.0, 8.0, 8.0),
    };
    const auto out =
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Minute5, kTz);
    REQUIRE(out.size() == 1);
    CHECK(out[0].ts == open);
    CHECK(out[0].open == Catch::Approx(10.0));
    CHECK(out[0].close == Catch::Approx(10.5));
    CHECK(out[0].volume == Catch::Approx(4.0));
}

TEST_CASE("full RTH session is 78 five-minute bars and 26 fifteen-minute bars")
{
    const auto open = ny("2025-01-15 09:30");
    const auto in = sequentialMinutes(1, open, terminal::kUsRthExpected1m);
    const auto five =
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Minute5, kTz);
    const auto fifteen =
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Minute15, kTz);
    REQUIRE(five.size() == 78);
    REQUIRE(fifteen.size() == 26);
    CHECK(five.front().ts == open);
    CHECK(five.back().ts == ny("2025-01-15 15:55"));
    CHECK(fifteen.front().ts == open);
    CHECK(fifteen.back().ts == ny("2025-01-15 15:45"));
}

TEST_CASE("1h is session-open aligned and the last RTH bar is 30 minutes")
{
    const auto open = ny("2025-01-15 09:30");
    const auto in = sequentialMinutes(1, open, terminal::kUsRthExpected1m);
    const auto out =
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Hour1, kTz);
    REQUIRE(out.size() == 7);
    CHECK(out[0].ts == open);
    CHECK(out[0].ts != open - 1800);  // not 09:00 local / not UTC-hour floor
    CHECK(out[1].ts == ny("2025-01-15 10:30"));
    CHECK(out[5].ts == ny("2025-01-15 14:30"));
    CHECK(out[6].ts == ny("2025-01-15 15:30"));
    CHECK(out[6].timeframe_s == 3600);
    CHECK(out[0].volume == Catch::Approx(60.0));
    CHECK(out[6].volume == Catch::Approx(30.0));
    CHECK(out[0].open == Catch::Approx(100.0));
    CHECK(out[0].close == Catch::Approx(159.5));
    CHECK(out[6].close == Catch::Approx(489.5));
}

TEST_CASE("1h alignment follows 09:30 local across DST")
{
    const auto open = ny("2025-03-10 09:30");
    const auto in = sequentialMinutes(1, open, 60);
    const auto out =
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Hour1, kTz);
    REQUIRE(out.size() == 1);
    CHECK(out[0].ts == open);
    CHECK(out[0].ts != open - (open % 3600));  // not the UTC hour floor (13:00)
}

TEST_CASE("daily is one RTH session bar not a UTC day bucket")
{
    const auto day1 = ny("2025-01-15 09:30");
    const auto day2 = ny("2025-01-16 09:30");
    auto in = sequentialMinutes(3, day1, 3);
    const auto day2_bars = sequentialMinutes(3, day2, 2);
    in.insert(in.end(), day2_bars.begin(), day2_bars.end());

    const auto out = terminal::transformChartBars(in, terminal::ChartBarPeriod::Day1, kTz);
    REQUIRE(out.size() == 2);
    CHECK(out[0].instrument_id == 3);
    CHECK(out[0].timeframe_s == 86400);
    CHECK(out[0].ts == day1);
    CHECK(out[0].open == Catch::Approx(100.0));
    CHECK(out[0].close == Catch::Approx(102.5));
    CHECK(out[0].volume == Catch::Approx(3.0));
    CHECK(out[1].ts == day2);
    CHECK(out[1].open == Catch::Approx(100.0));
    CHECK(out[1].close == Catch::Approx(101.5));
    CHECK(out[1].volume == Catch::Approx(2.0));
}

TEST_CASE("empty timezone throws at transform time")
{
    const auto open = ny("2025-01-15 09:30");
    const auto in = sequentialMinutes(1, open, 5);
    CHECK_THROWS_AS(
        terminal::transformChartBars(in, terminal::ChartBarPeriod::Minute5, ""),
        std::runtime_error);
}

TEST_CASE("transform does not persist higher-timeframe rows")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(aaplInstrument());
    ingestMinutes(store, id, 20250115, 5);

    const auto loaded = store.queryBars(id, terminal::kTimeframe1m, 0, 4'000'000'000);
    const auto five =
        terminal::transformChartBars(loaded, terminal::ChartBarPeriod::Minute5, kTz);
    REQUIRE(five.size() == 1);
    CHECK(store.queryBars(id, 300, 0, 4'000'000'000).empty());
    CHECK(store.queryBars(id, terminal::kTimeframe1m, 0, 4'000'000'000).size() == 5);
}

TEST_CASE("loadChartBars Minute5 composites the 1-minute snapshot")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(aaplInstrument());
    ingestMinutes(store, id, 20250115, 5);

    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.period = terminal::ChartBarPeriod::Minute5;
    const auto result = terminal::loadChartBars(store, settings);
    REQUIRE(result.status == terminal::ChartLoadStatus::Ready);
    REQUIRE(result.bars.size() == 1);
    CHECK(result.bars[0].timeframe_s == 300);
    CHECK(result.bars[0].ts == ny("2025-01-15 09:30"));
    CHECK(result.message.find("5m") != std::string::npos);
    CHECK(store.queryBars(id, 300, result.ts_begin, result.ts_end).empty());
}

TEST_CASE("loadChartBars Minute5 still skips 0-bar holidays")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(aaplInstrument());
    ingestMinutes(store, id, 20250113, 5);
    ingestMinutes(store, id, 20250114, 5);
    const auto holiday = store.ingestSession({}, id, terminal::kTimeframe1m, 20250115, 0);
    CHECK(holiday.coverage.bar_count == 0);
    ingestMinutes(store, id, 20250116, 5);

    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.period = terminal::ChartBarPeriod::Minute5;
    settings.intraday_session_count = 3;
    const auto result = terminal::loadChartBars(store, settings);
    REQUIRE(result.status == terminal::ChartLoadStatus::Ready);
    CHECK(result.sessions_used == 3);
    CHECK(result.first_session == 20250113);
    CHECK(result.last_session == 20250116);
    CHECK(result.bars.size() == 3);
    CHECK(result.bars[0].timeframe_s == 300);
}

TEST_CASE("loadChartBars Day1 ignores 1m rows until daily bars exist")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(aaplInstrument());
    ingestMinutes(store, id, 20250115, 5);
    ingestMinutes(store, id, 20250116, 5);

    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.period = terminal::ChartBarPeriod::Day1;
    settings.historical_session_count = 2;
    const auto result = terminal::loadChartBars(store, settings);
    CHECK(result.status == terminal::ChartLoadStatus::Empty);
    CHECK(result.bars.empty());
}

TEST_CASE("loadChartBars still rejects non-candlestick and non-session limiters")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    store.insertInstrument(aaplInstrument());

    terminal::CChartSettings ohlc;
    ohlc.symbol = "AAPL";
    ohlc.bar_type = terminal::ChartBarType::Ohlc;
    const auto a = terminal::loadChartBars(store, ohlc);
    CHECK(a.status == terminal::ChartLoadStatus::Unsupported);
    CHECK(a.bars.empty());

    terminal::CChartSettings counted;
    counted.symbol = "AAPL";
    counted.period = terminal::ChartBarPeriod::Minute5;
    counted.limit_mode = terminal::ChartDataLimitMode::BarCount;
    const auto b = terminal::loadChartBars(store, counted);
    CHECK(b.status == terminal::ChartLoadStatus::Unsupported);
    CHECK(b.bars.empty());
}
