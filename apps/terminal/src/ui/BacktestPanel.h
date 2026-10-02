// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "backtest/BacktestJob.h"
#include "backtest/BacktestWorker.h"
#include "chart/CChartbookDocument.h"

#include "market_data/Types.h"

#include "imgui.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

class IngestWorker;
class Store;

// Asks the chartbook to open a ledger or statistics window on a recorded run.
struct BacktestOpenRequest
{
    bool statistics{false};
    LedgerId ledger_id{0};
};

// Runs a registered strategy over stored bars on a BacktestWorker and lists the
// recorded runs. A symbol or range without bars is downloaded first.
class BacktestPanel
{
public:
    explicit BacktestPanel(int id);
    ~BacktestPanel();

    BacktestPanel(const BacktestPanel&) = delete;
    BacktestPanel& operator=(const BacktestPanel&) = delete;
    BacktestPanel(BacktestPanel&&) = delete;
    BacktestPanel& operator=(BacktestPanel&&) = delete;

    [[nodiscard]] int id() const noexcept;
    [[nodiscard]] bool windowOpen() const noexcept;
    void closeWindow();
    void requestFocus();
    void importState(const ChartbookBacktest& state);
    [[nodiscard]] ChartbookBacktest exportState() const;
    void setWindowScope(int runtime_id) noexcept;
    void setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);

    // True when this window is focused.
    bool draw(Store* store, std::string_view store_error, IngestWorker* ingest);
    // Refreshes the list of recorded runs.
    void requestData(Store* store, IngestWorker* ingest);
    // A window the user asked for this frame, once.
    [[nodiscard]] std::optional<BacktestOpenRequest> takeOpenRequest();
    // Hands over the worker so a closed panel can be destroyed without waiting for its
    // run; the run still finishes and records. nullptr when no run was ever started.
    [[nodiscard]] std::unique_ptr<BacktestWorker> releaseWorker() noexcept;

private:
    [[nodiscard]] const StrategyType* strategy() const noexcept;
    void selectStrategy(const StrategyType& type);
    [[nodiscard]] std::optional<BacktestRequest> buildRequest();
    void startRun(const Store& store, IngestWorker* ingest);
    void enqueueRun(const Store& store, BacktestRequest request);
    void poll(const Store& store, IngestWorker* ingest);
    void reloadRuns(const Store& store);
    void loadInputs(const BacktestRun& run);
    void deleteRun(const Store& store, LedgerId ledger_id);
    void drawInputs(const Store& store, IngestWorker* ingest);
    void drawRuns(const Store& store);

    std::string strategy_id_;
    std::vector<int> options_;
    char symbol_[32]{};
    int period_{static_cast<int>(ChartBarPeriod::Day1)};
    char from_[16]{};
    char to_[16]{};
    BacktestConfig config_{};
    bool use_stop_{false};
    double stop_pct_{5.0};
    bool use_target_{false};
    double target_pct_{10.0};

    std::unique_ptr<BacktestWorker> worker_;
    std::uint64_t run_serial_{0};
    std::uint64_t fetch_serial_{0};
    std::optional<BacktestRequest> after_fetch_;
    std::optional<BacktestOutcome> outcome_;

    std::vector<BacktestRun> runs_;
    std::map<LedgerId, std::string> ledger_names_;
    bool runs_stale_{true};
    double runs_loaded_at_{-1.0};
    LedgerId delete_ledger_{0};
    std::optional<BacktestOpenRequest> open_request_;

    std::string status_;
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
