// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartSettings.h"
#include "chart/CChartView.h"
#include "chart/CStudy.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

inline constexpr int kChartbookFormatVersion = 1;
inline constexpr float kChartbookDefaultDataRatio = 0.30f;
inline constexpr const char* kChartbookFileSuffix = ".chartbook.json";

// Summary table column ids, declaration order. Matches InventoryPanel.
inline constexpr const char* kChartbookDataColumns[] = {
    "symbol", "exch", "tf", "bars", "sess", "ok", "part", "miss", "err", "first", "last",
};
inline constexpr int kChartbookDataColumnCount = 11;

[[nodiscard]] bool isChartbookDataColumn(std::string_view id) noexcept;

// ImGui requires DisplayOrder to be a permutation. Empty orders means the saved list is partial or repeated.
[[nodiscard]] bool chartbookColumnDisplayOrders(const std::vector<ChartbookColumn>& columns, int column_count,
                                                std::vector<int>& orders);

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
// A leaf uses windows ("data" or "pane:<id>") and selected.
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

struct ChartbookPane
{
    int id{0};
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
    ChartbookData data{};
    ChartbookLayout layout{};
    std::vector<ChartbookFloating> floating;
    std::vector<ChartbookPane> panes;
};

[[nodiscard]] inline std::string paneWindowId(int pane_id)
{
    return "pane:" + std::to_string(pane_id);
}

[[nodiscard]] ChartbookLayout chartbookLeaf(std::vector<std::string> windows, std::string selected);

[[nodiscard]] ChartbookLayout chartbookSplit(ChartbookSplitAxis axis, float ratio,
                                             const ChartbookLayout& first, const ChartbookLayout& second);

// Dock a new chart into the space: an existing pane leaf, else the side opposite DATA, else the root.
void chartbookInsertPane(ChartbookLayout& layout, int pane_id);

// Dock DATA on the left at the default ratio when this space does not already show it.
void chartbookInsertData(ChartbookLayout& layout);

// Drop one window. An emptied leaf is replaced by its sibling.
void chartbookRemoveWindow(ChartbookLayout& layout, std::string_view window_id);

// Split ratios within 0.01 and the same windows count as the same layout.
[[nodiscard]] bool chartbookLayoutsEquivalent(const ChartbookLayout& left, const ChartbookLayout& right);

[[nodiscard]] bool chartbookWindowReferenced(const CChartbookDocument& document, std::string_view window_id);

[[nodiscard]] bool chartbookPaneIsOpen(const CChartbookDocument& document, int pane_id);

[[nodiscard]] CChartbookDocument makeDefaultChartbook(std::string name);

}  // namespace terminal
