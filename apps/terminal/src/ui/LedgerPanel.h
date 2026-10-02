// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartbookDocument.h"
#include "data/LedgerMarks.h"
#include "trading/Ledger.h"

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

// One trade ledger. A manual ledger takes fills and cash flows here. A backtest
// ledger is shown read-only. Positions and round trips are derived on every load.
class LedgerPanel
{
public:
    explicit LedgerPanel(int id);

    [[nodiscard]] int id() const noexcept;
    [[nodiscard]] bool windowOpen() const noexcept;
    void closeWindow();
    void requestFocus();
    void importState(const ChartbookLedger& state);
    [[nodiscard]] ChartbookLedger exportState() const;
    void setWindowScope(int runtime_id) noexcept;
    void setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);

    // True when this window is focused.
    bool draw(Store* store, std::string_view store_error, IngestWorker* ingest);
    // Fetches prices for the open positions, including names that already have bars.
    void requestData(Store* store, IngestWorker* ingest);
    // Switches the window to one ledger, as a backtest window's "Open ledger" does.
    void showLedger(LedgerId id);

private:
    enum class Tab : std::uint8_t
    {
        Positions = 0,
        Closed,
        Fills,
        Cash,
    };

    void reload(const Store& store);
    void recompute(const Store& store);
    void refreshMarks(const Store& store);
    void selectLedger(LedgerId id);
    void clearLedger(std::string status);
    void drawHeader(Store& store);
    void drawSummary();
    void drawPositions();
    void drawClosed();
    void drawFills(Store& store);
    void drawCash(Store& store);
    void drawAddFill(Store& store, IngestWorker* ingest);
    void drawAddCash(Store& store);
    void pollPending(Store& store, IngestWorker* ingest);
    // Resolves symbol and writes the fill. False when the symbol has no open listing.
    [[nodiscard]] bool appendFill(Store& store, const std::string& symbol, TradeFill fill);
    void createLedger(Store& store);
    void renameLedger(Store& store);
    void deleteLedger(Store& store);
    void deleteFill(Store& store, TradeFillId fill_id);
    void deleteCashFlow(Store& store, LedgerCashFlowId flow_id);
    [[nodiscard]] bool editable() const noexcept;

    std::vector<Ledger> ledgers_;
    LedgerId ledger_id_{0};
    std::string ledger_name_;
    LedgerKind ledger_kind_{LedgerKind::Manual};
    UnixSeconds loaded_updated_{0};
    bool loaded_{false};

    std::vector<TradeFill> fills_;
    std::vector<LedgerCashFlow> flows_;
    LedgerBook book_;
    double deposits_{0.0};
    std::vector<std::optional<LedgerMark>> marks_;
    bool marks_valid_{false};
    std::optional<UnixSeconds> received_at_;
    std::uint64_t priced_serial_{0};
    std::uint64_t refresh_serial_{0};

    Tab tab_{Tab::Positions};
    // Set by importState so the saved tab is selected on the first frame.
    bool select_tab_{false};
    std::string status_{"select a ledger"};
    std::string error_;

    char new_name_[64]{};
    char rename_[64]{};
    char symbol_[32]{};
    int kind_{0};
    int side_{0};
    char quantity_[32]{};
    char price_[32]{};
    char fees_[32]{};
    char when_[32]{};
    char note_[96]{};
    char expiration_[16]{};
    int expiration_type_{0};
    char strike_[32]{};
    int right_{0};
    char cash_amount_[32]{};
    char cash_when_[32]{};
    char cash_note_[96]{};

    bool pending_{false};
    std::string pending_symbol_;
    TradeFill pending_fill_{};
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
