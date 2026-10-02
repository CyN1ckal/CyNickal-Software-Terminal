// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/StatsPanel.h"

#include "data/IngestWorker.h"
#include "ui/ReceivedStamp.h"
#include "ui/Theme.h"
#include "ui/TradingFormat.h"
#include "ui/TradingWidgets.h"

#include "market_data/Store.h"
#include "market_data/Time.h"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace terminal {
namespace {

constexpr const char* kNoValue = "\xE2\x80\x94";  // em dash

void copyInto(char* buffer, std::size_t size, std::string_view text)
{
    const std::size_t count = text.copy(buffer, size - 1);
    buffer[count] = '\0';
}

[[nodiscard]] std::string trimmedUpper(const char* text)
{
    std::string out;
    for (const char* ch = text; *ch != '\0'; ++ch)
    {
        if (*ch != ' ' && *ch != '\t')
        {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(*ch))));
        }
    }
    return out;
}

[[nodiscard]] std::string ratioText(const std::optional<double>& value)
{
    if (!value.has_value() || !std::isfinite(*value))
    {
        return kNoValue;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", *value);
    return buf;
}

[[nodiscard]] std::string percentText(const std::optional<double>& value)
{
    if (!value.has_value() || !std::isfinite(*value))
    {
        return kNoValue;
    }
    return formatPercent(*value);
}

void sectionRow(const char* title)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextColored(Theme::kAccent, "%s", title);
}

void valueRow(const char* label, const std::string& value, const ImVec4& color)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextColored(Theme::kTextDim, "%s", label);
    ImGui::TableSetColumnIndex(1);
    drawRightText(value, color);
}

void valueRow(const char* label, const std::string& value)
{
    valueRow(label, value, Theme::kText);
}

[[nodiscard]] ImVec4 signColor(double value)
{
    if (value > 0.0)
    {
        return Theme::kUp;
    }
    if (value < 0.0)
    {
        return Theme::kDown;
    }
    return Theme::kText;
}

void moneyRow(const char* label, double amount, bool signed_color)
{
    valueRow(label, formatMoney(amount), signed_color ? signColor(amount) : Theme::kText);
}

}  // namespace

StatsPanel::StatsPanel(int id) : benchmark_applied_("SPY"), id_(id)
{
    copyInto(benchmark_, sizeof(benchmark_), benchmark_applied_);
}

int StatsPanel::id() const noexcept
{
    return id_;
}

bool StatsPanel::windowOpen() const noexcept
{
    return window_open_;
}

void StatsPanel::closeWindow()
{
    window_open_ = false;
}

void StatsPanel::requestFocus()
{
    focus_on_appear_ = true;
}

void StatsPanel::importState(const ChartbookStats& state)
{
    ledger_id_ = state.ledger_id;
    copyInto(benchmark_, sizeof(benchmark_), state.benchmark);
    benchmark_applied_ = trimmedUpper(benchmark_);
    loaded_ = false;
    stale_ = true;
}

ChartbookStats StatsPanel::exportState() const
{
    ChartbookStats state;
    state.id = id_;
    state.ledger_id = ledger_id_;
    state.benchmark = benchmark_applied_;
    return state;
}

void StatsPanel::showLedger(LedgerId id)
{
    ledger_id_ = id;
    loaded_ = false;
    fetched_missing_ = false;
    stale_ = true;
    error_.clear();
}

void StatsPanel::setWindowScope(int runtime_id) noexcept
{
    runtime_id_ = runtime_id;
}

void StatsPanel::setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    place_force_ = force;
    place_floating_ = floating;
    place_dock_ = dock;
    place_pos_ = pos;
    place_size_ = size;
}

void StatsPanel::reload(const Store& store, IngestWorker* ingest)
{
    ledgers_ = store.listLedgers();
    if (ledger_id_ == 0)
    {
        return;
    }
    const auto found = std::ranges::find_if(ledgers_, [&](const Ledger& ledger) { return ledger.id == ledger_id_; });
    if (found == ledgers_.end())
    {
        ledger_id_ = 0;
        ledger_name_.clear();
        analysis_ = {};
        loaded_ = false;
        status_ = "ledger was deleted";
        return;
    }
    if (!loaded_ || found->updated_at != loaded_updated_)
    {
        ledger_name_ = found->name;
        ledger_kind_ = found->kind;
        loaded_updated_ = found->updated_at;
        loaded_ = true;
        stale_ = true;
        if (ingest != nullptr && !fetched_missing_)
        {
            fetched_missing_ = true;
            fetchHistory(store, *ingest, true);
        }
    }
}

