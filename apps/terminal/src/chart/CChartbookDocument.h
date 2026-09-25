// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartSettings.h"
#include "chart/CChartView.h"
#include "chart/CStudy.h"
#include "options/OptionPayoff.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

inline constexpr int kChartbookFormatVersion = 1;
inline constexpr float kChartbookDefaultDataRatio = 0.30f;
inline constexpr const char* kChartbookFileSuffix = ".chartbook.json";

// Summary table column ids, declaration order. Matches InventoryPanel.
// Books written before the FIGI column name it "exch"; the reader maps that id to "figi".
inline constexpr const char* kChartbookDataColumns[] = {
    "symbol", "figi", "tf", "bars", "sess", "ok", "part", "miss", "err", "first", "last",
};
inline constexpr int kChartbookDataColumnCount = 11;

[[nodiscard]] bool isChartbookDataColumn(std::string_view id) noexcept;

enum class ChartbookSplitAxis : std::uint8_t
{
    Horizontal = 0,  // first = left, second = right
    Vertical         // first = top, second = bottom
};

struct ChartbookColumn
{
    std::string id;
    float width{0.f};
    bool visible{true};
    int order{0};
};

// False unless columns names every id in 0..column_count-1 once and the orders are a permutation.
[[nodiscard]] bool chartbookColumnDisplayOrders(const std::vector<ChartbookColumn>& columns, int column_count,
                                                std::vector<int>& orders);

// DATA panel fields that belong to one chart space. Empty selected_* means no row.
// sort_column empty means the summary table is unsorted.
struct ChartbookData
{
    std::string symbol;
    std::string from;
    std::string to;
    std::string ingest_timeframe{"1m"};
    std::string selected_symbol;
    std::string selected_timeframe;
    std::string sort_column;
    bool sort_descending{false};
    std::vector<ChartbookColumn> columns;
};

struct ChartbookFloating
{
    std::string window;
    float x{0.f};
    float y{0.f};
    float w{0.f};
    float h{0.f};
};

// Flat tree. root < 0 means an empty dock. A split node's first/second are indexes.
// A leaf uses windows ("data", "financials:<id>", "options:<id>", "portfolio:<id>", "payoff:<id>", or
// "pane:<id>") and selected.
struct ChartbookLayoutNode
{
    bool is_split{false};
    ChartbookSplitAxis axis{ChartbookSplitAxis::Horizontal};
    float ratio{0.5f};
    int first{-1};
    int second{-1};
    std::vector<std::string> windows;
    std::string selected;
};

struct ChartbookLayout
{
    std::vector<ChartbookLayoutNode> nodes;
    int root{-1};
};

// One financials sheet. statement is income|balance|cashflow.
// timeframe is annually|quarterly. A chartbook holds several, the same way it holds chart panes.
struct ChartbookFinancials
{
    int id{0};
    int link_group{0};  // 0 ungrouped, 1..4 a symbol group. Omitted from the file when 0.
    std::string symbol;
    std::string figi;  // empty until the symbol resolves to an instrument with a FIGI
    std::string statement{"income"};
    std::string timeframe{"annually"};
};

// Call-side fields, left to right. Strike is not a field: the chain keeps it in the middle
// and mirrors the selected fields onto the put side.
struct OptionChainColumn
{
    const char* id;
    const char* label;
};

inline constexpr OptionChainColumn kOptionChainColumns[] = {
    {"bid", "Bid"},   {"ask", "Ask"},   {"last", "Last"}, {"chg", "Chg"}, {"pct", "%"},
    {"mid", "Mid"},   {"iv", "IV"},     {"delta", "Delta"}, {"theta", "Theta"}, {"vega", "Vega"},
    {"rho", "Rho"},   {"vol", "Vol"},   {"oi", "OI"},     {"oi_chg", "OI Chg"},
};
inline constexpr int kOptionChainColumnCount =
    static_cast<int>(sizeof(kOptionChainColumns) / sizeof(kOptionChainColumns[0]));

// The chain as it looked before the column menu: bid through open interest.
inline constexpr const char* kOptionChainDefaultColumns[] = {
    "bid", "ask", "last", "iv", "delta", "vol", "oi",
};
inline constexpr int kOptionChainDefaultColumnCount =
    static_cast<int>(sizeof(kOptionChainDefaultColumns) / sizeof(kOptionChainDefaultColumns[0]));

[[nodiscard]] constexpr int optionChainColumnIndex(std::string_view id) noexcept
{
    for (int index = 0; index < kOptionChainColumnCount; ++index)
    {
        if (id == kOptionChainColumns[index].id)
        {
            return index;
        }
    }
    return -1;
}

[[nodiscard]] constexpr bool isOptionChainColumn(std::string_view id) noexcept
{
    return optionChainColumnIndex(id) >= 0;
}

[[nodiscard]] inline const char* optionChainColumnLabel(std::string_view id) noexcept
{
    const int index = optionChainColumnIndex(id);
    if (index < 0)
    {
        return "";
    }
    return kOptionChainColumns[index].label;
}

