// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/TradingFormat.h"

#include "market_data/Time.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

namespace terminal {
namespace {

void appendGrouped(std::string& out, std::string_view whole)
{
    const std::size_t lead = whole.size() % 3;
    for (std::size_t index = 0; index < whole.size(); ++index)
    {
        if (index > 0 && (index + 3 - lead) % 3 == 0)
        {
            out.push_back(',');
        }
        out.push_back(whole[index]);
    }
}

[[nodiscard]] std::string formatFixed(double amount, int decimals, bool money, bool trim, bool grouped = true)
{
    if (!std::isfinite(amount) || decimals < 0 || decimals > 8)
    {
        return {};
    }
    char raw[96];
    const int written = std::snprintf(raw, sizeof(raw), "%.*f", decimals, std::fabs(amount));
    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(raw))
    {
        return {};
    }
    const std::string_view text(raw, static_cast<std::size_t>(written));
    const bool zero = std::ranges::all_of(text, [](char ch) { return ch == '0' || ch == '.'; });
    const std::size_t dot = text.find('.');
    const std::string_view whole = dot == std::string_view::npos ? text : text.substr(0, dot);
    std::string fraction;
    if (dot != std::string_view::npos)
    {
        fraction.assign(text.substr(dot));
        if (trim)
        {
            while (!fraction.empty() && fraction.back() == '0')
            {
                fraction.pop_back();
            }
            if (fraction == ".")
            {
                fraction.clear();
            }
        }
    }
    std::string out;
    if (amount < 0.0 && !zero)
    {
        out.push_back('-');
    }
    if (money)
    {
        out.push_back('$');
    }
    if (grouped)
    {
        appendGrouped(out, whole);
    }
    else
    {
        out += whole;
    }
    out += fraction;
    return out;
}

[[nodiscard]] std::string trimmed(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
    {
        text.remove_suffix(1);
    }
    return std::string(text);
}

}  // namespace

std::string formatMoney(double amount, int decimals)
{
    return formatFixed(amount, decimals, true, false);
}

std::string formatQuantity(double quantity)
{
    return formatFixed(quantity, 4, false, true);
}

std::string formatPrice(double price)
{
    const double magnitude = std::fabs(price);
    return formatFixed(price, magnitude > 0.0 && magnitude < 1.0 ? 4 : 2, false, false);
}

std::string formatPercent(double fraction, int decimals)
{
    if (!std::isfinite(fraction))
    {
        return {};
    }
    return formatFixed(fraction * 100.0, decimals, false, false) + "%";
}

std::string formatLedgerTime(UnixSeconds ts)
{
    using namespace std::chrono;
    try
    {
        const zoned_time local{kLedgerTimezone, sys_seconds{seconds{ts}}};
        const local_seconds when = local.get_local_time();
        const local_days day = floor<days>(when);
        const year_month_day ymd{day};
        const hh_mm_ss clock{when - day};
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u %02d:%02d", static_cast<int>(ymd.year()),
                      static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()),
                      static_cast<int>(clock.hours().count()), static_cast<int>(clock.minutes().count()));
        return buf;
    }
    catch (const std::exception&)
    {
        return {};
    }
}

std::string formatDuration(UnixSeconds seconds)
{
    const UnixSeconds total = std::max<UnixSeconds>(seconds, 0);
    const UnixSeconds days = total / 86'400;
    const UnixSeconds hours = (total % 86'400) / 3'600;
    const UnixSeconds minutes = (total % 3'600) / 60;
    char buf[48];
    if (days > 0)
    {
        std::snprintf(buf, sizeof(buf), "%lldd %lldh", static_cast<long long>(days), static_cast<long long>(hours));
    }
    else if (hours > 0)
    {
        std::snprintf(buf, sizeof(buf), "%lldh %lldm", static_cast<long long>(hours),
                      static_cast<long long>(minutes));
    }
    else
    {
        std::snprintf(buf, sizeof(buf), "%lldm", static_cast<long long>(minutes));
    }
    return buf;
}

std::optional<UnixSeconds> parseLedgerTime(std::string_view text, UnixSeconds now)
{
    const std::string value = trimmed(text);
    if (value.empty())
    {
        return now;
    }
    std::string naive;
    if (value.size() == 8 && std::ranges::all_of(value, [](char ch) { return ch >= '0' && ch <= '9'; }))
    {
        naive = value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2);
    }
    else
    {
        naive = value;
    }
    if (naive.size() == 10)
    {
        if (!tryParseIsoDate(naive).has_value())
        {
            return std::nullopt;
        }
        naive += " 16:00";
    }
    try
    {
        return naiveLocalToUtc(kLedgerTimezone, naive);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

std::optional<double> parseLedgerNumber(std::string_view text)
{
    std::string compact;
    compact.reserve(text.size());
    for (const char ch : text)
    {
        if (ch == ',' || ch == ' ' || ch == '\t')
        {
            continue;
        }
        compact.push_back(ch);
    }
    std::size_t start = 0;
    bool negative = false;
    if (!compact.empty() && compact[0] == '-')
    {
        negative = true;
        start = 1;
    }
    if (start < compact.size() && compact[start] == '$')
    {
        ++start;
    }
    const std::string_view digits = std::string_view(compact).substr(start);
    if (digits.empty() || digits.front() == '-' || digits.front() == '+')
    {
        return std::nullopt;
    }
    double value = 0.0;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() || !std::isfinite(value))
    {
        return std::nullopt;
    }
    return negative ? -value : value;
}

std::string contractLabel(const PositionKey& key)
{
    if (key.kind != TradeAssetKind::Option)
    {
        return {};
    }
    return formatSessionDate(key.expiration) + " " + formatFixed(key.strike, 4, false, true, false) +
           (key.right == OptionRight::Call ? "C" : "P");
}

}  // namespace terminal
