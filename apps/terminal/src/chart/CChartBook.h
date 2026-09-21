// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "imgui.h"
#include "market_data/Store.h"

#include <memory>
#include <string>
#include <vector>

namespace terminal {

class CChartPane;

class CChartBook
{
public:
    CChartBook();
    ~CChartBook();

    CChartBook(const CChartBook&) = delete;
    CChartBook& operator=(const CChartBook&) = delete;
    CChartBook(CChartBook&&) = delete;
    CChartBook& operator=(CChartBook&&) = delete;

    void drawMenu();
    void draw(ImGuiID chart_dock_id);
    void addPane();
    void closeFocused();
    void openFocusedSettings();
    void openFocusedStudies();

private:
    void eraseClosed();
    [[nodiscard]] CChartPane* focused();

    std::unique_ptr<Store> store_;
    std::string open_error_;
    std::vector<std::unique_ptr<CChartPane>> panes_;
    int next_id_{1};
    int focused_id_{0};
};

}  // namespace terminal