[[nodiscard]] inline std::vector<std::string> defaultOptionChainColumns()
{
    std::vector<std::string> columns;
    columns.reserve(static_cast<std::size_t>(kOptionChainDefaultColumnCount));
    for (const char* id : kOptionChainDefaultColumns)
    {
        columns.emplace_back(id);
    }
    return columns;
}

[[nodiscard]] inline bool optionChainColumnsAreDefault(const std::vector<std::string>& columns) noexcept
{
    if (columns.size() != static_cast<std::size_t>(kOptionChainDefaultColumnCount))
    {
        return false;
    }
    for (int index = 0; index < kOptionChainDefaultColumnCount; ++index)
    {
        if (columns[static_cast<std::size_t>(index)] != kOptionChainDefaultColumns[index])
        {
            return false;
        }
    }
    return true;
}

// Inserts or erases id, keeping catalog order. False when id is unknown or already in that state.
inline bool setOptionChainColumnVisible(std::vector<std::string>& columns, std::string_view id, bool visible)
{
    const int catalog = optionChainColumnIndex(id);
    if (catalog < 0)
    {
        return false;
    }
    const auto found = std::ranges::find_if(columns, [id](const std::string& column) { return column == id; });
    if (!visible)
    {
        if (found == columns.end())
        {
            return false;
        }
        columns.erase(found);
        return true;
    }
    if (found != columns.end())
    {
        return false;
    }
    const auto insert_at = std::ranges::find_if(columns, [catalog](const std::string& existing) {
        return optionChainColumnIndex(existing) > catalog;
    });
    columns.emplace(insert_at, id);
    return true;
}

constexpr bool optionChainDefaultsAreOrdered()
{
    int previous = -1;
    for (const char* id : kOptionChainDefaultColumns)
    {
        const int index = optionChainColumnIndex(id);
        if (index <= previous)
        {
            return false;
        }
        previous = index;
    }
    return kOptionChainDefaultColumnCount > 0;
}
static_assert(optionChainDefaultsAreOrdered());

// One options chain. expiration is YYYYMMDD, or 0 when the panel has not chosen a date.
// expiration_type is weekly, monthly, or empty while expiration is 0.
// columns is the call-side field ids in catalog order. Empty shows the strike only.
// A chartbook that omits the key loads the default set, which is not empty.
struct ChartbookOptions
{
    int id{0};
    int link_group{0};  // 0 ungrouped, 1..4 a symbol group. Omitted from the file when 0.
    std::string symbol;
    std::string figi;  // empty until the symbol resolves to an instrument with a FIGI
    int expiration{0};
    std::string expiration_type;
    std::vector<std::string> columns{defaultOptionChainColumns()};
};

// One portfolio window. portfolio_id is the store id, or 0 when the window has not chosen a book.
struct ChartbookPortfolio
{
    int id{0};
    std::int64_t portfolio_id{0};
};

// One payoff wizard. The chain fields match ChartbookOptions. spot is the price
// typed by hand, or 0 while the wizard estimates it from the chain. legs is the
// strategy and validates as one set.
struct ChartbookPayoff
{
    int id{0};
    std::string symbol;
    std::string figi;
    int expiration{0};
    std::string expiration_type;
    double spot{0.0};
    std::vector<PayoffLeg> legs;
};

struct ChartbookPane
{
    int id{0};
    int link_group{0};  // 0 ungrouped, 1..4 a symbol group. Omitted from the file when 0.
    CChartSettings settings{};
    ChartInteractiveScale interactive{ChartInteractiveScale::Move};
    std::vector<float> region_ratios;
    int next_study_id{1};
    std::vector<CStudyInstance> studies;
};

struct CChartbookDocument
{
    int format{kChartbookFormatVersion};
    std::string name;
    int focused_pane{0};
    int next_pane_id{1};
    int focused_financials{0};
    int next_financials_id{1};
    int focused_options{0};
    int next_options_id{1};
    int focused_portfolio{0};
    int next_portfolio_id{1};
    int focused_payoff{0};
    int next_payoff_id{1};
    ChartbookData data{};
    ChartbookLayout layout{};
    std::vector<ChartbookFloating> floating;
    std::vector<ChartbookPane> panes;
    std::vector<ChartbookFinancials> financials;
    std::vector<ChartbookOptions> options;
    std::vector<ChartbookPortfolio> portfolios;
    std::vector<ChartbookPayoff> payoffs;
};

[[nodiscard]] inline std::string paneWindowId(int pane_id)
{
    return "pane:" + std::to_string(pane_id);
}

[[nodiscard]] inline std::string financialsWindowId(int financials_id)
{
    return "financials:" + std::to_string(financials_id);
}

[[nodiscard]] inline std::string optionsWindowId(int options_id)
{
    return "options:" + std::to_string(options_id);
}

// True when window is "portfolio:<id>" and the id is a positive integer of at most 9 digits.
[[nodiscard]] inline std::string portfolioWindowId(int portfolio_id)
{
    return "portfolio:" + std::to_string(portfolio_id);
}

