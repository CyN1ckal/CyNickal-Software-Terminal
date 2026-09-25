// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartbookDocument.h"
#include "ui/OptionChainSource.h"
#include "ui/PayoffWizard.h"

#include "imgui.h"

#include <string_view>

namespace terminal {

class IngestWorker;
class Store;

// A standalone options payoff wizard. It picks its own underlying and
// expiration. The top section is the payoff graph; the bottom section is
// where legs are entered. A divider between them can be dragged.
class PayoffPanel
{
public:
    explicit PayoffPanel(int id);
    ~PayoffPanel() = default;

    PayoffPanel(const PayoffPanel&) = delete;
    PayoffPanel& operator=(const PayoffPanel&) = delete;
    PayoffPanel(PayoffPanel&&) = delete;
    PayoffPanel& operator=(PayoffPanel&&) = delete;

    [[nodiscard]] int id() const noexcept;
    [[nodiscard]] bool windowOpen() const noexcept;
    void closeWindow();
    void requestFocus();
    void importState(const ChartbookPayoff& state);
    [[nodiscard]] ChartbookPayoff exportState() const;
    void setWindowScope(int runtime_id) noexcept;
    void setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);

    // True when this window is focused.
    bool draw(Store* store, std::string_view store_error, IngestWorker* ingest);
    void requestData();

private:
    void drawEntry(IngestWorker* ingest);

    OptionChainSource source_;
    PayoffWizard wizard_;
    float graph_share_{0.55f};
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
