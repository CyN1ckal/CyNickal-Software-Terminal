// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartBook.h"

#include "chart/CChartPane.h"
#include "ui/TitleBar.h"

#include "imgui.h"

#include <algorithm>
#include <utility>

namespace terminal {
namespace {

[[nodiscard]] bool paneIdOf(std::string_view window, int& pane_id)
{
    constexpr std::string_view prefix = "pane:";
    if (!window.starts_with(prefix) || window.size() > prefix.size() + 9)
    {
        return false;
    }
    pane_id = 0;
    for (const char digit : window.substr(prefix.size()))
    {
        if (digit < '0' || digit > '9')
        {
            return false;
        }
        pane_id = (pane_id * 10) + (digit - '0');
    }
    return pane_id > 0;
}

void dropClosedPanes(CChartbookDocument& document)
{
    std::vector<std::string> stale;
    const auto consider = [&](const std::string& window) {
        int pane_id = 0;
        if (!paneIdOf(window, pane_id))
        {
            return;
        }
        const bool present = std::ranges::any_of(document.panes, [&](const ChartbookPane& pane) {
            return pane.id == pane_id;
        });
        if (!present)
        {
            stale.push_back(window);
        }
    };
    std::vector<int> pending;
    if (document.layout.root >= 0)
    {
        pending.push_back(document.layout.root);
    }
    for (int steps = 0; !pending.empty() && steps < 64; ++steps)
    {
        const int index = pending.back();
        pending.pop_back();
        if (index < 0 || std::cmp_greater_equal(index, document.layout.nodes.size()))
        {
            continue;
        }
        const ChartbookLayoutNode& node = document.layout.nodes[static_cast<std::size_t>(index)];
        if (node.is_split)
        {
            pending.push_back(node.second);
            pending.push_back(node.first);
            continue;
        }
        for (const std::string& window : node.windows)
        {
            consider(window);
        }
    }
    for (const ChartbookFloating& floating : document.floating)
    {
        consider(floating.window);
    }
    for (const std::string& window : stale)
    {
        chartbookRemoveWindow(document.layout, window);
    }
    std::erase_if(document.floating, [&](const ChartbookFloating& floating) {
        return std::ranges::find(stale, floating.window) != stale.end();
    });
}

}  // namespace

CChartBook::CChartBook(int runtime_id)
    : runtime_id_(runtime_id)
{
}

CChartBook::~CChartBook() = default;

CChartPane* CChartBook::focused()
{
    if (focused_id_ == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<CChartPane>& pane : panes_)
    {
        if (pane->id() == focused_id_ && pane->windowOpen())
        {
            return pane.get();
        }
    }
    return nullptr;
}

const CChartPane* CChartBook::focusedPane() const
{
    return findPane(focused_id_);
}

const CChartPane* CChartBook::findPane(int pane_id) const
{
    if (pane_id == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<CChartPane>& pane : panes_)
    {
        if (pane->id() == pane_id && pane->windowOpen())
        {
            return pane.get();
        }
    }
    return nullptr;
}

void CChartBook::eraseClosed()
{
    const auto removed = std::ranges::remove_if(panes_, [](const std::unique_ptr<CChartPane>& pane) {
        return !pane->windowOpen();
    });
    panes_.erase(removed.begin(), removed.end());
    if (focused() == nullptr)
    {
        focused_id_ = 0;
    }
}

void CChartBook::addPane()
{
    auto pane = std::make_unique<CChartPane>(next_id_);
    pane->setWindowScope(runtime_id_);
    focused_id_ = next_id_;
    pane->requestFocus();
    chartbookInsertPane(layout_, next_id_);
    ++next_id_;
    panes_.push_back(std::move(pane));
    layout_request_ = true;
}

void CChartBook::closeFocused()
{
    if (CChartPane* pane = focused())
    {
        pane->closeWindow();
    }
}

void CChartBook::openFocusedSettings()
{
    if (CChartPane* pane = focused())
    {
        pane->requestFocus();
        pane->openSettings();
    }
}

void CChartBook::openFocusedStudies()
{
    if (CChartPane* pane = focused())
    {
        pane->requestFocus();
        pane->openStudies();
    }
}

void CChartBook::loadDocument(const CChartbookDocument& document)
{
    panes_.clear();
    name_ = document.name;
    data_ = document.data;
    financials_.importState(document.financials);
    financials_.setWindowScope(runtime_id_);
    layout_ = document.layout;
    floating_ = document.floating;
    next_id_ = std::max(document.next_pane_id, 1);
    focused_id_ = document.focused_pane;
    for (const ChartbookPane& record : document.panes)
    {
        if (!chartbookPaneIsOpen(document, record.id))
        {
            continue;
        }
        auto pane = std::make_unique<CChartPane>(record.id);
        pane->setWindowScope(runtime_id_);
        pane->importRecord(record);
        if (record.id == focused_id_)
        {
            pane->requestFocus();
        }
        panes_.push_back(std::move(pane));
    }
    if (findPane(focused_id_) == nullptr)
    {
        focused_id_ = panes_.empty() ? 0 : panes_.front()->id();
    }
}

CChartbookDocument CChartBook::exportDocument() const
{
    CChartbookDocument document;
    document.name = name_;
    document.focused_pane = focused_id_;
    document.next_pane_id = next_id_;
    document.data = data_;
    document.financials = financials_.exportState();
    document.layout = layout_;
    document.floating = floating_;
    for (const std::unique_ptr<CChartPane>& pane : panes_)
    {
        if (pane->windowOpen())
        {
            document.panes.push_back(pane->exportRecord());
        }
    }
    dropClosedPanes(document);
    return document;
}

int CChartBook::runtimeId() const noexcept
{
    return runtime_id_;
}

const std::string& CChartBook::name() const noexcept
{
    return name_;
}

void CChartBook::setName(std::string name)
{
    name_ = std::move(name);
}

const ChartbookData& CChartBook::data() const noexcept
{
    return data_;
}

void CChartBook::setData(ChartbookData data)
{
    data_ = std::move(data);
}

const ChartbookLayout& CChartBook::layout() const noexcept
{
    return layout_;
}

void CChartBook::setLayout(ChartbookLayout layout)
{
    layout_ = std::move(layout);
}

const std::vector<ChartbookFloating>& CChartBook::floating() const noexcept
{
    return floating_;
}

void CChartBook::setFloating(std::vector<ChartbookFloating> floating)
{
    floating_ = std::move(floating);
}

bool CChartBook::consumeLayoutRequest() noexcept
{
    const bool requested = layout_request_;
    layout_request_ = false;
    return requested;
}

void CChartBook::setWindowScope(int runtime_id)
{
    runtime_id_ = runtime_id;
    financials_.setWindowScope(runtime_id_);
    for (const std::unique_ptr<CChartPane>& pane : panes_)
    {
        pane->setWindowScope(runtime_id_);
    }
}

void CChartBook::placePane(int pane_id, bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    for (const std::unique_ptr<CChartPane>& pane : panes_)
    {
        if (pane->id() == pane_id)
        {
            pane->setPlacement(force, floating, dock, pos, size);
        }
    }
}

bool CChartBook::containsPane(int pane_id) const
{
    return findPane(pane_id) != nullptr;
}

bool CChartBook::drawFinancials(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    financials_.setWindowScope(runtime_id_);
    return financials_.draw(store, store_error, ingest);
}

void CChartBook::placeFinancials(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    financials_.setPlacement(force, floating, dock, pos, size);
}

void CChartBook::drawMenu()
{
    if (beginTitleMenu("Chart"))
    {
        if (ImGui::MenuItem("New Chart"))
        {
            addPane();
        }
        CChartPane const* pane = focused();
        const bool has_focus = pane != nullptr;
        const bool settings_enabled = has_focus && !pane->studiesOpen();
        const bool studies_enabled = has_focus && !pane->settingsOpen();
        if (ImGui::MenuItem("Chart Settings", nullptr, false, settings_enabled))
        {
            openFocusedSettings();
        }
        if (ImGui::MenuItem("Studies", nullptr, false, studies_enabled))
        {
            openFocusedStudies();
        }
        if (ImGui::MenuItem("Close Chart", nullptr, false, has_focus))
        {
            closeFocused();
        }
        endTitleMenu();
    }
}

void CChartBook::draw(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    for (std::unique_ptr<CChartPane> const& pane : panes_)
    {
        if (pane->draw(store, store_error, ingest))
        {
            focused_id_ = pane->id();
        }
    }
    eraseClosed();
}

}  // namespace terminal
