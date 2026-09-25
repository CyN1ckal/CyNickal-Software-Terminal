// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartbookDocument.h"
#include "chart/CSymbolLink.h"
#include "ui/FinancialsPanel.h"
#include "ui/OptionsChainPanel.h"
#include "ui/PortfolioPanel.h"
#include "ui/PayoffPanel.h"

#include "imgui.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

class CChartPane;
class IngestWorker;
class Store;

class CChartBook
{
public:
    explicit CChartBook(int runtime_id);
    ~CChartBook();

    CChartBook(const CChartBook&) = delete;
    CChartBook& operator=(const CChartBook&) = delete;
    CChartBook(CChartBook&&) = delete;
    CChartBook& operator=(CChartBook&&) = delete;

    void drawMenu();
    void draw(Store* store, std::string_view store_error, IngestWorker* ingest);
    void drawFinancials(Store* store, std::string_view store_error, IngestWorker* ingest);
    void drawOptions(Store* store, std::string_view store_error, IngestWorker* ingest);
    void drawPortfolios(Store* store, std::string_view store_error, IngestWorker* ingest);
    void drawPayoffs(Store* store, std::string_view store_error, IngestWorker* ingest);
    void placeFinancials(int financials_id, bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);
    void placeOptions(int options_id, bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);
    void placePortfolio(int portfolio_id, bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);
    void placePayoff(int payoff_id, bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);
    [[nodiscard]] const CChartPane* focusedPane() const;
    [[nodiscard]] const FinancialsPanel* focusedFinancials() const;
    [[nodiscard]] const OptionsChainPanel* focusedOptions() const;
    [[nodiscard]] const PortfolioPanel* focusedPortfolio() const;
    [[nodiscard]] const PayoffPanel* focusedPayoff() const;
    void addPane();
    void addFinancials();
    void addOptions();
    void addPortfolio();
    void addPayoff();
    void closeFocused();
    void closeFocusedFinancials();
    void closeFocusedOptions();
    void closeFocusedPortfolio();
    void closeFocusedPayoff();
    void openFocusedSettings();
    void openFocusedStudies();

    void loadDocument(const CChartbookDocument& document);
    [[nodiscard]] CChartbookDocument exportDocument() const;
    [[nodiscard]] int runtimeId() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    void setName(std::string name);
    [[nodiscard]] const ChartbookData& data() const noexcept;
    void setData(ChartbookData data);
    [[nodiscard]] const ChartbookLayout& layout() const noexcept;
    void setLayout(ChartbookLayout layout);
    [[nodiscard]] const std::vector<ChartbookFloating>& floating() const noexcept;
    void setFloating(std::vector<ChartbookFloating> floating);
    [[nodiscard]] bool consumeLayoutRequest() noexcept;
    void setWindowScope(int runtime_id);
    void placePane(int pane_id, bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);
    [[nodiscard]] bool containsPane(int pane_id) const;
    [[nodiscard]] bool containsFinancials(int financials_id) const;
    [[nodiscard]] bool containsOptions(int options_id) const;
    [[nodiscard]] bool containsPortfolio(int portfolio_id) const;
    [[nodiscard]] bool containsPayoff(int payoff_id) const;

private:
    void eraseClosed();
    void eraseClosedFinancials();
    void eraseClosedOptions();
    void eraseClosedPortfolios();
    void eraseClosedPayoffs();
    [[nodiscard]] CChartPane* focused();
    [[nodiscard]] const CChartPane* findPane(int pane_id) const;
    [[nodiscard]] FinancialsPanel* focusedFinancialsPanel();
    [[nodiscard]] const FinancialsPanel* findFinancials(int financials_id) const;
    [[nodiscard]] OptionsChainPanel* focusedOptionsPanel();
    [[nodiscard]] const OptionsChainPanel* findOptions(int options_id) const;
    [[nodiscard]] PortfolioPanel* focusedPortfolioPanel();
    [[nodiscard]] PayoffPanel* focusedPayoffPanel();
    [[nodiscard]] const PortfolioPanel* findPortfolio(int portfolio_id) const;
    [[nodiscard]] const PayoffPanel* findPayoff(int payoff_id) const;

    int runtime_id_{0};
    std::string name_;
    ChartbookData data_{};
    ChartbookLayout layout_{};
    std::vector<ChartbookFloating> floating_;
    // Destroyed after the pane vectors so a binding detaches while this table is still alive.
    CSymbolLink symbol_link_;
    std::vector<std::unique_ptr<CChartPane>> panes_;
    std::vector<std::unique_ptr<FinancialsPanel>> financials_;
    std::vector<std::unique_ptr<OptionsChainPanel>> options_;
    std::vector<std::unique_ptr<PortfolioPanel>> portfolios_;
    std::vector<std::unique_ptr<PayoffPanel>> payoffs_;
    int next_id_{1};
    int focused_id_{0};
    int next_financials_id_{1};
    int focused_financials_id_{0};
    int next_options_id_{1};
    int focused_options_id_{0};
    int next_portfolio_id_{1};
    int focused_portfolio_id_{0};
    int next_payoff_id_{1};
    int focused_payoff_id_{0};
    bool layout_request_{false};
};

}  // namespace terminal