void StatsPanel::fetchHistory(const Store& store, IngestWorker& ingest, bool missing_only)
{
    const std::vector<TradeFill> fills = store.queryFills(ledger_id_);
    const SessionDate today = utcToSessionDate(kLedgerTimezone, nowUtc());
    const std::vector<IngestWorker::Job> jobs =
        ledgerHistoryFetchJobs(store, fills, benchmark_applied_, today, missing_only);
    std::uint64_t serial = 0;
    for (const IngestWorker::Job& job : jobs)
    {
        serial = std::max(serial, ingest.enqueue(job).serial);
    }
    if (serial != 0)
    {
        refresh_serial_ = serial;
        status_ = "fetching";
    }
}

void StatsPanel::analyze(const Store& store)
{
    stale_ = false;
    std::optional<InstrumentId> benchmark;
    benchmark_found_ = false;
    if (!benchmark_applied_.empty())
    {
        if (const std::optional<Instrument> instrument = store.resolveSymbol(benchmark_applied_);
            instrument.has_value())
        {
            benchmark = instrument->id;
            benchmark_found_ = true;
        }
    }
    analysis_ = analyzeLedger(store, ledger_id_, benchmark, nowUtc());

    times_.clear();
    return_pct_.clear();
    drawdown_pct_.clear();
    for (const EquityPoint& point : analysis_.curve)
    {
        times_.push_back(static_cast<double>(point.ts));
        return_pct_.push_back((point.growth - 1.0) * 100.0);
        drawdown_pct_.push_back(point.drawdown * 100.0);
    }
    benchmark_times_.clear();
    benchmark_pct_.clear();
    if (!analysis_.benchmark_marks.empty() && analysis_.benchmark_marks.front().price > 0.0)
    {
        const double base = analysis_.benchmark_marks.front().price;
        for (const Mark& mark : analysis_.benchmark_marks)
        {
            benchmark_times_.push_back(static_cast<double>(mark.ts));
            benchmark_pct_.push_back(((mark.price / base) - 1.0) * 100.0);
        }
    }
    win_x_.clear();
    win_y_.clear();
    loss_x_.clear();
    loss_y_.clear();
    for (std::size_t index = 0; index < analysis_.round_trips.size(); ++index)
    {
        const double net = analysis_.round_trips[index].net_pnl;
        const auto trade = static_cast<double>(index + 1);
        if (net >= 0.0)
        {
            win_x_.push_back(trade);
            win_y_.push_back(net);
        }
        else
        {
            loss_x_.push_back(trade);
            loss_y_.push_back(net);
        }
    }
    if (status_ != "fetching")
    {
        status_ = ledger_kind_ == LedgerKind::Backtest ? ledger_name_ + " (backtest)" : ledger_name_;
    }
}

void StatsPanel::requestData(Store* store, IngestWorker* ingest)
{
    if (store == nullptr || ingest == nullptr || ledger_id_ == 0)
    {
        return;
    }
    try
    {
        fetchHistory(*store, *ingest, false);
    }
    catch (const std::exception& ex)
    {
        error_ = ex.what();
        status_ = error_;
    }
}

