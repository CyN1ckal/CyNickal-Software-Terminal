// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartBook.h"

#include "chart/CChartPane.h"
#include "ui/TitleBar.h"

#include "imgui.h"

#include <algorithm>
#include <utility>

namespace terminal {
namespace {

[[nodiscard]] bool paneIdOf(std::string_view window, int& pane_id)
{
    constexpr std::string_view prefix = "pane:";
    if (!window.starts_with(prefix) || window.size() > prefix.size() + 9)
    {
        return false;
    }
    pane_id = 0;
    for (const char digit : window.substr(prefix.size()))
    {
        if (digit < '0' || digit > '9')
        {
            return false;
        }
        pane_id = (pane_id * 10) + (digit - '0');
    }
    return pane_id > 0;
}

void dropClosedPanes(CChartbookDocument& document)
{
    std::vector<std::string> stale;
    const auto consider = [&](const std::string& window) {
        int pane_id = 0;
        if (!paneIdOf(window, pane_id))
        {
            return;
        }
        const bool present = std::ranges::any_of(document.panes, [&](const ChartbookPane& pane) {
            return pane.id == pane_id;
        });
        if (!present)
        {
            stale.push_back(window);
        }
    };
    const auto considerFinancials = [&](const std::string& window) {
        int financials_id = 0;
        if (!financialsIdFromWindow(window, financials_id))
        {
            return;
        }
        const bool present = std::ranges::any_of(document.financials, [&](const ChartbookFinancials& panel) {
            return panel.id == financials_id;
        });
        if (!present)
        {
            stale.push_back(window);
        }
    };
    const auto considerOptions = [&](const std::string& window) {
        int options_id = 0;
        if (!optionsIdFromWindow(window, options_id))
        {
            return;
        }
        const bool present = std::ranges::any_of(document.options, [&](const ChartbookOptions& panel) {
            return panel.id == options_id;
        });
        if (!present)
        {
            stale.push_back(window);
        }
    };
    const auto considerPortfolio = [&](const std::string& window) {
        int portfolio_id = 0;
        if (!portfolioIdFromWindow(window, portfolio_id))
        {
            return;
        }
        const bool present = std::ranges::any_of(document.portfolios, [&](const ChartbookPortfolio& panel) {
            return panel.id == portfolio_id;
        });
        if (!present)
        {
            stale.push_back(window);
        }
    };
    const auto considerPayoff = [&](const std::string& window) {
        int payoff_id = 0;
        if (!payoffIdFromWindow(window, payoff_id))
        {
            return;
        }
        const bool present = std::ranges::any_of(document.payoffs, [&](const ChartbookPayoff& panel) {
            return panel.id == payoff_id;
        });
        if (!present)
        {
            stale.push_back(window);
        }
    };
    std::vector<int> pending;
    if (document.layout.root >= 0)
    {
        pending.push_back(document.layout.root);
    }
    for (int steps = 0; !pending.empty() && steps < 64; ++steps)
    {
        const int index = pending.back();
        pending.pop_back();
        if (index < 0 || std::cmp_greater_equal(index, document.layout.nodes.size()))
        {
            continue;
        }
        const ChartbookLayoutNode& node = document.layout.nodes[static_cast<std::size_t>(index)];
        if (node.is_split)
        {
            pending.push_back(node.second);
            pending.push_back(node.first);
            continue;
        }
        for (const std::string& window : node.windows)
        {
            consider(window);
            considerFinancials(window);
            considerOptions(window);
            considerPortfolio(window);
            considerPayoff(window);
        }
    }
    for (const ChartbookFloating& floating : document.floating)
    {
        consider(floating.window);
        considerFinancials(floating.window);
        considerOptions(floating.window);
        considerPortfolio(floating.window);
        considerPayoff(floating.window);
    }
    for (const std::string& window : stale)
    {
        chartbookRemoveWindow(document.layout, window);
    }
    std::erase_if(document.floating, [&](const ChartbookFloating& floating) {
        return std::ranges::find(stale, floating.window) != stale.end();
    });
}

}  // namespace

CChartBook::CChartBook(int runtime_id)
    : runtime_id_(runtime_id)
{
}

CChartBook::~CChartBook() = default;

CChartPane* CChartBook::focused()
{
    if (focused_id_ == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<CChartPane>& pane : panes_)
    {
        if (pane->id() == focused_id_ && pane->windowOpen())
        {
            return pane.get();
        }
    }
    return nullptr;
}

const CChartPane* CChartBook::focusedPane() const
{
    return findPane(focused_id_);
}

const CChartPane* CChartBook::findPane(int pane_id) const
{
    if (pane_id == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<CChartPane>& pane : panes_)
    {
        if (pane->id() == pane_id && pane->windowOpen())
        {
            return pane.get();
        }
    }
    return nullptr;
}

void CChartBook::eraseClosed()
{
    const auto removed = std::ranges::remove_if(panes_, [](const std::unique_ptr<CChartPane>& pane) {
        return !pane->windowOpen();
    });
    panes_.erase(removed.begin(), removed.end());
    if (focused() == nullptr)
    {
        focused_id_ = 0;
    }
}

void CChartBook::eraseClosedFinancials()
{
    const auto removed = std::ranges::remove_if(financials_, [](const std::unique_ptr<FinancialsPanel>& panel) {
        return !panel->windowOpen();
    });
    financials_.erase(removed.begin(), removed.end());
    if (focusedFinancialsPanel() == nullptr)
    {
        focused_financials_id_ = 0;
    }
}

FinancialsPanel* CChartBook::focusedFinancialsPanel()
{
    if (focused_financials_id_ == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<FinancialsPanel>& panel : financials_)
    {
        if (panel->id() == focused_financials_id_ && panel->windowOpen())
        {
            return panel.get();
        }
    }
    return nullptr;
}

const FinancialsPanel* CChartBook::focusedFinancials() const
{
    return findFinancials(focused_financials_id_);
}

const FinancialsPanel* CChartBook::findFinancials(int financials_id) const
{
    if (financials_id == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<FinancialsPanel>& panel : financials_)
    {
        if (panel->id() == financials_id && panel->windowOpen())
        {
            return panel.get();
        }
    }
    return nullptr;
}

void CChartBook::addPane()
{
    auto pane = std::make_unique<CChartPane>(next_id_);
    pane->setWindowScope(runtime_id_);
    pane->attachSymbolLink(symbol_link_);
    focused_id_ = next_id_;
    pane->requestFocus();
    chartbookInsertPane(layout_, next_id_);
    ++next_id_;
    panes_.push_back(std::move(pane));
    layout_request_ = true;
}

void CChartBook::addFinancials()
{
    auto panel = std::make_unique<FinancialsPanel>(next_financials_id_);
    panel->setWindowScope(runtime_id_);
    panel->attachSymbolLink(symbol_link_);
    focused_financials_id_ = next_financials_id_;
    panel->requestFocus();
    chartbookInsertFinancials(layout_, next_financials_id_);
    ++next_financials_id_;
    financials_.push_back(std::move(panel));
    layout_request_ = true;
}

void CChartBook::addOptions()
{
    auto panel = std::make_unique<OptionsChainPanel>(next_options_id_);
    panel->setWindowScope(runtime_id_);
    panel->attachSymbolLink(symbol_link_);
    focused_options_id_ = next_options_id_;
    panel->requestFocus();
    chartbookInsertOptions(layout_, next_options_id_);
    ++next_options_id_;
    options_.push_back(std::move(panel));
    layout_request_ = true;
}

void CChartBook::addPortfolio()
{
    auto panel = std::make_unique<PortfolioPanel>(next_portfolio_id_);
    panel->setWindowScope(runtime_id_);
    focused_portfolio_id_ = next_portfolio_id_;
    panel->requestFocus();
    chartbookInsertPortfolio(layout_, next_portfolio_id_);
    ++next_portfolio_id_;
    portfolios_.push_back(std::move(panel));
    layout_request_ = true;
}

void CChartBook::addPayoff()
{
    auto panel = std::make_unique<PayoffPanel>(next_payoff_id_);
    panel->setWindowScope(runtime_id_);
    focused_payoff_id_ = next_payoff_id_;
    panel->requestFocus();
    chartbookInsertPayoff(layout_, next_payoff_id_);
    ++next_payoff_id_;
    payoffs_.push_back(std::move(panel));
    layout_request_ = true;
}

void CChartBook::closeFocused()
{
    if (CChartPane* pane = focused())
    {
        pane->closeWindow();
    }
}

void CChartBook::closeFocusedFinancials()
{
    if (FinancialsPanel* panel = focusedFinancialsPanel())
    {
        panel->closeWindow();
    }
}

void CChartBook::closeFocusedOptions()
{
    if (OptionsChainPanel* panel = focusedOptionsPanel())
    {
        panel->closeWindow();
    }
}

void CChartBook::closeFocusedPortfolio()
{
    if (PortfolioPanel* panel = focusedPortfolioPanel())
    {
        panel->closeWindow();
    }
}

void CChartBook::closeFocusedPayoff()
{
    if (PayoffPanel* panel = focusedPayoffPanel())
    {
        panel->closeWindow();
    }
}

void CChartBook::openFocusedSettings()
{
    if (CChartPane* pane = focused())
    {
        pane->requestFocus();
        pane->openSettings();
    }
}

void CChartBook::openFocusedStudies()
{
    if (CChartPane* pane = focused())
    {
        pane->requestFocus();
        pane->openStudies();
    }
}

void CChartBook::loadDocument(const CChartbookDocument& document)
{
    panes_.clear();
    financials_.clear();
    options_.clear();
    portfolios_.clear();
    payoffs_.clear();
    name_ = document.name;
    data_ = document.data;
    layout_ = document.layout;
    floating_ = document.floating;
    next_id_ = std::max(document.next_pane_id, 1);
    focused_id_ = document.focused_pane;
    next_financials_id_ = std::max(document.next_financials_id, 1);
    focused_financials_id_ = document.focused_financials;
    next_options_id_ = std::max(document.next_options_id, 1);
    focused_options_id_ = document.focused_options;
    next_portfolio_id_ = std::max(document.next_portfolio_id, 1);
    focused_portfolio_id_ = document.focused_portfolio;
    next_payoff_id_ = std::max(document.next_payoff_id, 1);
    focused_payoff_id_ = document.focused_payoff;
    for (const ChartbookFinancials& record : document.financials)
    {
        if (!chartbookFinancialsIsOpen(document, record.id))
        {
            continue;
        }
        auto panel = std::make_unique<FinancialsPanel>(record.id);
        panel->setWindowScope(runtime_id_);
        panel->importState(record);
        panel->attachSymbolLink(symbol_link_);
        panel->setSymbolLinkGroup(record.link_group);
        if (record.id == focused_financials_id_)
        {
            panel->requestFocus();
        }
        financials_.push_back(std::move(panel));
    }
    if (findFinancials(focused_financials_id_) == nullptr)
    {
        focused_financials_id_ = financials_.empty() ? 0 : financials_.front()->id();
    }
    for (const ChartbookOptions& record : document.options)
    {
        if (!chartbookOptionsIsOpen(document, record.id))
        {
            continue;
        }
        auto panel = std::make_unique<OptionsChainPanel>(record.id);
        panel->setWindowScope(runtime_id_);
        panel->importState(record);
        panel->attachSymbolLink(symbol_link_);
        panel->setSymbolLinkGroup(record.link_group);
        if (record.id == focused_options_id_)
        {
            panel->requestFocus();
        }
        options_.push_back(std::move(panel));
    }
    if (findOptions(focused_options_id_) == nullptr)
    {
        focused_options_id_ = options_.empty() ? 0 : options_.front()->id();
    }
    for (const ChartbookPortfolio& record : document.portfolios)
    {
        if (!chartbookPortfolioIsOpen(document, record.id))
        {
            continue;
        }
        auto panel = std::make_unique<PortfolioPanel>(record.id);
        panel->setWindowScope(runtime_id_);
        panel->importState(record);
        if (record.id == focused_portfolio_id_)
        {
            panel->requestFocus();
        }
        portfolios_.push_back(std::move(panel));
    }
    if (findPortfolio(focused_portfolio_id_) == nullptr)
    {
        focused_portfolio_id_ = portfolios_.empty() ? 0 : portfolios_.front()->id();
    }
    for (const ChartbookPayoff& record : document.payoffs)
    {
        if (!chartbookPayoffIsOpen(document, record.id))
        {
            continue;
        }
        auto panel = std::make_unique<PayoffPanel>(record.id);
        panel->setWindowScope(runtime_id_);
        panel->importState(record);
        if (record.id == focused_payoff_id_)
        {
            panel->requestFocus();
        }
        payoffs_.push_back(std::move(panel));
    }
    if (findPayoff(focused_payoff_id_) == nullptr)
    {
        focused_payoff_id_ = payoffs_.empty() ? 0 : payoffs_.front()->id();
    }
    for (const ChartbookPane& record : document.panes)
    {
        if (!chartbookPaneIsOpen(document, record.id))
        {
            continue;
        }
        auto pane = std::make_unique<CChartPane>(record.id);
        pane->setWindowScope(runtime_id_);
        pane->importRecord(record);
        pane->attachSymbolLink(symbol_link_);
        pane->setSymbolLinkGroup(record.link_group);
        if (record.id == focused_id_)
        {
            pane->requestFocus();
        }
        panes_.push_back(std::move(pane));
    }
    if (findPane(focused_id_) == nullptr)
    {
        focused_id_ = panes_.empty() ? 0 : panes_.front()->id();
    }
}

CChartbookDocument CChartBook::exportDocument() const
{
    CChartbookDocument document;
    document.name = name_;
    document.focused_pane = focused_id_;
    document.next_pane_id = next_id_;
    document.focused_financials = focused_financials_id_;
    document.next_financials_id = next_financials_id_;
    document.focused_options = focused_options_id_;
    document.next_options_id = next_options_id_;
    document.focused_portfolio = focused_portfolio_id_;
    document.next_portfolio_id = next_portfolio_id_;
    document.focused_payoff = focused_payoff_id_;
    document.next_payoff_id = next_payoff_id_;
    document.data = data_;
    document.layout = layout_;
    document.floating = floating_;
    for (const std::unique_ptr<CChartPane>& pane : panes_)
    {
        if (pane->windowOpen())
        {
            document.panes.push_back(pane->exportRecord());
        }
    }
    for (const std::unique_ptr<FinancialsPanel>& panel : financials_)
    {
        if (panel->windowOpen())
        {
            document.financials.push_back(panel->exportState());
        }
    }
    for (const std::unique_ptr<OptionsChainPanel>& panel : options_)
    {
        if (panel->windowOpen())
        {
            document.options.push_back(panel->exportState());
        }
    }
    for (const std::unique_ptr<PortfolioPanel>& panel : portfolios_)
    {
        if (panel->windowOpen())
        {
            document.portfolios.push_back(panel->exportState());
        }
    }
    for (const std::unique_ptr<PayoffPanel>& panel : payoffs_)
    {
        if (panel->windowOpen())
        {
            document.payoffs.push_back(panel->exportState());
        }
    }
    dropClosedPanes(document);
    return document;
}

int CChartBook::runtimeId() const noexcept
{
    return runtime_id_;
}

const std::string& CChartBook::name() const noexcept
{
    return name_;
}

void CChartBook::setName(std::string name)
{
    name_ = std::move(name);
}

const ChartbookData& CChartBook::data() const noexcept
{
    return data_;
}

void CChartBook::setData(ChartbookData data)
{
    data_ = std::move(data);
}

const ChartbookLayout& CChartBook::layout() const noexcept
{
    return layout_;
}

void CChartBook::setLayout(ChartbookLayout layout)
{
    layout_ = std::move(layout);
}

const std::vector<ChartbookFloating>& CChartBook::floating() const noexcept
{
    return floating_;
}

void CChartBook::setFloating(std::vector<ChartbookFloating> floating)
{
    floating_ = std::move(floating);
}

bool CChartBook::consumeLayoutRequest() noexcept
{
    const bool requested = layout_request_;
    layout_request_ = false;
    return requested;
}

void CChartBook::setWindowScope(int runtime_id)
{
    runtime_id_ = runtime_id;
    for (const std::unique_ptr<FinancialsPanel>& panel : financials_)
    {
        panel->setWindowScope(runtime_id_);
    }
    for (const std::unique_ptr<OptionsChainPanel>& panel : options_)
    {
        panel->setWindowScope(runtime_id_);
    }
    for (const std::unique_ptr<PortfolioPanel>& panel : portfolios_)
    {
        panel->setWindowScope(runtime_id_);
    }
    for (const std::unique_ptr<PayoffPanel>& panel : payoffs_)
    {
        panel->setWindowScope(runtime_id_);
    }
    for (const std::unique_ptr<CChartPane>& pane : panes_)
    {
        pane->setWindowScope(runtime_id_);
    }
}

void CChartBook::placePane(int pane_id, bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    for (const std::unique_ptr<CChartPane>& pane : panes_)
    {
        if (pane->id() == pane_id)
        {
            pane->setPlacement(force, floating, dock, pos, size);
        }
    }
}

bool CChartBook::containsPane(int pane_id) const
{
    return findPane(pane_id) != nullptr;
}

bool CChartBook::containsFinancials(int financials_id) const
{
    return findFinancials(financials_id) != nullptr;
}

bool CChartBook::containsOptions(int options_id) const
{
    return findOptions(options_id) != nullptr;
}

bool CChartBook::containsPortfolio(int portfolio_id) const
{
    return findPortfolio(portfolio_id) != nullptr;
}

bool CChartBook::containsPayoff(int payoff_id) const
{
    return findPayoff(payoff_id) != nullptr;
}

void CChartBook::drawFinancials(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    for (const std::unique_ptr<FinancialsPanel>& panel : financials_)
    {
        panel->setWindowScope(runtime_id_);
        if (panel->draw(store, store_error, ingest))
        {
            focused_financials_id_ = panel->id();
        }
    }
    eraseClosedFinancials();
}

void CChartBook::placeFinancials(int financials_id, bool force, bool floating, ImGuiID dock, ImVec2 pos,
                                 ImVec2 size)
{
    for (const std::unique_ptr<FinancialsPanel>& panel : financials_)
    {
        if (panel->id() == financials_id)
        {
            panel->setPlacement(force, floating, dock, pos, size);
        }
    }
}

void CChartBook::eraseClosedOptions()
{
    const auto removed = std::ranges::remove_if(options_, [](const std::unique_ptr<OptionsChainPanel>& panel) {
        return !panel->windowOpen();
    });
    options_.erase(removed.begin(), removed.end());
    if (focusedOptionsPanel() == nullptr)
    {
        focused_options_id_ = 0;
    }
}

OptionsChainPanel* CChartBook::focusedOptionsPanel()
{
    if (focused_options_id_ == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<OptionsChainPanel>& panel : options_)
    {
        if (panel->id() == focused_options_id_ && panel->windowOpen())
        {
            return panel.get();
        }
    }
    return nullptr;
}

const OptionsChainPanel* CChartBook::focusedOptions() const
{
    return findOptions(focused_options_id_);
}

const OptionsChainPanel* CChartBook::findOptions(int options_id) const
{
    if (options_id == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<OptionsChainPanel>& panel : options_)
    {
        if (panel->id() == options_id && panel->windowOpen())
        {
            return panel.get();
        }
    }
    return nullptr;
}

void CChartBook::drawOptions(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    for (const std::unique_ptr<OptionsChainPanel>& panel : options_)
    {
        panel->setWindowScope(runtime_id_);
        if (panel->draw(store, store_error, ingest))
        {
            focused_options_id_ = panel->id();
        }
    }
    eraseClosedOptions();
}

void CChartBook::placeOptions(int options_id, bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    for (const std::unique_ptr<OptionsChainPanel>& panel : options_)
    {
        if (panel->id() == options_id)
        {
            panel->setPlacement(force, floating, dock, pos, size);
        }
    }
}

void CChartBook::eraseClosedPortfolios()
{
    const auto removed = std::ranges::remove_if(portfolios_, [](const std::unique_ptr<PortfolioPanel>& panel) {
        return !panel->windowOpen();
    });
    portfolios_.erase(removed.begin(), removed.end());
    if (focusedPortfolioPanel() == nullptr)
    {
        focused_portfolio_id_ = 0;
    }
}

void CChartBook::eraseClosedPayoffs()
{
    const auto removed = std::ranges::remove_if(payoffs_, [](const std::unique_ptr<PayoffPanel>& panel) {
        return !panel->windowOpen();
    });
    payoffs_.erase(removed.begin(), removed.end());
    if (focusedPayoffPanel() == nullptr)
    {
        focused_payoff_id_ = 0;
    }
}

PortfolioPanel* CChartBook::focusedPortfolioPanel()
{
    if (focused_portfolio_id_ == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<PortfolioPanel>& panel : portfolios_)
    {
        if (panel->id() == focused_portfolio_id_ && panel->windowOpen())
        {
            return panel.get();
        }
    }
    return nullptr;
}

PayoffPanel* CChartBook::focusedPayoffPanel()
{
    if (focused_payoff_id_ == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<PayoffPanel>& panel : payoffs_)
    {
        if (panel->id() == focused_payoff_id_ && panel->windowOpen())
        {
            return panel.get();
        }
    }
    return nullptr;
}

const PortfolioPanel* CChartBook::focusedPortfolio() const
{
    return findPortfolio(focused_portfolio_id_);
}

const PayoffPanel* CChartBook::focusedPayoff() const
{
    return findPayoff(focused_payoff_id_);
}

const PortfolioPanel* CChartBook::findPortfolio(int portfolio_id) const
{
    if (portfolio_id == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<PortfolioPanel>& panel : portfolios_)
    {
        if (panel->id() == portfolio_id && panel->windowOpen())
        {
            return panel.get();
        }
    }
    return nullptr;
}

const PayoffPanel* CChartBook::findPayoff(int payoff_id) const
{
    if (payoff_id == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<PayoffPanel>& panel : payoffs_)
    {
        if (panel->id() == payoff_id && panel->windowOpen())
        {
            return panel.get();
        }
    }
    return nullptr;
}

void CChartBook::drawPortfolios(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    for (const std::unique_ptr<PortfolioPanel>& panel : portfolios_)
    {
        panel->setWindowScope(runtime_id_);
        if (panel->draw(store, store_error, ingest))
        {
            focused_portfolio_id_ = panel->id();
        }
    }
    eraseClosedPortfolios();
}

void CChartBook::drawPayoffs(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    for (const std::unique_ptr<PayoffPanel>& panel : payoffs_)
    {
        panel->setWindowScope(runtime_id_);
        if (panel->draw(store, store_error, ingest))
        {
            focused_payoff_id_ = panel->id();
        }
    }
    eraseClosedPayoffs();
}

void CChartBook::placePortfolio(int portfolio_id, bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    for (const std::unique_ptr<PortfolioPanel>& panel : portfolios_)
    {
        if (panel->id() == portfolio_id)
        {
            panel->setPlacement(force, floating, dock, pos, size);
        }
    }
}

void CChartBook::placePayoff(int payoff_id, bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    for (const std::unique_ptr<PayoffPanel>& panel : payoffs_)
    {
        if (panel->id() == payoff_id)
        {
            panel->setPlacement(force, floating, dock, pos, size);
        }
    }
}

void CChartBook::drawMenu()
{
    if (beginTitleMenu("Chart"))
    {
        if (ImGui::MenuItem("New Chart"))
        {
            addPane();
        }
        CChartPane const* pane = focused();
        const bool has_focus = pane != nullptr;
        const bool settings_enabled = has_focus && !pane->studiesOpen();
        const bool studies_enabled = has_focus && !pane->settingsOpen();
        if (ImGui::MenuItem("Chart Settings", "F5", false, settings_enabled))
        {
            openFocusedSettings();
        }
        if (ImGui::MenuItem("Studies", "F6", false, studies_enabled))
        {
            openFocusedStudies();
        }
        if (ImGui::MenuItem("Close Chart", nullptr, false, has_focus))
        {
            closeFocused();
        }
        endTitleMenu();
    }
}

void CChartBook::draw(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    for (std::unique_ptr<CChartPane> const& pane : panes_)
    {
        if (pane->draw(store, store_error, ingest))
        {
            focused_id_ = pane->id();
        }
    }
    eraseClosed();
}

}  // namespace terminal
