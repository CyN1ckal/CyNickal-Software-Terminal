// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartbookDocument.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace terminal {
namespace {

constexpr int kLayoutStepLimit = 64;

void appendRemapped(ChartbookLayout& dest, const ChartbookLayout& src, int& root_out)
{
    const int base = static_cast<int>(dest.nodes.size());
    for (const ChartbookLayoutNode& node : src.nodes)
    {
        ChartbookLayoutNode copy = node;
        if (copy.first >= 0)
        {
            copy.first += base;
        }
        if (copy.second >= 0)
        {
            copy.second += base;
        }
        dest.nodes.push_back(std::move(copy));
    }
    root_out = src.root < 0 ? -1 : src.root + base;
}

[[nodiscard]] const ChartbookLayoutNode* nodeAt(const ChartbookLayout& layout, int index) noexcept
{
    if (index < 0 || std::cmp_greater_equal(index, layout.nodes.size()))
    {
        return nullptr;
    }
    return &layout.nodes[static_cast<std::size_t>(index)];
}

[[nodiscard]] ChartbookLayoutNode* nodeAt(ChartbookLayout& layout, int index) noexcept
{
    if (index < 0 || std::cmp_greater_equal(index, layout.nodes.size()))
    {
        return nullptr;
    }
    return &layout.nodes[static_cast<std::size_t>(index)];
}

[[nodiscard]] bool leafHoldsPane(const ChartbookLayoutNode& node)
{
    if (node.is_split)
    {
        return false;
    }
    return std::ranges::any_of(node.windows, [](const std::string& window) {
        return window.starts_with("pane:");
    });
}

[[nodiscard]] bool leafIsOnlyData(const ChartbookLayoutNode& node)
{
    return !node.is_split && node.windows.size() == 1 && node.windows.front() == "data";
}

[[nodiscard]] int findLeaf(const ChartbookLayout& layout, int start, bool panes_only)
{
    std::vector<int> pending;
    if (start >= 0)
    {
        pending.push_back(start);
    }
    for (int steps = 0; !pending.empty() && steps < kLayoutStepLimit; ++steps)
    {
        const int index = pending.back();
        pending.pop_back();
        const ChartbookLayoutNode* node = nodeAt(layout, index);
        if (node == nullptr)
        {
            continue;
        }
        if (!node->is_split)
        {
            if (!panes_only || leafHoldsPane(*node))
            {
                return index;
            }
            continue;
        }
        if (node->second >= 0)
        {
            pending.push_back(node->second);
        }
        if (node->first >= 0)
        {
            pending.push_back(node->first);
        }
    }
    return -1;
}

void appendToLeaf(ChartbookLayout& layout, int index, const std::string& window_id)
{
    ChartbookLayoutNode* node = nodeAt(layout, index);
    if (node == nullptr || node->is_split)
    {
        return;
    }
    node->windows.push_back(window_id);
    node->selected = window_id;
}

void visitWindows(const ChartbookLayout& layout, int start, const auto& visitor)
{
    std::vector<int> pending;
    if (start >= 0)
    {
        pending.push_back(start);
    }
    for (int steps = 0; !pending.empty() && steps < kLayoutStepLimit; ++steps)
    {
        const int index = pending.back();
        pending.pop_back();
        const ChartbookLayoutNode* node = nodeAt(layout, index);
        if (node == nullptr)
        {
            continue;
        }
        if (node->is_split)
        {
            if (node->second >= 0)
            {
                pending.push_back(node->second);
            }
            if (node->first >= 0)
            {
                pending.push_back(node->first);
            }
            continue;
        }
        for (const std::string& window : node->windows)
        {
            visitor(window);
        }
    }
}

}  // namespace

bool isChartbookDataColumn(std::string_view id) noexcept
{
    return std::ranges::any_of(kChartbookDataColumns, [id](const char* column) { return id == column; });
}

bool chartbookColumnDisplayOrders(const std::vector<ChartbookColumn>& columns, int column_count,
                                  std::vector<int>& orders)
{
    orders.clear();
    if (column_count <= 0 || column_count > kChartbookDataColumnCount)
    {
        return false;
    }
    orders.assign(static_cast<std::size_t>(column_count), -1);
    bool used[kChartbookDataColumnCount] = {};
    for (int column_n = 0; column_n < column_count; ++column_n)
    {
        const char* const id = kChartbookDataColumns[column_n];
        const auto found = std::ranges::find_if(columns, [id](const ChartbookColumn& column) {
            return column.id == id;
        });
        if (found == columns.end() || found->order < 0 || found->order >= column_count ||
            used[found->order])
        {
            orders.clear();
            return false;
        }
        used[found->order] = true;
        orders[static_cast<std::size_t>(column_n)] = found->order;
    }
    return true;
}

