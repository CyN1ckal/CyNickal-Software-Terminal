// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartbookDocument.h"
#include "data/LedgerAnalysis.h"

#include "market_data/Types.h"

#include "imgui.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

class IngestWorker;
class Store;

// Performance of one ledger, manual or backtest: return and risk ratios, trade
// statistics, the time-weighted return against a buy-and-hold benchmark, drawdown,
// and each closed trade's profit.
class StatsPanel
{
public:
    explicit StatsPanel(int id);

    [[nodiscard]] int id() const noexcept;
    [[nodiscard]] bool windowOpen() const noexcept;
    void closeWindow();
    void requestFocus();
    void importState(const ChartbookStats& state);
    [[nodiscard]] ChartbookStats exportState() const;
    void setWindowScope(int runtime_id) noexcept;
    void setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);

    // True when this window is focused.
    bool draw(Store* store, std::string_view store_error, IngestWorker* ingest);
    // Fetches daily history for every traded share and the benchmark.
    void requestData(Store* store, IngestWorker* ingest);
    // Switches the window to one ledger, as a backtest window's "Open in Statistics" does.
    void showLedger(LedgerId id);

private:
    void reload(const Store& store, IngestWorker* ingest);
    void analyze(const Store& store);
    void fetchHistory(const Store& store, IngestWorker& ingest, bool missing_only);
    void drawHeader();
    void drawTable() const;
    void drawCharts() const;

    std::vector<Ledger> ledgers_;
    LedgerId ledger_id_{0};
    std::string ledger_name_;
    LedgerKind ledger_kind_{LedgerKind::Manual};
    UnixSeconds loaded_updated_{0};
    bool loaded_{false};
    bool stale_{true};
    bool fetched_missing_{false};

    char benchmark_[16]{};
    std::string benchmark_applied_;
    bool benchmark_found_{false};

    LedgerAnalysis analysis_;
    std::vector<double> times_;
    std::vector<double> return_pct_;
    std::vector<double> drawdown_pct_;
    std::vector<double> benchmark_times_;
    std::vector<double> benchmark_pct_;
    std::vector<double> win_x_;
    std::vector<double> win_y_;
    std::vector<double> loss_x_;
    std::vector<double> loss_y_;

    std::uint64_t priced_serial_{0};
    std::uint64_t refresh_serial_{0};
    std::string status_{"select a ledger"};
    std::string error_;

    int id_{0};
    bool window_open_{true};
    bool focus_on_appear_{false};
    int runtime_id_{0};
    bool place_force_{false};
    bool place_floating_{false};
    ImGuiID place_dock_{0};
    ImVec2 place_pos_;
    ImVec2 place_size_;
};

}  // namespace terminal
