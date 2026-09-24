// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/FinancialsPanel.h"

#include "chart/CChartLoad.h"
#include "data/IngestWorker.h"
#include "ui/Theme.h"

#include "market_data/Store.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace terminal {
namespace {

constexpr StatementKind kStatements[] = {
    StatementKind::Income,
    StatementKind::Balance,
    StatementKind::Cashflow,
};

constexpr StatementTimeframe kPeriods[] = {
    StatementTimeframe::Annually,
    StatementTimeframe::Quarterly,
};

[[nodiscard]] const char* statementLabel(StatementKind statement)
{
    switch (statement)
    {
    case StatementKind::Income:
        return "Income";
    case StatementKind::Balance:
        return "Balance";
    case StatementKind::Cashflow:
        return "Cash Flow";
    }
    return "Income";
}

[[nodiscard]] const char* periodLabel(StatementTimeframe timeframe)
{
    switch (timeframe)
    {
    case StatementTimeframe::Annually:
        return "Yearly";
    case StatementTimeframe::Quarterly:
        return "Quarterly";
    case StatementTimeframe::Trailing:
        return "Trailing";
    }
    return "Yearly";
}

[[nodiscard]] bool isBusyError(std::string_view what) noexcept
{
    return what.find("busy") != std::string_view::npos || what.find("locked") != std::string_view::npos;
}

void drawRight(const char* text, const ImVec4& color)
{
    const float width = ImGui::GetContentRegionAvail().x;
    const float text_w = ImGui::CalcTextSize(text).x;
    if (text_w < width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width - text_w);
    }
    ImGui::TextColored(color, "%s", text);
}

}  // namespace

FinancialsPanel::FinancialsPanel(int id) : id_(id) {}

int FinancialsPanel::id() const noexcept
{
    return id_;
}

bool FinancialsPanel::windowOpen() const noexcept
{
    return window_open_;
}

void FinancialsPanel::closeWindow()
{
    window_open_ = false;
}

void FinancialsPanel::requestFocus()
{
    focus_on_appear_ = true;
}

void FinancialsPanel::importState(const ChartbookFinancials& state)
{
    active_symbol_ = normalizeChartSymbol(state.symbol);
    active_figi_ = state.figi;
    std::snprintf(symbol_, sizeof(symbol_), "%s", active_symbol_.c_str());
    try
    {
        statement_ = statementKindFromSql(state.statement);
    }
    catch (const std::exception&)
    {
        statement_ = StatementKind::Income;
    }
    try
    {
        timeframe_ = statementTimeframeFromSql(state.timeframe);
    }
    catch (const std::exception&)
    {
        timeframe_ = StatementTimeframe::Annually;
    }
    if (timeframe_ == StatementTimeframe::Trailing)
    {
        timeframe_ = StatementTimeframe::Annually;
    }
    sheet_ = {};
    loaded_key_.clear();
    failed_key_.clear();
    inflight_key_.clear();
    inflight_serial_ = 0;
    inflight_ = false;
    have_snapshot_ = false;
    busy_ = false;
    blocked_ = false;
    fetch_now_ = false;
    needs_reload_ = true;
    error_.clear();
    status_ = active_symbol_.empty() ? "enter a symbol" : "not fetched";
}

ChartbookFinancials FinancialsPanel::exportState() const
{
    ChartbookFinancials state;
    state.id = id_;
    state.symbol = active_symbol_;
    state.figi = active_figi_;
    state.statement = std::string(toSql(statement_));
    state.timeframe = std::string(toSql(timeframe_));
    return state;
}

void FinancialsPanel::setWindowScope(int runtime_id) noexcept
{
    runtime_id_ = runtime_id;
}

void FinancialsPanel::setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    place_force_ = force;
    place_floating_ = floating;
    place_dock_ = dock;
    place_pos_ = pos;
    place_size_ = size;
}

std::string FinancialsPanel::viewKey() const
{
    return active_symbol_ + "|" + std::string(toSql(statement_)) + "|" +
           std::string(toSql(timeframe_));
}

void FinancialsPanel::requestFetch(IngestWorker* ingest, bool force)
{
    if (ingest == nullptr || active_symbol_.empty())
    {
        return;
    }
    const std::string key = viewKey();
    if (!force && (busy_ || blocked_ || have_snapshot_ || key == failed_key_))
    {
        return;
    }
    if (!force && inflight_ && inflight_key_ == key)
    {
        return;
    }
    IngestWorker::Job job;
    job.symbol = active_symbol_;
    job.statements = true;
    job.statement = statement_;
    job.statement_timeframe = timeframe_;
    const IngestWorker::EnqueueResult result = ingest->enqueue(std::move(job));
    inflight_ = true;
    inflight_serial_ = result.serial;
    inflight_key_ = key;
    if (force)
    {
        failed_key_.clear();
        error_.clear();
    }
    status_ = std::string("fetching ") + active_symbol_ + " " + statementLabel(statement_) + " " +
              periodLabel(timeframe_);
}

