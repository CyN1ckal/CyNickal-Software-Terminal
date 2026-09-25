// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartbookDocument.h"

#include "market_data/Types.h"

#include "imgui.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

class IngestWorker;
class Store;

// One hypothetical book. Holdings are edited here and written with replaceHoldings.
class PortfolioPanel
{
public:
    explicit PortfolioPanel(int id);
    ~PortfolioPanel() = default;

    PortfolioPanel(const PortfolioPanel&) = delete;
    PortfolioPanel& operator=(const PortfolioPanel&) = delete;
    PortfolioPanel(PortfolioPanel&&) = delete;
    PortfolioPanel& operator=(PortfolioPanel&&) = delete;

    [[nodiscard]] int id() const noexcept;
    [[nodiscard]] bool windowOpen() const noexcept;
    void closeWindow();
    void requestFocus();
    void importState(const ChartbookPortfolio& state);
    [[nodiscard]] ChartbookPortfolio exportState() const;
    void setWindowScope(int runtime_id) noexcept;
    void setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);

    // True when this window is focused.
    bool draw(Store* store, std::string_view store_error, IngestWorker* ingest);

private:
    void reload(Store& store);
    void drawBooks(Store& store);
    void drawHoldings(const Store& store);
    void drawAllocation() const;
    void refreshMarks(const Store& store);
    void drawAdd(Store& store, IngestWorker* ingest);
    void pollPending(Store& store, IngestWorker* ingest);
    void apply(Store& store, IngestWorker* ingest);
    void createBook(Store& store);
    void renameBook(Store& store);
    void deleteBook(Store& store);
    [[nodiscard]] bool appendResolved(const Store& store, PortfolioAssetKind kind, const std::string& symbol,
                                      double quantity);

    std::vector<Portfolio> books_;
    std::vector<PortfolioHolding> drafts_;
    std::vector<std::optional<double>> lasts_;
    bool marks_valid_{false};
    std::uint64_t priced_serial_{0};
    std::string book_name_;
    std::string status_{"select a portfolio"};
    std::string error_;
    PortfolioId portfolio_id_{0};
    UnixSeconds loaded_updated_{0};
    bool dirty_{false};
    bool loaded_{false};
    char new_name_[64]{};
    char rename_[64]{};
    char symbol_[32]{};
    char quantity_[32]{};
    char quantity_edit_buf_[96]{};
    int quantity_edit_{-1};
    char expiration_[16]{};
    char strike_[32]{};
    int kind_{0};
    int expiration_type_{0};
    int right_{0};
    bool pending_{false};
    PortfolioAssetKind pending_kind_{PortfolioAssetKind::Equity};
    std::string pending_symbol_;
    double pending_quantity_{};
    SessionDate pending_expiration_{0};
    OptionExpirationType pending_expiration_type_{OptionExpirationType::Weekly};
    double pending_strike_{};
    OptionRight pending_right_{OptionRight::Call};
    std::uint64_t pending_serial_{0};
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
