// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "backtest/Strategy.h"
#include "backtest/strategies/Direction.h"
#include "market_data/Time.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <span>
#include <vector>

namespace terminal {
namespace {

// Minute slots from the 09:30 open; slot k is the bar that opens at 09:30 + k.
constexpr int kSlots = 390;
constexpr int kFirstEntrySlot = 15;   // 09:45 close; the open is price discovery, not a shock
constexpr int kLastEntrySlot = 360;   // 15:30 close
constexpr int kExitSlot = 388;        // 15:58 close, so the exit fills at the 15:59 open
constexpr int kMinLocalReturns = 10;  // today's one-minute returns needed before the shock window
constexpr const char* kTimezone = "America/New_York";

constexpr StudyOption kOptions[] = {
    {.key = "window", .label = "Shock window (minutes)", .min = 2, .max = 60, .fallback = 5},
    {.key = "threshold", .label = "Threshold (0.1 sigma)", .min = 10, .max = 200, .fallback = 40},
    {.key = "lookback", .label = "Profile sessions", .min = 5, .max = 250, .fallback = 20},
    {.key = "local", .label = "Local minutes", .min = kMinLocalReturns, .max = 240, .fallback = 30},
    {.key = "retrace", .label = "Take profit (% of shock retraced)", .min = 5, .max = 200, .fallback = 50},
    {.key = "extend", .label = "Stop (% extension of shock, 0 = none)", .min = 0, .max = 1000, .fallback = 100},
    {.key = "hold", .label = "Time stop (minutes)", .min = 1, .max = kSlots, .fallback = 30},
    {.key = "direction", .label = "Direction", .choices = kDirectionChoices, .fallback = 1},
};

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

using SlotValues = std::array<double, kSlots>;

// Squared one-minute log returns of past sessions, by slot, and their mean.
class Profile
{
public:
    explicit Profile(std::size_t lookback) : lookback_(lookback)
    {
        mean_.fill(kNaN);
    }

    [[nodiscard]] bool ready() const noexcept
    {
        return sessions_.size() >= lookback_;
    }

    // Mean squared return at `slot` over the stored sessions that have it; NaN when none do.
    [[nodiscard]] double mean(int slot) const noexcept
    {
        return mean_[static_cast<std::size_t>(slot)];
    }

    void push(const SlotValues& squared)
    {
        sessions_.push_back(squared);
        if (sessions_.size() > lookback_)
        {
            sessions_.pop_front();
        }
        for (std::size_t slot = 0; slot < mean_.size(); ++slot)
        {
            double sum = 0.0;
            int count = 0;
            for (const SlotValues& session : sessions_)
            {
                if (!std::isnan(session[slot]))
                {
                    sum += session[slot];
                    ++count;
                }
            }
            mean_[slot] = count > 0 ? sum / count : kNaN;
        }
    }

private:
    std::size_t lookback_;
    std::deque<SlotValues> sessions_;
    SlotValues mean_{};
};

struct Settings
{
    int window{};
    double threshold{};
    int local{};
    double retrace{};
    double extend{};  // 0: no stop
    int hold{};
    bool shorts{};
};

class ShockReverter
{
public:
    ShockReverter(const Settings& settings, std::size_t lookback) : settings_(settings), profile_(lookback) {}

    // The target after a regular-hours one-minute bar at `slot` of the current session.
    [[nodiscard]] double step(int slot, double close)
    {
        recordClose(slot, close);
        const double z = shockScore(slot);
        exitIfDone(slot, close);
        if (held_ == 0 && slot >= kFirstEntrySlot && slot <= kLastEntrySlot && std::abs(z) >= settings_.threshold)
        {
            const int side = z > 0.0 ? -1 : 1;
            const bool allowed = side > 0 ? !blocked_long_ : (settings_.shorts && !blocked_short_);
            if (allowed)
            {
                held_ = side;
                signal_slot_ = slot;
                p0_ = closes_[static_cast<std::size_t>(slot - settings_.window)];
                p1_ = close;
            }
        }
        return static_cast<double>(held_);
    }

    // Files today's returns into the profile and starts a new session flat.
    void startSession(bool first)
    {
        if (!first)
        {
            profile_.push(squared_);
        }
        squared_.fill(kNaN);
        closes_.fill(kNaN);
        last_slot_ = -1;
        held_ = 0;
        blocked_long_ = false;
        blocked_short_ = false;
    }

    [[nodiscard]] int lastSlot() const noexcept
    {
        return last_slot_;
    }

private:
    // Carries the close over missing minutes, whose one-minute return is 0.
    void recordClose(int slot, double close)
    {
        const auto at = static_cast<std::size_t>(slot);
        if (last_slot_ >= 0)
        {
            const double previous = closes_[static_cast<std::size_t>(last_slot_)];
            for (int gap = last_slot_ + 1; gap < slot; ++gap)
            {
                closes_[static_cast<std::size_t>(gap)] = previous;
                squared_[static_cast<std::size_t>(gap)] = 0.0;
            }
            const double r = std::log(close / previous);
            squared_[at] = r * r;
        }
        closes_[at] = close;
        last_slot_ = slot;
    }

