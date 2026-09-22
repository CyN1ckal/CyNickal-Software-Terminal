// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartbookDocument.h"

#include "imgui.h"
#include "market_data/Store.h"
#include "market_data/Types.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace terminal {

class IngestWorker;

class InventoryPanel
{
public:
    InventoryPanel();
    ~InventoryPanel();

    InventoryPanel(const InventoryPanel&) = delete;
    InventoryPanel& operator=(const InventoryPanel&) = delete;
    InventoryPanel(InventoryPanel&&) = delete;
    InventoryPanel& operator=(InventoryPanel&&) = delete;

    // True while the window stays open. False when the user closes it.
    bool draw();
    void setWindowScope(int runtime_id) noexcept;
    void setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);
    [[nodiscard]] ChartbookData exportData() const;
    void importData(const ChartbookData& data);
    [[nodiscard]] IngestWorker* ingestWorker() noexcept;
    [[nodiscard]] const IngestWorker* ingestWorker() const noexcept;
    [[nodiscard]] std::string_view statusText() const noexcept;
    [[nodiscard]] std::string_view openError() const noexcept;

private:
    void fillDefaultDates();
    void pollWorker();
    void refreshSummaries();
    void refreshDays();
    void submitIngest();
    void drawToolbar();
    void drawSummaryTable();
    void drawDayTable();
    void applySortSpecs();
    void resolveSelection();
    void applySavedColumns();
    void snapshotColumns();
    void noteColumnEdits();

    std::filesystem::path db_path_;
    std::unique_ptr<Store> store_;
    std::unique_ptr<IngestWorker> worker_;
    std::string open_error_;
    std::string status_;
    std::vector<CoverageSummary> summaries_;
    std::vector<CoverageDay> days_;
    std::optional<InstrumentId> selected_id_;
    int selected_timeframe_s_{kTimeframe1m};
    int ingest_timeframe_s_{kTimeframe1m};
    char symbol_[32]{};
    char from_[16]{};
    char to_[16]{};
    std::string selected_symbol_;
    std::string selected_timeframe_;
    std::string pending_symbol_;
    std::string pending_timeframe_;
    std::vector<ChartbookColumn> columns_;
    std::string sort_column_;
    bool sort_descending_{false};
    bool apply_columns_{false};
    bool ignore_settings_dirty_{false};
    int runtime_id_{0};
    bool place_force_{false};
    bool place_floating_{false};
    ImGuiID place_dock_{0};
    ImVec2 place_pos_;
    ImVec2 place_size_;
    std::chrono::steady_clock::time_point last_refresh_;
};

}  // namespace terminal
