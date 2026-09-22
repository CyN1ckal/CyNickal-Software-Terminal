// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartbookDocument.h"
#include "data/StatementSheet.h"

#include "imgui.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace terminal {

class IngestWorker;
class Store;

// Spreadsheet of one statement. Columns are the last four yearly or quarterly periods.
class FinancialsPanel
{
public:
    FinancialsPanel() = default;
    ~FinancialsPanel() = default;

    FinancialsPanel(const FinancialsPanel&) = delete;
    FinancialsPanel& operator=(const FinancialsPanel&) = delete;
    FinancialsPanel(FinancialsPanel&&) = delete;
    FinancialsPanel& operator=(FinancialsPanel&&) = delete;

    void importState(const ChartbookFinancials& state);
    [[nodiscard]] ChartbookFinancials exportState() const;
    void setWindowScope(int runtime_id) noexcept;
    void setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);

    // True while the window stays open.
    bool draw(Store* store, std::string_view store_error, IngestWorker* ingest);

private:
    void drawToolbar(IngestWorker* ingest);
    void refresh(Store* store, IngestWorker* ingest);
    void requestFetch(IngestWorker* ingest, bool force);
    void drawSheet() const;
    [[nodiscard]] std::string viewKey() const;

    char symbol_[32]{};
    std::string active_symbol_;
    StatementKind statement_{StatementKind::Income};
    StatementTimeframe timeframe_{StatementTimeframe::Annually};
    StatementSheet sheet_{};
    std::string loaded_key_;
    std::string failed_key_;
    std::string inflight_key_;
    std::string status_{"enter a symbol"};
    std::string error_;
    std::uint64_t inflight_serial_{0};
    bool inflight_{false};
    bool have_snapshot_{false};
    bool busy_{false};
    bool blocked_{false};
    bool fetch_now_{false};
    bool needs_reload_{true};
    int runtime_id_{0};
    bool place_force_{false};
    bool place_floating_{false};
    ImGuiID place_dock_{0};
    ImVec2 place_pos_;
    ImVec2 place_size_;
};

}  // namespace terminal