    // The n-minute log move over the larger of the usual variance for these minutes of
    // the day and n times today's mean squared return before the window. NaN until both exist.
    [[nodiscard]] double shockScore(int slot) const
    {
        const int n = settings_.window;
        if (!profile_.ready() || slot < n)
        {
            return kNaN;
        }
        const double start = closes_[static_cast<std::size_t>(slot - n)];
        if (std::isnan(start))
        {
            return kNaN;
        }
        double usual = 0.0;
        for (int j = slot - n + 1; j <= slot; ++j)
        {
            usual += profile_.mean(j);
        }
        double sum = 0.0;
        int count = 0;
        for (int j = std::max(1, slot - n - settings_.local + 1); j <= slot - n; ++j)
        {
            const double value = squared_[static_cast<std::size_t>(j)];
            if (!std::isnan(value))
            {
                sum += value;
                ++count;
            }
        }
        if (std::isnan(usual) || count < kMinLocalReturns)
        {
            return kNaN;
        }
        const double today = n * sum / count;
        const double variance = std::max(usual, today);
        if (!(variance > 0.0))
        {
            return kNaN;
        }
        return std::log(closes_[static_cast<std::size_t>(slot)] / start) / std::sqrt(variance);
    }

    // Take profit, stop, time, then the end of the day, each on this bar's close.
    void exitIfDone(int slot, double close)
    {
        if (held_ == 0)
        {
            return;
        }
        const double shock = p1_ - p0_;
        const double target = p1_ - (settings_.retrace * shock);
        const double stop = p1_ + (settings_.extend * shock);
        const bool longs = held_ > 0;
        const bool took_profit = longs ? close >= target : close <= target;
        const bool stopped = !took_profit && settings_.extend > 0.0 && (longs ? close <= stop : close >= stop);
        if (stopped)
        {
            // A fade that failed this badly says the day is not reverting that way.
            (longs ? blocked_long_ : blocked_short_) = true;
        }
        if (took_profit || stopped || slot - signal_slot_ >= settings_.hold || slot >= kExitSlot)
        {
            held_ = 0;
        }
    }

    Settings settings_;
    Profile profile_;
    SlotValues squared_{};
    SlotValues closes_{};
    int last_slot_{-1};
    int held_{0};
    int signal_slot_{0};
    double p0_{0.0};
    double p1_{0.0};
    bool blocked_long_{false};
    bool blocked_short_{false};
};

// Fades a move that is extreme for both this time of day and today's own tape.
// One-minute regular-hours bars only; other bars get no opinion.
void processShockRevert(std::span<const Bar> bars, std::span<const int> options, std::vector<double>& targets)
{
    Settings settings;
    settings.window = options[0];
    settings.threshold = static_cast<double>(options[1]) / 10.0;
    settings.local = options[3];
    settings.retrace = static_cast<double>(options[4]) / 100.0;
    settings.extend = static_cast<double>(options[5]) / 100.0;
    settings.hold = options[6];
    settings.shorts = directionAllowsShort(options[7]);
    ShockReverter reverter(settings, static_cast<std::size_t>(options[2]));

    SessionDate session{};
    UtcWindow window{.start = 0, .end = 0};
    bool started = false;
    for (const Bar& bar : bars)
    {
        if (bar.timeframe_s != kTimeframe1m)
        {
            targets.push_back(kNaN);
            continue;
        }
        if (bar.ts < window.start || bar.ts >= window.end)
        {
            const SessionDate date = utcToSessionDate(kTimezone, bar.ts);
            const UtcWindow rth = usRthUtcWindow(kTimezone, date);
            if (bar.ts < rth.start || bar.ts >= rth.end)
            {
                targets.push_back(kNaN);
                continue;
            }
            if (!started || date != session)
            {
                reverter.startSession(!started);
                started = true;
                session = date;
            }
            window = rth;
        }
        const auto slot = static_cast<int>((bar.ts - window.start) / kTimeframe1m);
        if (slot <= reverter.lastSlot() || !(bar.close > 0.0))
        {
            targets.push_back(kNaN);
            continue;
        }
        targets.push_back(reverter.step(slot, bar.close));
    }
}

constexpr StrategyType kType{
    .id = "shock_revert",
    .display_name = "Intraday shock reversion",
    .note = "1-minute bars. Fades a move whose size over the window, in sigmas of both the usual variance for "
            "that time of day and today's own, passes the threshold. Exits on the retrace, the stop, the time "
            "stop, or 15:58, and enters only from 09:45 to 15:30. Trades only after Profile sessions of bars, so "
            "widen the range past the 20-day default, and flatten at session end. On QQQ 2021-2026 it did not "
            "beat costs (reports/qqq-intraday-reversion).",
    .options = kOptions,
    .process = &processShockRevert,
};

struct Registration
{
    Registration() noexcept
    {
        registerStrategy(kType);
    }
};

[[maybe_unused]] const Registration kShockRevertRegistration{};

}  // namespace
}  // namespace terminal