void FinancialsPanel::refresh(Store* store, IngestWorker* ingest)
{
    if (ingest != nullptr && inflight_)
    {
        const IngestWorker::Snapshot snap = ingest->snapshot();
        if (snap.finished_serial >= inflight_serial_ && inflight_serial_ != 0)
        {
            const IngestWorker::SerialFailure failure = ingest->failureForSerial(inflight_serial_);
            inflight_ = false;
            if (failure.failed)
            {
                failed_key_ = inflight_key_;
                error_ = failure.message;
                status_ = failure.message;
            }
            else if (inflight_key_ == viewKey())
            {
                error_.clear();
                needs_reload_ = true;
            }
        }
    }

    const std::string key = viewKey();
    if (!needs_reload_ && key == loaded_key_)
    {
        if (fetch_now_)
        {
            requestFetch(ingest, true);
            fetch_now_ = false;
        }
        return;
    }

    needs_reload_ = false;
    busy_ = false;
    blocked_ = false;
    if (store == nullptr)
    {
        sheet_ = {};
        have_snapshot_ = false;
        loaded_key_ = key;
        fetch_now_ = false;
        status_ = "market data is unavailable";
        return;
    }
    if (active_symbol_.empty())
    {
        sheet_ = {};
        have_snapshot_ = false;
        loaded_key_ = key;
        error_.clear();
        status_ = "enter a symbol";
        fetch_now_ = false;
        return;
    }

    try
    {
        const std::optional<Instrument> found = resolveChartInstrument(*store, active_figi_, active_symbol_);
        if (found.has_value() && found->figi.has_value())
        {
            active_figi_ = *found->figi;
            if (found->listing_open && found->symbol != active_symbol_)
            {
                // Renamed since the book was saved: follow the security to its current ticker.
                active_symbol_ = found->symbol;
                std::snprintf(symbol_, sizeof(symbol_), "%s", active_symbol_.c_str());
                needs_reload_ = true;
                return;
            }
        }
        if (!found.has_value())
        {
            sheet_ = {};
            have_snapshot_ = false;
            loaded_key_ = key;
        }
        else
        {
            const std::optional<StatementSnapshot> snapshot =
                store->findStatementSnapshot(found->id, statement_, timeframe_);
            if (!snapshot.has_value())
            {
                sheet_ = {};
                have_snapshot_ = false;
                loaded_key_ = key;
            }
            else
            {
                const std::vector<StatementCell> cells =
                    store->queryStatementCells(found->id, statement_, timeframe_);
                sheet_ = buildStatementSheet(cells);
                have_snapshot_ = true;
                loaded_key_ = key;
                error_.clear();
                if (sheet_.rows.empty())
                {
                    status_ = active_symbol_ + "  " + statementLabel(statement_) + "  " +
                              periodLabel(timeframe_) + "  no rows";
                }
                else
                {
                    status_ = active_symbol_ + "  " + statementLabel(statement_) + "  " +
                              periodLabel(timeframe_) + "  " + std::to_string(sheet_.rows.size()) +
                              " lines";
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        if (isBusyError(ex.what()))
        {
            busy_ = true;
            if (loaded_key_ != key)
            {
                sheet_ = {};
                have_snapshot_ = false;
            }
            needs_reload_ = true;
            status_ = "database busy";
            // Keep the GO across a lock so the retry still enqueues.
            return;
        }
        sheet_ = {};
        have_snapshot_ = false;
        loaded_key_ = key;
        error_ = ex.what();
        status_ = error_;
        fetch_now_ = false;
        return;
    }

    if (!have_snapshot_ && error_.empty())
    {
        status_ = "not fetched";
    }
    if (fetch_now_)
    {
        requestFetch(ingest, true);
        fetch_now_ = false;
    }
    else if (!have_snapshot_ && !blocked_ && !busy_)
    {
        requestFetch(ingest, false);
    }
}

void FinancialsPanel::drawToolbar(IngestWorker* ingest)
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::kField);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Theme::kBg3);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Theme::kBg3);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("SYMBOL");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(88.0f);
    const bool symbol_go =
        ImGui::InputText("##fin_symbol", symbol_, sizeof(symbol_),
                         ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::TextUnformatted("STATEMENT");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::BeginCombo("##fin_statement", statementLabel(statement_)))
    {
        for (const StatementKind kind : kStatements)
        {
            if (ImGui::Selectable(statementLabel(kind), kind == statement_) && kind != statement_)
            {
                statement_ = kind;
                error_.clear();
                needs_reload_ = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("PERIOD");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::BeginCombo("##fin_period", periodLabel(timeframe_)))
    {
        for (const StatementTimeframe period : kPeriods)
        {
            if (ImGui::Selectable(periodLabel(period), period == timeframe_) && period != timeframe_)
            {
                timeframe_ = period;
                error_.clear();
                needs_reload_ = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::kGo);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::kAccentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::kAccentPressed);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kBg0);
    ImGui::BeginDisabled(ingest == nullptr);
    const bool clicked = ImGui::Button("GO");
    ImGui::EndDisabled();
    ImGui::PopStyleColor(4);

    if (symbol_go || clicked)
    {
        const std::string typed = normalizeChartSymbol(symbol_);
        if (typed != active_symbol_)
        {
            active_figi_.clear();
        }
        active_symbol_ = typed;
        std::snprintf(symbol_, sizeof(symbol_), "%s", active_symbol_.c_str());
        failed_key_.clear();
        error_.clear();
        fetch_now_ = true;
        needs_reload_ = true;
    }
}

void FinancialsPanel::drawSheet() const
{
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    constexpr int kColumns = 1 + kStatementSheetPeriods;
    ImFont* const mono = Theme::monoFont();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    if (!ImGui::BeginTable("financials_sheet", kColumns, flags, ImVec2(0.0f, 0.0f)))
    {
        if (mono != nullptr)
        {
            ImGui::PopFont();
        }
        return;
    }
    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide,
                            180.0f);
    std::array<std::string, kStatementSheetPeriods> headers;
    for (int index = 0; index < kStatementSheetPeriods; ++index)
    {
        if (index < sheet_.period_count)
        {
            headers[static_cast<std::size_t>(index)] = sheet_.periods[static_cast<std::size_t>(index)];
        }
        else
        {
            headers[static_cast<std::size_t>(index)] = "—";
        }
        ImGui::TableSetupColumn(headers[static_cast<std::size_t>(index)].c_str(),
                                ImGuiTableColumnFlags_WidthStretch);
    }
    ImGui::TableHeadersRow();

    if (sheet_.rows.empty())
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(Theme::kMuted, "%s",
                           active_symbol_.empty() ? "Enter a symbol and GO."
                                                  : "No values in the last four periods.");
        ImGui::EndTable();
        if (mono != nullptr)
        {
            ImGui::PopFont();
        }
        return;
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(sheet_.rows.size()));
    while (clipper.Step())
    {
        for (int row_index = clipper.DisplayStart; row_index < clipper.DisplayEnd; ++row_index)
        {
            const StatementSheetRow& row = sheet_.rows[static_cast<std::size_t>(row_index)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.line_item.c_str());
            for (int column = 0; column < kStatementSheetPeriods; ++column)
            {
                ImGui::TableSetColumnIndex(column + 1);
                const StatementSheetSlot& slot = row.slots[static_cast<std::size_t>(column)];
                if (!slot.present || column >= sheet_.period_count)
                {
                    drawRight("—", Theme::kTextFaint);
                    continue;
                }
                const std::string text = formatStatementValue(slot.value);
                const bool negative = !text.empty() && text.front() == '-';
                drawRight(text.c_str(), negative ? Theme::kDown : Theme::kText);
            }
        }
    }
    ImGui::EndTable();
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }
}

