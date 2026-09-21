// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartBook.h"

#include "RepoRoot.h"
#include "chart/CChartPane.h"

#include "imgui.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace terminal {

CChartBook::CChartBook()
{
    try
    {
        store_ = std::make_unique<Store>(defaultMarketDataDbPath(), StoreMode::Reader);
    }
    catch (const std::exception& ex)
    {
        open_error_ = ex.what();
    }
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
        if (pane->id() == focused_id_)
        {
            return pane.get();
        }
    }
    return nullptr;
}

void CChartBook::eraseClosed()
{
    panes_.erase(std::remove_if(panes_.begin(), panes_.end(),
                                [](const std::unique_ptr<CChartPane>& pane) {
                                    return !pane->windowOpen();
                                }),
                 panes_.end());
    if (focused() == nullptr)
    {
        focused_id_ = 0;
    }
}

void CChartBook::addPane()
{
    // imgui.ini may still list ###chart_N from a previous run; ids are not reused in-process.
    auto pane = std::make_unique<CChartPane>(next_id_);
    focused_id_ = next_id_;
    pane->requestFocus();
    ++next_id_;
    panes_.push_back(std::move(pane));
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

void CChartBook::drawMenu()
{
    if (ImGui::BeginMenu("Chart"))
    {
        if (ImGui::MenuItem("New Chart"))
        {
            addPane();
        }
        CChartPane* pane = focused();
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
        ImGui::EndMenu();
    }
}

void CChartBook::draw(ImGuiID chart_dock_id)
{
    for (std::unique_ptr<CChartPane>& pane : panes_)
    {
        if (pane->draw(store_.get(), open_error_, chart_dock_id))
        {
            focused_id_ = pane->id();
        }
    }
    eraseClosed();
}

}  // namespace terminal