void StatsPanel::drawHeader()
{
    std::string preview = ledger_name_.empty() ? std::string("select a ledger") : ledger_name_;
    if (ledger_kind_ == LedgerKind::Backtest && !ledger_name_.empty())
    {
        preview += " (backtest)";
    }
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::BeginCombo("##ledger", preview.c_str()))
    {
        for (const Ledger& ledger : ledgers_)
        {
            ImGui::PushID(static_cast<int>(ledger.id));
            const std::string label =
                ledger.kind == LedgerKind::Backtest ? ledger.name + " (backtest)" : ledger.name;
            if (ImGui::Selectable(label.c_str(), ledger.id == ledger_id_) && ledger.id != ledger_id_)
            {
                ledger_id_ = ledger.id;
                loaded_ = false;
                fetched_missing_ = false;
                error_.clear();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextColored(Theme::kTextDim, "Benchmark");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.f);
    ImGui::InputTextWithHint("##benchmark", "none", benchmark_, sizeof(benchmark_));
    if (ImGui::IsItemDeactivated())
    {
        const std::string applied = trimmedUpper(benchmark_);
        copyInto(benchmark_, sizeof(benchmark_), applied);
        if (applied != benchmark_applied_)
        {
            benchmark_applied_ = applied;
            stale_ = true;
            fetched_missing_ = false;
            loaded_ = false;
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Compared by buy-and-hold over the same span. Empty for none.");
    }
}

void StatsPanel::drawTable() const
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                      ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable("stats_table", 2, flags))
    {
        return;
    }
    ImGui::TableSetupColumn("Measure", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 110.f);
    const CurveStats& curve = analysis_.curve_stats;
    const TradeStats& trades = analysis_.trade_stats;

    sectionRow("Returns");
    moneyRow("Net profit", curve.net_pnl, true);
    valueRow("Total return", formatPercent(curve.total_return), signColor(curve.total_return));
    valueRow("CAGR", percentText(curve.cagr));
    if (!benchmark_applied_.empty())
    {
        const std::string label = "Buy and hold " + benchmark_applied_;
        valueRow(label.c_str(), benchmark_found_ ? percentText(analysis_.benchmark_return) : std::string("not stored"));
    }
    valueRow("Exposure", formatPercent(curve.exposure, 1));

    sectionRow("Risk");
    valueRow("Volatility", percentText(curve.volatility));
    valueRow("Sharpe", ratioText(curve.sharpe));
    valueRow("Sortino", ratioText(curve.sortino));
    valueRow("Calmar", ratioText(curve.calmar));
    valueRow("Max drawdown", formatPercent(curve.max_drawdown), curve.max_drawdown < 0.0 ? Theme::kDown : Theme::kText);
    valueRow("Longest drawdown", curve.max_drawdown_seconds > 0 ? formatDuration(curve.max_drawdown_seconds)
                                                                 : std::string(kNoValue));
    if (curve.var.has_value())
    {
        valueRow("VaR 95%, one period", formatMoney(curve.var->var));
        valueRow("CVaR 95%, one period", formatMoney(curve.var->cvar));
    }
    else
    {
        valueRow("VaR 95%, one period", kNoValue);
    }
    valueRow("Periods", std::to_string(curve.periods));

    sectionRow("Trades");
    valueRow("Closed trades", std::to_string(trades.trades));
    valueRow("Win rate", trades.trades > 0 ? formatPercent(trades.win_rate, 1) : std::string(kNoValue));
    valueRow("Profit factor", ratioText(trades.profit_factor));
    valueRow("Payoff ratio", ratioText(trades.payoff_ratio));
    const auto moneyOrDash = [](const char* label, double amount, bool present) {
        if (present)
        {
            moneyRow(label, amount, true);
        }
        else
        {
            valueRow(label, kNoValue);
        }
    };
    moneyOrDash("Average trade", trades.average_trade, trades.trades > 0);
    moneyOrDash("Average win", trades.average_win, trades.wins > 0);
    moneyOrDash("Average loss", trades.average_loss, trades.losses > 0);
    moneyOrDash("Largest win", trades.largest_win, trades.wins > 0);
    moneyOrDash("Largest loss", trades.largest_loss, trades.losses > 0);
    valueRow("Wins in a row", std::to_string(trades.max_consecutive_wins));
    valueRow("Losses in a row", std::to_string(trades.max_consecutive_losses));
    valueRow("Average hold", trades.trades > 0
                                 ? formatDuration(static_cast<UnixSeconds>(trades.average_holding_seconds))
                                 : std::string(kNoValue));
    moneyRow("Fees", trades.fees, false);
    ImGui::EndTable();
}