bool FinancialsPanel::draw(Store* store, std::string_view store_error, IngestWorker* ingest)
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

    char title[160];
    if (active_symbol_.empty())
    {
        std::snprintf(title, sizeof(title), "FINANCIALS %d###cb%d_financials%d", id_, runtime_id_, id_);
    }
    else
    {
        std::snprintf(title, sizeof(title), "%s  %s  %s###cb%d_financials%d", active_symbol_.c_str(),
                      statementLabel(statement_), periodLabel(timeframe_), runtime_id_, id_);
    }

    if (!ImGui::Begin(title, &window_open_, ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return false;
    }
    if (store == nullptr && !store_error.empty())
    {
        ImGui::TextColored(Theme::kDown, "%s", std::string(store_error).c_str());
    }

    drawToolbar(ingest);
    refresh(store, ingest);
    ImGui::Separator();
    const bool fetching = inflight_ && inflight_key_ == viewKey();
    ImVec4 status_color = Theme::kMuted;
    if (fetching)
    {
        status_color = Theme::kAccent;
    }
    else if (!error_.empty())
    {
        status_color = Theme::kDown;
    }
    const std::string shown =
        fetching ? std::string("fetching ") + active_symbol_ + " " + statementLabel(statement_) + " " +
                       periodLabel(timeframe_)
                 : status_;
    ImGui::TextColored(status_color, "%s", shown.c_str());

    if (ImGui::BeginChild("financials_body", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
    {
        drawSheet();
    }
    ImGui::EndChild();
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::End();
    return focused;
}

}  // namespace terminal