// True when window is "portfolio:<id>" and the id is a positive integer of at most 9 digits.
[[nodiscard]] inline bool portfolioIdFromWindow(std::string_view window, int& portfolio_id) noexcept
{
    constexpr std::string_view prefix = "portfolio:";
    if (!window.starts_with(prefix) || window.size() > prefix.size() + 9)
    {
        return false;
    }
    portfolio_id = 0;
    for (const char digit : window.substr(prefix.size()))
    {
        if (digit < '0' || digit > '9')
        {
            portfolio_id = 0;
            return false;
        }
        portfolio_id = (portfolio_id * 10) + (digit - '0');
    }
    return portfolio_id > 0;
}

[[nodiscard]] inline std::string payoffWindowId(int payoff_id)
{
    return "payoff:" + std::to_string(payoff_id);
}

// True when window is "payoff:<id>" and the id is a positive integer of at most 9 digits.
[[nodiscard]] inline bool payoffIdFromWindow(std::string_view window, int& payoff_id) noexcept
{
    constexpr std::string_view prefix = "payoff:";
    if (!window.starts_with(prefix) || window.size() > prefix.size() + 9)
    {
        return false;
    }
    payoff_id = 0;
    for (const char digit : window.substr(prefix.size()))
    {
        if (digit < '0' || digit > '9')
        {
            payoff_id = 0;
            return false;
        }
        payoff_id = (payoff_id * 10) + (digit - '0');
    }
    return payoff_id > 0;
}

// True when window is "options:<id>" and the id is a positive integer of at most 9 digits.
[[nodiscard]] inline bool optionsIdFromWindow(std::string_view window, int& options_id) noexcept
{
    constexpr std::string_view prefix = "options:";
    if (!window.starts_with(prefix) || window.size() > prefix.size() + 9)
    {
        return false;
    }
    options_id = 0;
    for (const char digit : window.substr(prefix.size()))
    {
        if (digit < '0' || digit > '9')
        {
            options_id = 0;
            return false;
        }
        options_id = (options_id * 10) + (digit - '0');
    }
    return options_id > 0;
}

[[nodiscard]] inline bool financialsIdFromWindow(std::string_view window, int& financials_id) noexcept
{
    constexpr std::string_view prefix = "financials:";
    if (!window.starts_with(prefix) || window.size() > prefix.size() + 9)
    {
        return false;
    }
    financials_id = 0;
    for (const char digit : window.substr(prefix.size()))
    {
        if (digit < '0' || digit > '9')
        {
            financials_id = 0;
            return false;
        }
        financials_id = (financials_id * 10) + (digit - '0');
    }
    return financials_id > 0;
}

[[nodiscard]] ChartbookLayout chartbookLeaf(std::vector<std::string> windows, std::string selected);

[[nodiscard]] ChartbookLayout chartbookSplit(ChartbookSplitAxis axis, float ratio,
                                             const ChartbookLayout& first, const ChartbookLayout& second);

// Dock a new chart into the space: an existing pane leaf, else the side opposite DATA, else the root.
void chartbookInsertPane(ChartbookLayout& layout, int pane_id);

// Dock DATA on the left at the default ratio when this space does not already show it.
void chartbookInsertData(ChartbookLayout& layout);

// Dock one financials sheet the same way a new chart pane docks.
void chartbookInsertFinancials(ChartbookLayout& layout, int financials_id);

// Dock one options chain the same way a new chart pane docks.
void chartbookInsertOptions(ChartbookLayout& layout, int options_id);

// Dock one portfolio window the same way a new chart pane docks.
void chartbookInsertPortfolio(ChartbookLayout& layout, int portfolio_id);

// Dock one payoff wizard the same way a new chart pane docks.
void chartbookInsertPayoff(ChartbookLayout& layout, int payoff_id);

// Drop one window. An emptied leaf is replaced by its sibling.
void chartbookRemoveWindow(ChartbookLayout& layout, std::string_view window_id);

// Split ratios within 0.01 and the same windows count as the same layout.
[[nodiscard]] bool chartbookLayoutsEquivalent(const ChartbookLayout& left, const ChartbookLayout& right);

[[nodiscard]] bool chartbookWindowReferenced(const CChartbookDocument& document, std::string_view window_id);

[[nodiscard]] bool chartbookPaneIsOpen(const CChartbookDocument& document, int pane_id);

[[nodiscard]] bool chartbookFinancialsIsOpen(const CChartbookDocument& document, int financials_id);

[[nodiscard]] bool chartbookOptionsIsOpen(const CChartbookDocument& document, int options_id);

[[nodiscard]] bool chartbookPortfolioIsOpen(const CChartbookDocument& document, int portfolio_id);

[[nodiscard]] bool chartbookPayoffIsOpen(const CChartbookDocument& document, int payoff_id);

[[nodiscard]] CChartbookDocument makeDefaultChartbook(std::string name);

}  // namespace terminal