ChartbookLayout chartbookLeaf(std::vector<std::string> windows, std::string selected)
{
    ChartbookLayout layout;
    ChartbookLayoutNode node;
    node.windows = std::move(windows);
    node.selected = std::move(selected);
    layout.nodes.push_back(std::move(node));
    layout.root = 0;
    return layout;
}

ChartbookLayout chartbookSplit(ChartbookSplitAxis axis, float ratio, const ChartbookLayout& first,
                               const ChartbookLayout& second)
{
    ChartbookLayout layout;
    layout.nodes.emplace_back();
    int first_root = -1;
    int second_root = -1;
    appendRemapped(layout, first, first_root);
    appendRemapped(layout, second, second_root);
    ChartbookLayoutNode& root = layout.nodes.front();
    root.is_split = true;
    root.axis = axis;
    root.ratio = ratio;
    root.first = first_root;
    root.second = second_root;
    layout.root = 0;
    return layout;
}

namespace {

void chartbookInsertWindow(ChartbookLayout& layout, const std::string& window_id)
{
    if (layout.root < 0 || layout.nodes.empty())
    {
        layout = chartbookLeaf({window_id}, window_id);
        return;
    }

    const int pane_leaf = findLeaf(layout, layout.root, true);
    if (pane_leaf >= 0)
    {
        appendToLeaf(layout, pane_leaf, window_id);
        return;
    }

    const ChartbookLayoutNode* root = nodeAt(layout, layout.root);
    if (root == nullptr)
    {
        layout = chartbookLeaf({window_id}, window_id);
        return;
    }

    if (root->is_split)
    {
        const ChartbookLayoutNode* first = nodeAt(layout, root->first);
        const ChartbookLayoutNode* second = nodeAt(layout, root->second);
        int side = -1;
        if (first != nullptr && leafIsOnlyData(*first))
        {
            side = root->second;
        }
        else if (second != nullptr && leafIsOnlyData(*second))
        {
            side = root->first;
        }
        const int leaf = findLeaf(layout, side, false);
        if (leaf >= 0)
        {
            appendToLeaf(layout, leaf, window_id);
            return;
        }
    }

    if (!root->is_split && leafIsOnlyData(*root))
    {
        const ChartbookLayout data = chartbookLeaf({"data"}, "data");
        const ChartbookLayout pane = chartbookLeaf({window_id}, window_id);
        layout = chartbookSplit(ChartbookSplitAxis::Horizontal, kChartbookDefaultDataRatio, data, pane);
        return;
    }

    if (!root->is_split)
    {
        appendToLeaf(layout, layout.root, window_id);
        return;
    }

    layout = chartbookSplit(ChartbookSplitAxis::Horizontal, 0.5f, layout,
                            chartbookLeaf({window_id}, window_id));
}

}  // namespace

void chartbookInsertPane(ChartbookLayout& layout, int pane_id)
{
    chartbookInsertWindow(layout, paneWindowId(pane_id));
}

void chartbookInsertFinancials(ChartbookLayout& layout, int financials_id)
{
    chartbookInsertWindow(layout, financialsWindowId(financials_id));
}

void chartbookInsertData(ChartbookLayout& layout)
{
    if (layout.root < 0 || layout.nodes.empty())
    {
        layout = chartbookLeaf({"data"}, "data");
        return;
    }
    bool present = false;
    visitWindows(layout, layout.root, [&present](const std::string& window) {
        present = present || window == "data";
    });
    if (present)
    {
        return;
    }
    const ChartbookLayout previous = layout;
    layout = chartbookSplit(ChartbookSplitAxis::Horizontal, kChartbookDefaultDataRatio,
                            chartbookLeaf({"data"}, "data"), previous);
}

bool chartbookWindowReferenced(const CChartbookDocument& document, std::string_view window_id)
{
    bool found = false;
    if (document.layout.root >= 0)
    {
        visitWindows(document.layout, document.layout.root, [&](const std::string& window) {
            found = found || window == window_id;
        });
    }
    if (found)
    {
        return true;
    }
    return std::ranges::any_of(document.floating, [&](const ChartbookFloating& floating) {
        return floating.window == window_id;
    });
}

bool chartbookPaneIsOpen(const CChartbookDocument& document, int pane_id)
{
    return chartbookWindowReferenced(document, paneWindowId(pane_id));
}

bool chartbookFinancialsIsOpen(const CChartbookDocument& document, int financials_id)
{
    return chartbookWindowReferenced(document, financialsWindowId(financials_id));
}

