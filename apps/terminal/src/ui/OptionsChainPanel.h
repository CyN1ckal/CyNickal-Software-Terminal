// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartbookDocument.h"
#include "chart/CSymbolLink.h"
#include "ui/OptionChainSource.h"

#include "market_data/Types.h"

#include "imgui.h"

#include <string>
#include <string_view>
#include <vector>

namespace terminal {

class IngestWorker;
class Store;

// One underlying's option chain. Calls sit left of the strike and puts sit right.
// The Columns menu chooses the call-side fields. Puts mirror that selection.
class OptionsChainPanel
{
public:
    explicit OptionsChainPanel(int id);
    ~OptionsChainPanel() = default;

    OptionsChainPanel(const OptionsChainPanel&) = delete;
    OptionsChainPanel& operator=(const OptionsChainPanel&) = delete;
    OptionsChainPanel(OptionsChainPanel&&) = delete;
    OptionsChainPanel& operator=(OptionsChainPanel&&) = delete;

    [[nodiscard]] int id() const noexcept;
    [[nodiscard]] bool windowOpen() const noexcept;
    void closeWindow();
    void requestFocus();
    void importState(const ChartbookOptions& state);
    [[nodiscard]] ChartbookOptions exportState() const;
    void setWindowScope(int runtime_id) noexcept;
    void attachSymbolLink(CSymbolLink& link);
    void setSymbolLinkGroup(int group) noexcept;
    void setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);

    // True when this window is focused.
    bool draw(Store* store, std::string_view store_error, IngestWorker* ingest);

private:
    void drawColumnMenu();
    void drawChain() const;
    static void applyLinkedThunk(void* self, std::string_view symbol);

    CSymbolLink::Binding symbol_link_;
    OptionChainSource source_;
    std::vector<std::string> columns_{defaultOptionChainColumns()};
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
