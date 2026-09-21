// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartLoad.h"
#include "chart/CChartSettings.h"
#include "chart/CChartView.h"
#include "market_data/Store.h"

#include "imgui.h"

#include <chrono>
#include <string_view>

namespace terminal {

class CChartPane
{
public:
    explicit CChartPane(int id);

    CChartPane(const CChartPane&) = delete;
    CChartPane& operator=(const CChartPane&) = delete;
    CChartPane(CChartPane&&) = delete;
    CChartPane& operator=(CChartPane&&) = delete;
    ~CChartPane() = default;

    [[nodiscard]] int id() const noexcept;
    [[nodiscard]] bool windowOpen() const noexcept;
    [[nodiscard]] const CChartSettings& settings() const noexcept;
    [[nodiscard]] ChartLoadStatus status() const noexcept;

    void openSettings();
    void closeWindow();
    void requestFocus();

    bool draw(Store* store, std::string_view store_error, ImGuiID dock_id);

private:
    void drawSettingsPopup(Store* store, std::string_view store_error);
    void applyDraft(Store* store, std::string_view store_error);
    void cancelDraft();
    void reload(Store* store, std::string_view store_error);
    void drawStatusLine() const;
    void drawPlotBody();
    void handleChartKeys();

    int id_{};
    bool window_open_{true};
    bool settings_open_{false};
    bool focus_on_appear_{false};
    CChartSettings settings_{};
    CChartSettings draft_{};
    CChartSettings loaded_settings_{};
    ChartLoadResult loaded_{};
    char draft_symbol_[32]{};
    CChartViewState view_;
    std::chrono::steady_clock::time_point last_reload_;
};

}  // namespace terminal