namespace {

[[nodiscard]] int findParentIndex(const ChartbookLayout& layout, int child)
{
    for (int index = 0; std::cmp_less(index, layout.nodes.size()); ++index)
    {
        const ChartbookLayoutNode& node = layout.nodes[static_cast<std::size_t>(index)];
        if (node.is_split && (node.first == child || node.second == child))
        {
            return index;
        }
    }
    return -1;
}

void eraseWindow(ChartbookLayoutNode& node, std::string_view window_id)
{
    std::erase(node.windows, std::string(window_id));
    if (node.selected == window_id)
    {
        node.selected = node.windows.empty() ? std::string{} : node.windows.front();
    }
}

}  // namespace

void chartbookRemoveWindow(ChartbookLayout& layout, std::string_view window_id)
{
    int leaf = -1;
    std::vector<int> pending;
    if (layout.root >= 0)
    {
        pending.push_back(layout.root);
    }
    for (int steps = 0; leaf < 0 && !pending.empty() && steps < 64; ++steps)
    {
        const int index = pending.back();
        pending.pop_back();
        ChartbookLayoutNode* node =
            index >= 0 && std::cmp_less(index, layout.nodes.size())
                ? &layout.nodes[static_cast<std::size_t>(index)]
                : nullptr;
        if (node == nullptr)
        {
            continue;
        }
        if (node->is_split)
        {
            if (node->second >= 0)
            {
                pending.push_back(node->second);
            }
            if (node->first >= 0)
            {
                pending.push_back(node->first);
            }
            continue;
        }
        if (std::ranges::find(node->windows, window_id) != node->windows.end())
        {
            leaf = index;
        }
    }
    ChartbookLayoutNode* node = leaf >= 0 && std::cmp_less(leaf, layout.nodes.size())
                                     ? &layout.nodes[static_cast<std::size_t>(leaf)]
                                     : nullptr;
    if (node == nullptr)
    {
        return;
    }
    eraseWindow(*node, window_id);
    if (!node->windows.empty())
    {
        return;
    }
    const int parent = findParentIndex(layout, leaf);
    if (parent < 0)
    {
        layout.nodes.clear();
        layout.root = -1;
        return;
    }
    const ChartbookLayoutNode& split = layout.nodes[static_cast<std::size_t>(parent)];
    const int sibling = split.first == leaf ? split.second : split.first;
    const ChartbookLayoutNode* sibling_node =
        sibling >= 0 && std::cmp_less(sibling, layout.nodes.size())
            ? &layout.nodes[static_cast<std::size_t>(sibling)]
            : nullptr;
    if (sibling_node == nullptr)
    {
        layout.nodes.clear();
        layout.root = -1;
        return;
    }
    const ChartbookLayoutNode promoted = *sibling_node;
    layout.nodes[static_cast<std::size_t>(parent)] = promoted;
    if (layout.root == leaf)
    {
        layout.root = parent;
    }
}

bool chartbookLayoutsEquivalent(const ChartbookLayout& left, const ChartbookLayout& right)
{
    if (left.root < 0 && right.root < 0)
    {
        return true;
    }
    if (left.root < 0 || right.root < 0)
    {
        return false;
    }
    struct Pair
    {
        int left{0};
        int right{0};
    };
    std::vector<Pair> pending;
    pending.push_back(Pair{.left=left.root, .right=right.root});
    for (int steps = 0; !pending.empty() && steps < 64; ++steps)
    {
        const Pair pair = pending.back();
        pending.pop_back();
        const ChartbookLayoutNode* a =
            pair.left >= 0 && std::cmp_less(pair.left, left.nodes.size())
                ? &left.nodes[static_cast<std::size_t>(pair.left)]
                : nullptr;
        const ChartbookLayoutNode* b =
            pair.right >= 0 && std::cmp_less(pair.right, right.nodes.size())
                ? &right.nodes[static_cast<std::size_t>(pair.right)]
                : nullptr;
        if (a == nullptr || b == nullptr || a->is_split != b->is_split)
        {
            return false;
        }
        if (a->is_split)
        {
            if (a->axis != b->axis || std::fabs(a->ratio - b->ratio) > 0.01f)
            {
                return false;
            }
            pending.push_back(Pair{.left=a->second, .right=b->second});
            pending.push_back(Pair{.left=a->first, .right=b->first});
            continue;
        }
        if (a->windows != b->windows || a->selected != b->selected)
        {
            return false;
        }
    }
    return pending.empty();
}

CChartbookDocument makeDefaultChartbook(std::string name)
{
    CChartbookDocument document;
    document.name = std::move(name);
    document.focused_pane = 1;
    document.next_pane_id = 2;
    document.next_financials_id = 1;
    document.data.ingest_timeframe = "1m";
    document.layout = chartbookSplit(ChartbookSplitAxis::Horizontal, kChartbookDefaultDataRatio,
                                     chartbookLeaf({"data"}, "data"),
                                     chartbookLeaf({paneWindowId(1)}, paneWindowId(1)));
    ChartbookPane pane;
    pane.id = 1;
    document.panes.push_back(std::move(pane));
    return document;
}

}  // namespace terminal
