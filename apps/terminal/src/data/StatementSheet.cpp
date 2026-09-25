// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "data/StatementSheet.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

namespace terminal {
namespace {

constexpr std::string_view kPreferredLines[] = {
    "revenue", "cor", "gp", "opex", "opinc", "ebit", "ebitda", "netinc", "netinccmn", "eps",
    "epsdil",
};

[[nodiscard]] bool isLabelLine(std::string_view line_item)
{
    return line_item == "fiscalYear" || line_item == "fiscalQuarter";
}

[[nodiscard]] bool containsLine(const std::vector<std::string>& lines, std::string_view line_item)
{
    return std::ranges::find(lines, line_item) != lines.end();
}

[[nodiscard]] std::string groupDigits(std::string_view digits)
{
    std::string out;
    const std::size_t count = digits.size();
    out.reserve(count + (count / 3));
    for (std::size_t index = 0; index < count; ++index)
    {
        if (index > 0 && (count - index) % 3 == 0)
        {
            out.push_back(',');
        }
        out.push_back(digits[index]);
    }
    return out;
}

[[nodiscard]] std::string formatInteger(std::int64_t value)
{
    const bool negative = value < 0;
    std::uint64_t magnitude = 0;
    if (value == std::numeric_limits<std::int64_t>::min())
    {
        magnitude = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1U;
    }
    else if (negative)
    {
        magnitude = static_cast<std::uint64_t>(-value);
    }
    else
    {
        magnitude = static_cast<std::uint64_t>(value);
    }
    std::string text = groupDigits(std::to_string(magnitude));
    if (negative)
    {
        text.insert(text.begin(), '-');
    }
    return text;
}

[[nodiscard]] std::string formatReal(double value)
{
    if (!std::isfinite(value))
    {
        return {};
    }
    const bool negative = std::signbit(value) && value != 0.0;
    char buf[128];
    const int wrote = std::snprintf(buf, sizeof(buf), "%.4f", std::fabs(value));
    if (wrote < 0 || std::cmp_greater_equal(wrote, sizeof(buf)))
    {
        std::snprintf(buf, sizeof(buf), "%.4g", std::fabs(value));
        std::string fallback = buf;
        if (negative)
        {
            fallback.insert(fallback.begin(), '-');
        }
        return fallback;
    }
    std::string text = buf;
    if (const auto dot = text.find('.'); dot != std::string::npos)
    {
        while (!text.empty() && text.back() == '0')
        {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.')
        {
            text.pop_back();
        }
    }
    const auto dot = text.find('.');
    const std::string_view whole = dot == std::string::npos
                                       ? std::string_view{text}
                                       : std::string_view{text}.substr(0, dot);
    const std::string fraction = dot == std::string::npos ? std::string{} : text.substr(dot);
    std::string grouped = groupDigits(whole);
    if (negative)
    {
        grouped.insert(grouped.begin(), '-');
    }
    grouped += fraction;
    return grouped;
}

}  // namespace

StatementSheet buildStatementSheet(std::span<const StatementCell> cells)
{
    std::vector<std::string> dates;
    for (const StatementCell& cell : cells)
    {
        if (cell.period_end == "TTM" || containsLine(dates, cell.period_end))
        {
            continue;
        }
        dates.push_back(cell.period_end);
    }
    std::ranges::sort(dates);
    if (static_cast<int>(dates.size()) > kStatementSheetPeriods)
    {
        dates.erase(dates.begin(), dates.end() - kStatementSheetPeriods);
    }
    std::ranges::reverse(dates);

    std::vector<std::string> items;
    for (const StatementCell& cell : cells)
    {
        if (isLabelLine(cell.line_item) || !containsLine(dates, cell.period_end) ||
            containsLine(items, cell.line_item))
        {
            continue;
        }
        items.push_back(cell.line_item);
    }

    std::vector<std::string> ordered;
    for (const std::string_view preferred : kPreferredLines)
    {
        if (containsLine(items, preferred))
        {
            ordered.emplace_back(preferred);
        }
    }
    for (const std::string& line_item : items)
    {
        if (!containsLine(ordered, line_item))
        {
            ordered.push_back(line_item);
        }
    }

    StatementSheet sheet;
    sheet.period_count = static_cast<int>(dates.size());
    for (int index = 0; index < sheet.period_count; ++index)
    {
        sheet.periods[static_cast<std::size_t>(index)] = dates[static_cast<std::size_t>(index)];
    }
    sheet.rows.reserve(ordered.size());
    for (const std::string& line_item : ordered)
    {
        StatementSheetRow row;
        row.line_item = line_item;
        row.label = statementLineLabel(line_item);
        for (const StatementCell& cell : cells)
        {
            if (cell.line_item != line_item)
            {
                continue;
            }
            const auto found = std::ranges::find(dates, cell.period_end);
            if (found == dates.end())
            {
                continue;
            }
            const auto index = static_cast<std::size_t>(found - dates.begin());
            row.slots[index].present = true;
            row.slots[index].value = cell.value;
        }
        sheet.rows.push_back(std::move(row));
    }
    return sheet;
}

std::string formatStatementValue(const StatementValue& value)
{
    if (const auto* integer = std::get_if<std::int64_t>(&value))
    {
        return formatInteger(*integer);
    }
    if (const auto* real = std::get_if<double>(&value))
    {
        return formatReal(*real);
    }
    if (const auto* text = std::get_if<std::string>(&value))
    {
        return *text;
    }
    return {};
}

}  // namespace terminal