void StatsPanel::drawCharts() const
{
    const float avail = ImGui::GetContentRegionAvail().y;
    const float curves_h = std::max(120.f, avail * 0.66f);
    constexpr ImPlotFlags plot_flags = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect;
    float row_ratios[] = {3.f, 1.f};  // BeginSubplots takes a mutable pointer.
    if (ImPlot::BeginSubplots("##curves", 2, 1, ImVec2(-1.f, curves_h),
                              ImPlotSubplotFlags_LinkAllX | ImPlotSubplotFlags_NoTitle, row_ratios, nullptr))
    {
        if (ImPlot::BeginPlot("##return", ImVec2(), plot_flags))
        {
            ImPlot::SetupAxes(nullptr, "Return %", ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_AutoFit,
                              ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_NoMenus);
            if (!benchmark_pct_.empty())
            {
                ImPlotSpec spec;
                spec.LineColor = Theme::kTextDim;
                const std::string label = benchmark_applied_ + " buy and hold";
                ImPlot::PlotLine(label.c_str(), benchmark_times_.data(), benchmark_pct_.data(),
                                 static_cast<int>(benchmark_pct_.size()), spec);
            }
            ImPlotSpec spec;
            spec.LineColor = Theme::kAccent;
            spec.LineWeight = 1.5f;
            ImPlot::PlotLine("Ledger", times_.data(), return_pct_.data(), static_cast<int>(times_.size()), spec);
            ImPlot::EndPlot();
        }
        if (ImPlot::BeginPlot("##drawdown", ImVec2(), plot_flags | ImPlotFlags_NoLegend))
        {
            ImPlot::SetupAxes(nullptr, "Drawdown %", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
            ImPlotSpec fill;
            fill.FillColor = Theme::kDown;
            fill.FillAlpha = 0.35f;
            ImPlot::PlotShaded("Drawdown", times_.data(), drawdown_pct_.data(), static_cast<int>(times_.size()), 0.0,
                               fill);
            ImPlotSpec line;
            line.LineColor = Theme::kDown;
            ImPlot::PlotLine("Drawdown", times_.data(), drawdown_pct_.data(), static_cast<int>(times_.size()), line);
            ImPlot::EndPlot();
        }
        ImPlot::EndSubplots();
    }
    if (ImPlot::BeginPlot("##trades", ImVec2(-1.f, -1.f), plot_flags | ImPlotFlags_NoLegend))
    {
        ImPlot::SetupAxes("Closed trade", "Net profit", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        // One tick per trade while they fit; a longer history keeps ImPlot's own spacing.
        const std::size_t trades = analysis_.round_trips.size();
        if (trades > 0 && trades <= 30)
        {
            ImPlot::SetupAxisTicks(ImAxis_X1, 1.0, static_cast<double>(trades), static_cast<int>(trades));
        }
        ImPlot::SetupAxisFormat(ImAxis_X1, "%.0f");
        ImPlotSpec win;
        win.FillColor = Theme::kUp;
        win.LineColor = Theme::kUp;
        ImPlot::PlotBars("Win", win_x_.data(), win_y_.data(), static_cast<int>(win_x_.size()), 0.7, win);
        ImPlotSpec loss;
        loss.FillColor = Theme::kDown;
        loss.LineColor = Theme::kDown;
        ImPlot::PlotBars("Loss", loss_x_.data(), loss_y_.data(), static_cast<int>(loss_x_.size()), 0.7, loss);
        ImPlot::EndPlot();
    }
}

bool StatsPanel::draw(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    if (focus_on_appear_)
    {
        ImGui::SetNextWindowFocus();
        focus_on_appear_ = false;
    }
    if (place_force_)
    {
        if (place_floating_)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + place_pos_.x, viewport->WorkPos.y + place_pos_.y),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowSize(place_size_, ImGuiCond_Always);
            ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
            ImGui::SetNextWindowViewport(viewport->ID);
        }
        else if (place_dock_ != 0)
        {
            ImGui::SetNextWindowDockID(place_dock_, ImGuiCond_Always);
        }
        place_force_ = false;
    }

    const std::string title = (ledger_name_.empty() ? std::string("STATISTICS") : ledger_name_ + " statistics") +
                              "###cb" + std::to_string(runtime_id_) + "_stats" + std::to_string(id_);
    if (!ImGui::Begin(title.c_str(), &window_open_, ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return false;
    }
    if (store == nullptr)
    {
        if (!store_error.empty())
        {
            ImGui::TextColored(Theme::kDown, "%s", std::string(store_error).c_str());
        }
        ImGui::End();
        return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    try
    {
        reload(*store, ingest);
        if (ingest != nullptr)
        {
            const std::uint64_t finished = ingest->snapshot().finished_serial;
            if (finished != priced_serial_)
            {
                priced_serial_ = finished;
                stale_ = true;
            }
            if (refresh_serial_ != 0 && finished >= refresh_serial_)
            {
                refresh_serial_ = 0;
                status_.clear();
            }
        }
        if (stale_ && ledger_id_ != 0)
        {
            analyze(*store);
        }
    }
    catch (const std::exception& ex)
    {
        stale_ = false;
        error_ = ex.what();
        status_ = error_;
    }

    drawHeader();
    ImGui::Separator();
    ImVec4 status_color = Theme::kMuted;
    if (refresh_serial_ != 0)
    {
        status_color = Theme::kAccent;
    }
    else if (!error_.empty())
    {
        status_color = Theme::kDown;
    }
    ImGui::TextColored(status_color, "%s", status_.c_str());
    drawReceivedStamp(analysis_.received_at);
    if (ledger_id_ == 0)
    {
        ImGui::TextColored(Theme::kTextFaint, "Pick a ledger to measure.");
    }
    else if (analysis_.curve.empty())
    {
        ImGui::TextColored(Theme::kTextFaint, "This ledger has no fills or cash flows yet.");
    }
    else
    {
        if (!analysis_.unpriced_symbols.empty())
        {
            std::string names;
            for (const std::string& symbol : analysis_.unpriced_symbols)
            {
                names += names.empty() ? symbol : ", " + symbol;
            }
            ImGui::TextColored(Theme::kMuted, "No daily closes for %s; held at cost. Ctrl+R fetches them.",
                               names.c_str());
        }
        if (ImGui::BeginChild("stats_left", ImVec2(300.f, 0.f), ImGuiChildFlags_Borders))
        {
            drawTable();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if (ImGui::BeginChild("stats_right", ImVec2(0.f, 0.f), ImGuiChildFlags_Borders))
        {
            drawCharts();
        }
        ImGui::EndChild();
    }
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::End();
    return focused;
}

}  // namespace terminal
