// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/LedgerPanel.h"

#include "IngestDefaults.h"
#include "data/IngestWorker.h"
#include "ui/ReceivedStamp.h"
#include "ui/Theme.h"
#include "ui/TradingFormat.h"
#include "ui/TradingWidgets.h"

#include "market_data/NyseCalendar.h"
#include "market_data/Store.h"
#include "market_data/Time.h"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace terminal {
namespace {

constexpr const char* kShareOrOption[] = {"shares", "option"};
constexpr const char* kSides[] = {"buy", "sell"};
constexpr const char* kExpirationTypes[] = {"weekly", "monthly"};
constexpr const char* kRights[] = {"call", "put"};
constexpr const char* kNoValue = "\xE2\x80\x94";  // em dash

constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY |
                                        ImGuiTableFlags_ScrollX | ImGuiTableFlags_Resizable |
                                        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;

[[nodiscard]] std::string trimmed(const char* text)
{
    std::string_view view(text);
    while (!view.empty() && (view.front() == ' ' || view.front() == '\t'))
    {
        view.remove_prefix(1);
    }
    while (!view.empty() && (view.back() == ' ' || view.back() == '\t'))
    {
        view.remove_suffix(1);
    }
    return std::string(view);
}

void copyInto(char* buffer, std::size_t size, std::string_view text)
{
    const std::size_t count = text.copy(buffer, size - 1);
    buffer[count] = '\0';
}

[[nodiscard]] std::string fillSymbol(const TradeFill& fill)
{
    std::string text = fill.symbol.value_or(std::string{});
    if (!fill.listing_open && !text.empty())
    {
        text += " closed";
    }
    return text;
}

[[nodiscard]] std::optional<PositionKey> keyOf(const TradeFill& fill)
{
    try
    {
        return positionKey(fill);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

void setupColumns(std::initializer_list<const char*> headers)
{
    for (const char* header : headers)
    {
        ImGui::TableSetupColumn(header);
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
}

void drawMono(const std::string& text)
{
    drawRightText(text, Theme::kText);
}

void drawDim(const char* text)
{
    ImGui::TextColored(Theme::kTextFaint, "%s", text);
}

[[nodiscard]] IngestWorker::Job dailyJob(std::string symbol)
{
    const SessionDate today = utcToSessionDate(kLedgerTimezone, nowUtc());
    const auto ymd = sessionDateToYmd(today);
    IngestWorker::Job job;
    job.symbol = std::move(symbol);
    job.from = toSessionDate(std::chrono::sys_days{ymd} - std::chrono::days{kIngestDefaultDailyDays});
    job.to = today;
    job.timeframe_s = kTimeframe1d;
    return job;
}

}  // namespace

LedgerPanel::LedgerPanel(int id) : id_(id) {}

int LedgerPanel::id() const noexcept
{
    return id_;
}

bool LedgerPanel::windowOpen() const noexcept
{
    return window_open_;
}

void LedgerPanel::closeWindow()
{
    window_open_ = false;
}

void LedgerPanel::requestFocus()
{
    focus_on_appear_ = true;
}

void LedgerPanel::importState(const ChartbookLedger& state)
{
    ledger_id_ = state.ledger_id;
    loaded_ = false;
    tab_ = Tab::Positions;
    for (int index = 0; index < kLedgerTabCount; ++index)
    {
        if (state.tab == kLedgerTabs[index])
        {
            tab_ = static_cast<Tab>(index);
        }
    }
    select_tab_ = true;
}

ChartbookLedger LedgerPanel::exportState() const
{
    ChartbookLedger state;
    state.id = id_;
    state.ledger_id = ledger_id_;
    state.tab = kLedgerTabs[static_cast<int>(tab_)];
    return state;
}

void LedgerPanel::setWindowScope(int runtime_id) noexcept
{
    runtime_id_ = runtime_id;
}

void LedgerPanel::setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    place_force_ = force;
    place_floating_ = floating;
    place_dock_ = dock;
    place_pos_ = pos;
    place_size_ = size;
}

void LedgerPanel::showLedger(LedgerId id)
{
    selectLedger(id);
}

bool LedgerPanel::editable() const noexcept
{
    return ledger_id_ != 0 && ledger_kind_ == LedgerKind::Manual;
}

void LedgerPanel::selectLedger(LedgerId id)
{
    ledger_id_ = id;
    loaded_ = false;
    error_.clear();
}

void LedgerPanel::clearLedger(std::string status)
{
    ledger_id_ = 0;
    ledger_name_.clear();
    ledger_kind_ = LedgerKind::Manual;
    fills_.clear();
    flows_.clear();
    book_ = {};
    deposits_ = 0.0;
    marks_.clear();
    marks_valid_ = false;
    received_at_.reset();
    loaded_ = false;
    status_ = std::move(status);
}

void LedgerPanel::reload(const Store& store)
{
    ledgers_ = store.listLedgers();
    if (ledger_id_ == 0)
    {
        return;
    }
    const auto found = std::ranges::find_if(ledgers_, [&](const Ledger& ledger) { return ledger.id == ledger_id_; });
    if (found == ledgers_.end())
    {
        clearLedger("ledger was deleted");
        return;
    }
    if (loaded_ && found->updated_at == loaded_updated_)
    {
        return;
    }
    fills_ = store.queryFills(ledger_id_);
    flows_ = store.queryCashFlows(ledger_id_);
    ledger_name_ = found->name;
    ledger_kind_ = found->kind;
    loaded_updated_ = found->updated_at;
    loaded_ = true;
    copyInto(rename_, sizeof(rename_), ledger_name_);
    recompute(store);
    if (error_.empty())
    {
        status_ = ledger_kind_ == LedgerKind::Backtest ? ledger_name_ + " (backtest, read-only)" : ledger_name_;
    }
}

void LedgerPanel::recompute(const Store& store)
{
    std::set<InstrumentId> shares;
    for (const TradeFill& fill : fills_)
    {
        if (fill.kind != TradeAssetKind::Option && fill.instrument_id.has_value())
        {
            shares.insert(*fill.instrument_id);
        }
    }
    std::vector<CorporateAction> actions;
    for (const InstrumentId id : shares)
    {
        const std::vector<CorporateAction> found =
            store.queryCorporateActions(id, 0, std::numeric_limits<UnixSeconds>::max());
        actions.insert(actions.end(), found.begin(), found.end());
    }
    deposits_ = 0.0;
    for (const LedgerCashFlow& flow : flows_)
    {
        deposits_ += flow.amount;
    }
    try
    {
        book_ = matchLots(fills_, actions, nowUtc());
    }
    catch (const std::exception& ex)
    {
        book_ = {};
        error_ = ex.what();
        status_ = error_;
    }
    marks_valid_ = false;
}

void LedgerPanel::refreshMarks(const Store& store)
{
    marks_.assign(book_.positions.size(), std::nullopt);
    std::optional<UnixSeconds> newest;
    for (std::size_t index = 0; index < book_.positions.size(); ++index)
    {
        const PositionKey& key = book_.positions[index].key;
        const std::optional<LedgerMark> mark =
            key.kind == TradeAssetKind::Option ? latestOptionMark(store, key) : latestShareMark(store, key.instrument_id);
        if (mark.has_value() && (!newest.has_value() || mark->received_at > *newest))
        {
            newest = mark->received_at;
        }
        marks_[index] = mark;
    }
    received_at_ = newest;
    marks_valid_ = true;
}

void LedgerPanel::requestData(Store* store, IngestWorker* ingest)
{
    if (store == nullptr || ingest == nullptr || ledger_id_ == 0)
    {
        return;
    }
    const SessionDate today = utcToSessionDate(kLedgerTimezone, nowUtc());
    const std::vector<IngestWorker::Job> jobs = ledgerFetchJobs(*store, book_.positions, fills_, today, false);
    if (jobs.empty())
    {
        return;
    }
    std::uint64_t serial = 0;
    for (const IngestWorker::Job& job : jobs)
    {
        serial = std::max(serial, ingest->enqueue(job).serial);
    }
    refresh_serial_ = serial;
    status_ = "fetching";
}

void LedgerPanel::createLedger(Store& store)
{
    const std::string name = trimmed(new_name_);
    LedgerId created = 0;
    if (!withWriter(&store, error_, [&](Store& writer) { created = writer.createLedger(name); }))
    {
        status_ = error_;
        return;
    }
    new_name_[0] = '\0';
    selectLedger(created);
}

void LedgerPanel::renameLedger(Store& store)
{
    if (!editable())
    {
        return;
    }
    const std::string name = trimmed(rename_);
    if (!withWriter(&store, error_, [&](Store& writer) { writer.renameLedger(ledger_id_, name); }))
    {
        status_ = error_;
        return;
    }
    loaded_ = false;
}

void LedgerPanel::deleteLedger(Store& store)
{
    if (ledger_id_ == 0)
    {
        return;
    }
    if (!withWriter(&store, error_, [&](Store& writer) { writer.deleteLedger(ledger_id_); }))
    {
        status_ = error_;
        return;
    }
    clearLedger("select a ledger");
}

void LedgerPanel::deleteFill(Store& store, TradeFillId fill_id)
{
    if (!withWriter(&store, error_, [&](Store& writer) { writer.deleteFill(ledger_id_, fill_id); }))
    {
        status_ = error_;
        return;
    }
    loaded_ = false;
}

void LedgerPanel::deleteCashFlow(Store& store, LedgerCashFlowId flow_id)
{
    if (!withWriter(&store, error_, [&](Store& writer) { writer.deleteCashFlow(ledger_id_, flow_id); }))
    {
        status_ = error_;
        return;
    }
    loaded_ = false;
}

bool LedgerPanel::appendFill(Store& store, const std::string& symbol, TradeFill fill)
{
    const std::optional<Instrument> instrument = store.findOpenListing(symbol);
    if (!instrument.has_value() || !instrument->figi.has_value())
    {
        return false;
    }
    if (fill.kind != TradeAssetKind::Option)
    {
        if (instrument->asset_class == AssetClass::Equity)
        {
            fill.kind = TradeAssetKind::Equity;
        }
        else if (instrument->asset_class == AssetClass::Etf)
        {
            fill.kind = TradeAssetKind::Etf;
        }
        else
        {
            error_ = symbol + " is " + std::string(toSql(instrument->asset_class)) + "; only its options trade";
            status_ = error_;
            return true;
        }
    }
    fill.figi = instrument->figi;
    if (!withWriter(&store, error_, [&](Store& writer) {
            (void)writer.appendFills(ledger_id_, std::span<const TradeFill>(&fill, 1));
        }))
    {
        status_ = error_;
        return true;
    }
    quantity_[0] = '\0';
    price_[0] = '\0';
    fees_[0] = '\0';
    note_[0] = '\0';
    loaded_ = false;
    status_ = "added " + instrument->symbol;
    return true;
}

void LedgerPanel::pollPending(Store& store, IngestWorker* ingest)
{
    if (!pending_ || ingest == nullptr)
    {
        return;
    }
    if (pending_serial_ == 0 || ingest->snapshot().finished_serial < pending_serial_)
    {
        return;
    }
    pending_ = false;
    const IngestWorker::SerialFailure failure = ingest->failureForSerial(pending_serial_);
    if (failure.failed)
    {
        error_ = failure.message;
        status_ = error_;
        return;
    }
    if (!appendFill(store, pending_symbol_, pending_fill_))
    {
        error_ = pending_symbol_ + " has no open listing";
        status_ = error_;
    }
}

void LedgerPanel::drawHeader(Store& store)
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
                selectLedger(ledger.id);
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    bool ask_delete = false;
    ImGui::SameLine();
    if (ImGui::BeginMenu("Ledger"))
    {
        ImGui::SetNextItemWidth(160.f);
        ImGui::InputTextWithHint("##new", "new ledger name", new_name_, sizeof(new_name_));
        ImGui::SameLine();
        if (ImGui::Button("New"))
        {
            createLedger(store);
        }
        ImGui::BeginDisabled(!editable());
        ImGui::SetNextItemWidth(160.f);
        ImGui::InputText("##rename", rename_, sizeof(rename_));
        ImGui::SameLine();
        if (ImGui::Button("Rename"))
        {
            renameLedger(store);
        }
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::BeginDisabled(ledger_id_ == 0);
        if (ImGui::Button("Delete"))
        {
            ask_delete = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }
    if (ask_delete)
    {
        ImGui::OpenPopup("Delete ledger");
    }
    if (const ImGuiViewport* viewport = ImGui::GetMainViewport(); viewport != nullptr)
    {
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
    if (ImGui::BeginPopupModal("Delete ledger", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Delete this ledger?");
        ImGui::TextUnformatted(ledger_name_.c_str());
        ImGui::TextUnformatted(ledger_kind_ == LedgerKind::Backtest
                                   ? "Its fills, cash flows, and backtest run are removed."
                                   : "Its fills and cash flows are removed.");
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::kCancel);
        if (ImGui::Button("Delete"))
        {
            deleteLedger(store);
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
}

void LedgerPanel::drawSummary()
{
    double market_value = 0.0;
    double unrealized = 0.0;
    double open_fees = 0.0;
    for (std::size_t index = 0; index < book_.positions.size(); ++index)
    {
        const Position& position = book_.positions[index];
        const std::optional<LedgerMark>& mark = index < marks_.size() ? marks_[index] : std::nullopt;
        const double price = mark.has_value() ? mark->price : position.average_price;
        market_value += position.quantity * price * position.multiplier;
        unrealized += unrealizedPnl(position, price);
        open_fees += position.open_fees;
    }
    const double cash = deposits_ + book_.trade_cash;
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable("ledger_summary", 7, flags))
    {
        return;
    }
    setupColumns({"Deposits", "Cash", "Positions", "Equity", "Realized", "Unrealized", "Fees"});
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    drawMoneyCell(deposits_);
    ImGui::TableSetColumnIndex(1);
    drawMoneyCell(cash);
    ImGui::TableSetColumnIndex(2);
    drawMoneyCell(market_value);
    ImGui::TableSetColumnIndex(3);
    drawMoneyCell(cash + market_value);
    ImGui::TableSetColumnIndex(4);
    drawMoneyCell(book_.realized_pnl, true);
    ImGui::TableSetColumnIndex(5);
    drawMoneyCell(unrealized - open_fees, true);
    ImGui::TableSetColumnIndex(6);
    drawMoneyCell(book_.fees_paid);
    ImGui::EndTable();
}

void LedgerPanel::drawPositions()
{
    if (book_.positions.empty())
    {
        drawDim("No open positions.");
        return;
    }
    if (!ImGui::BeginTable("ledger_positions", 9, kTableFlags))
    {
        return;
    }
    setupColumns({"Symbol", "Contract", "Quantity", "Average", "Last", "Value", "Unrealized", "Cost", "Open fees"});
    for (std::size_t index = 0; index < book_.positions.size(); ++index)
    {
        const Position& position = book_.positions[index];
        const std::optional<LedgerMark>& mark = index < marks_.size() ? marks_[index] : std::nullopt;
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(position.symbol.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(contractLabel(position.key).c_str());
        ImGui::TableSetColumnIndex(2);
        drawRightText(formatQuantity(position.quantity), position.quantity < 0.0 ? Theme::kDown : Theme::kText);
        ImGui::TableSetColumnIndex(3);
        drawMono(formatPrice(position.average_price));
        ImGui::TableSetColumnIndex(4);
        if (mark.has_value())
        {
            drawMono(formatPrice(mark->price));
            ImGui::TableSetColumnIndex(5);
            drawMoneyCell(position.quantity * mark->price * position.multiplier);
            ImGui::TableSetColumnIndex(6);
            drawMoneyCell(unrealizedPnl(position, mark->price), true);
        }
        else
        {
            drawRightText(kNoValue, Theme::kTextFaint);
            ImGui::TableSetColumnIndex(5);
            drawRightText(kNoValue, Theme::kTextFaint);
            ImGui::TableSetColumnIndex(6);
            drawRightText(kNoValue, Theme::kTextFaint);
        }
        ImGui::TableSetColumnIndex(7);
        drawMoneyCell(position.cost_basis);
        ImGui::TableSetColumnIndex(8);
        drawMoneyCell(position.open_fees);
    }
    ImGui::EndTable();
}

void LedgerPanel::drawClosed()
{
    if (book_.round_trips.empty())
    {
        drawDim("No closed trades.");
        return;
    }
    if (!ImGui::BeginTable("ledger_closed", 11, kTableFlags))
    {
        return;
    }
    setupColumns({
        "Symbol",
        "Contract",
        "Opened",
        "Closed",
        "Held",
        "Quantity",
        "Entry",
        "Exit",
        "Gross",
        "Fees",
        "Net",
    });
    for (const RoundTrip& round_trip : std::views::reverse(book_.round_trips))
    {
        const RoundTrip* const trip = &round_trip;
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(trip->symbol.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(contractLabel(trip->key).c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(formatLedgerTime(trip->opened_at).c_str());
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(formatLedgerTime(trip->closed_at).c_str());
        ImGui::TableSetColumnIndex(4);
        drawMono(formatDuration(trip->closed_at - trip->opened_at));
        ImGui::TableSetColumnIndex(5);
        drawRightText(formatQuantity(trip->quantity), trip->quantity < 0.0 ? Theme::kDown : Theme::kText);
        ImGui::TableSetColumnIndex(6);
        drawMono(formatPrice(trip->entry_price));
        ImGui::TableSetColumnIndex(7);
        drawMono(formatPrice(trip->exit_price));
        ImGui::TableSetColumnIndex(8);
        drawMoneyCell(trip->gross_pnl, true);
        ImGui::TableSetColumnIndex(9);
        drawMoneyCell(trip->fees);
        ImGui::TableSetColumnIndex(10);
        drawMoneyCell(trip->net_pnl, true);
    }
    ImGui::EndTable();
}

void LedgerPanel::drawFills(Store& store)
{
    if (fills_.empty())
    {
        drawDim(editable() ? "No fills. Add one below." : "No fills.");
        return;
    }
    if (!ImGui::BeginTable("ledger_fills", 9, kTableFlags))
    {
        return;
    }
    setupColumns({"Time", "Symbol", "Contract", "Side", "Quantity", "Price", "Fees", "Cash", "Note"});
    std::optional<TradeFillId> doomed;
    for (const TradeFill& fill_row : std::views::reverse(fills_))
    {
        const TradeFill* const fill = &fill_row;
        ImGui::TableNextRow();
        ImGui::PushID(static_cast<int>(fill->id));
        ImGui::TableSetColumnIndex(0);
        const std::string when = formatLedgerTime(fill->ts);
        ImGui::Selectable(when.c_str(), false,
                          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
        if (editable() && ImGui::BeginPopupContextItem("fill_menu"))
        {
            if (ImGui::MenuItem("Delete fill"))
            {
                doomed = fill->id;
            }
            ImGui::EndPopup();
        }
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(fillSymbol(*fill).c_str());
        ImGui::TableSetColumnIndex(2);
        const std::optional<PositionKey> key = keyOf(*fill);
        ImGui::TextUnformatted(key.has_value() ? contractLabel(*key).c_str() : "");
        ImGui::TableSetColumnIndex(3);
        const bool buy = fill->quantity > 0.0;
        ImGui::TextColored(buy ? Theme::kUp : Theme::kDown, "%s", buy ? "Buy" : "Sell");
        ImGui::TableSetColumnIndex(4);
        drawMono(formatQuantity(std::abs(fill->quantity)));
        ImGui::TableSetColumnIndex(5);
        drawMono(formatPrice(fill->price));
        ImGui::TableSetColumnIndex(6);
        drawMoneyCell(fill->fees);
        ImGui::TableSetColumnIndex(7);
        drawMoneyCell(fillCashFlow(*fill));
        ImGui::TableSetColumnIndex(8);
        ImGui::TextUnformatted(fill->note.value_or(std::string{}).c_str());
        ImGui::PopID();
    }
    ImGui::EndTable();
    if (doomed.has_value())
    {
        deleteFill(store, *doomed);
    }
}

void LedgerPanel::drawCash(Store& store)
{
    if (flows_.empty())
    {
        drawDim(editable() ? "No cash flows. Add a deposit below." : "No cash flows.");
        return;
    }
    if (!ImGui::BeginTable("ledger_cash", 3, kTableFlags))
    {
        return;
    }
    setupColumns({"Time", "Amount", "Note"});
    std::optional<LedgerCashFlowId> doomed;
    for (const LedgerCashFlow& flow_row : std::views::reverse(flows_))
    {
        const LedgerCashFlow* const flow = &flow_row;
        ImGui::TableNextRow();
        ImGui::PushID(static_cast<int>(flow->id));
        ImGui::TableSetColumnIndex(0);
        const std::string when = formatLedgerTime(flow->ts);
        ImGui::Selectable(when.c_str(), false,
                          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
        if (editable() && ImGui::BeginPopupContextItem("cash_menu"))
        {
            if (ImGui::MenuItem("Delete cash flow"))
            {
                doomed = flow->id;
            }
            ImGui::EndPopup();
        }
        ImGui::TableSetColumnIndex(1);
        drawMoneyCell(flow->amount, true);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(flow->note.value_or(std::string{}).c_str());
        ImGui::PopID();
    }
    ImGui::EndTable();
    if (doomed.has_value())
    {
        deleteCashFlow(store, *doomed);
    }
}

void LedgerPanel::drawAddFill(Store& store, IngestWorker* ingest)
{
    ImGui::SetNextItemWidth(80.f);
    ImGui::InputTextWithHint("##symbol", "symbol", symbol_, sizeof(symbol_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.f);
    ImGui::Combo("##kind", &kind_, kShareOrOption, 2);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.f);
    ImGui::Combo("##side", &side_, kSides, 2);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.f);
    ImGui::InputTextWithHint("##quantity", "quantity", quantity_, sizeof(quantity_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.f);
    ImGui::InputTextWithHint("##price", "price", price_, sizeof(price_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.f);
    ImGui::InputTextWithHint("##fees", "fees", fees_, sizeof(fees_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.f);
    ImGui::InputTextWithHint("##when", "YYYY-MM-DD HH:MM", when_, sizeof(when_));
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("New York time. A date alone is the 16:00 close. Empty is now.");
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    ImGui::InputTextWithHint("##note", "note", note_, sizeof(note_));
    ImGui::SameLine();
    ImGui::BeginDisabled(pending_);
    const bool add = primaryButton("Add Fill");
    ImGui::EndDisabled();
    const bool option = kind_ == 1;
    if (option)
    {
        ImGui::SetNextItemWidth(90.f);
        ImGui::InputTextWithHint("##expiration", "YYYYMMDD", expiration_, sizeof(expiration_));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.f);
        ImGui::Combo("##expiration_type", &expiration_type_, kExpirationTypes, 2);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.f);
        ImGui::InputTextWithHint("##strike", "strike", strike_, sizeof(strike_));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.f);
        ImGui::Combo("##right", &right_, kRights, 2);
    }
    if (!add || pending_)
    {
        return;
    }

    const auto fail = [this](std::string message) {
        error_ = std::move(message);
        status_ = error_;
    };
    const std::string symbol = trimmed(symbol_);
    if (symbol.empty())
    {
        fail("symbol is empty");
        return;
    }
    const std::optional<double> quantity = parseLedgerNumber(quantity_);
    if (!quantity.has_value() || *quantity <= 0.0)
    {
        fail("quantity must be a positive number; the side sets the sign");
        return;
    }
    const std::optional<double> price = parseLedgerNumber(price_);
    if (!price.has_value() || *price < 0.0)
    {
        fail("price must be zero or more");
        return;
    }
    const std::string fees_text = trimmed(fees_);
    const std::optional<double> fees = fees_text.empty() ? std::optional<double>{0.0} : parseLedgerNumber(fees_text);
    if (!fees.has_value() || *fees < 0.0)
    {
        fail("fees must be zero or more");
        return;
    }
    const std::optional<UnixSeconds> when = parseLedgerTime(when_, nowUtc());
    if (!when.has_value())
    {
        fail("time must be YYYY-MM-DD or YYYY-MM-DD HH:MM");
        return;
    }

    TradeFill fill;
    fill.kind = option ? TradeAssetKind::Option : TradeAssetKind::Equity;
    fill.ts = *when;
    fill.quantity = side_ == 0 ? *quantity : -*quantity;
    fill.price = *price;
    fill.fees = *fees;
    const std::string note = trimmed(note_);
    if (!note.empty())
    {
        fill.note = note;
    }
    if (option)
    {
        SessionDate expiration = 0;
        try
        {
            expiration = parseSessionDate(trimmed(expiration_));
        }
        catch (const std::exception&)
        {
            fail("expiration must be YYYYMMDD");
            return;
        }
        const std::optional<double> strike = parseLedgerNumber(strike_);
        if (!strike.has_value() || *strike <= 0.0)
        {
            fail("strike must be positive");
            return;
        }
        fill.expiration = expiration;
        fill.expiration_type = expiration_type_ == 0 ? OptionExpirationType::Weekly : OptionExpirationType::Monthly;
        fill.strike = *strike;
        fill.right = right_ == 0 ? OptionRight::Call : OptionRight::Put;
    }
    error_.clear();
    if (appendFill(store, symbol, fill))
    {
        return;
    }
    if (ingest == nullptr)
    {
        fail(symbol + " has no open listing");
        return;
    }
    pending_ = true;
    pending_symbol_ = symbol;
    pending_fill_ = std::move(fill);
    pending_serial_ = ingest->enqueue(dailyJob(symbol)).serial;
    status_ = "fetching " + symbol;
}

void LedgerPanel::drawAddCash(Store& store)
{
    ImGui::SetNextItemWidth(110.f);
    ImGui::InputTextWithHint("##cash_amount", "amount (+/-)", cash_amount_, sizeof(cash_amount_));
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("A deposit is positive, a withdrawal negative.");
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.f);
    ImGui::InputTextWithHint("##cash_when", "YYYY-MM-DD HH:MM", cash_when_, sizeof(cash_when_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.f);
    ImGui::InputTextWithHint("##cash_note", "note", cash_note_, sizeof(cash_note_));
    ImGui::SameLine();
    if (!primaryButton("Add Cash"))
    {
        return;
    }
    const std::optional<double> amount = parseLedgerNumber(cash_amount_);
    const std::optional<UnixSeconds> when = parseLedgerTime(cash_when_, nowUtc());
    if (!amount.has_value() || *amount == 0.0)
    {
        error_ = "amount must be a non-zero number";
        status_ = error_;
        return;
    }
    if (!when.has_value())
    {
        error_ = "time must be YYYY-MM-DD or YYYY-MM-DD HH:MM";
        status_ = error_;
        return;
    }
    LedgerCashFlow flow;
    flow.ts = *when;
    flow.amount = *amount;
    const std::string note = trimmed(cash_note_);
    if (!note.empty())
    {
        flow.note = note;
    }
    if (!withWriter(&store, error_, [&](Store& writer) {
            (void)writer.appendCashFlows(ledger_id_, std::span<const LedgerCashFlow>(&flow, 1));
        }))
    {
        status_ = error_;
        return;
    }
    cash_amount_[0] = '\0';
    cash_note_[0] = '\0';
    loaded_ = false;
    status_ = "added cash flow";
}

bool LedgerPanel::draw(Store* store, std::string_view store_error, IngestWorker* ingest)
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

    const std::string title = (ledger_name_.empty() ? std::string("LEDGER") : ledger_name_) + "###cb" +
                              std::to_string(runtime_id_) + "_ledger" + std::to_string(id_);
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
        reload(*store);
    }
    catch (const std::exception& ex)
    {
        error_ = ex.what();
        status_ = error_;
    }
    pollPending(*store, ingest);
    if (ingest != nullptr)
    {
        const std::uint64_t finished = ingest->snapshot().finished_serial;
        if (finished != priced_serial_)
        {
            priced_serial_ = finished;
            marks_valid_ = false;
        }
        if (refresh_serial_ != 0 && finished >= refresh_serial_)
        {
            refresh_serial_ = 0;
            if (status_ == "fetching")
            {
                status_ = ledger_name_;
            }
        }
    }
    if (!marks_valid_ || marks_.size() != book_.positions.size())
    {
        try
        {
            refreshMarks(*store);
        }
        catch (const std::exception& ex)
        {
            marks_valid_ = true;
            error_ = ex.what();
            status_ = error_;
        }
    }

    drawHeader(*store);
    ImGui::Separator();
    ImVec4 status_color = Theme::kMuted;
    if (pending_ || refresh_serial_ != 0)
    {
        status_color = Theme::kAccent;
    }
    else if (!error_.empty())
    {
        status_color = Theme::kDown;
    }
    ImGui::TextColored(status_color, "%s", status_.c_str());
    drawReceivedStamp(received_at_);
    if (ledger_id_ == 0)
    {
        drawDim("Pick a ledger, or create one from the Ledger menu.");
        const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        ImGui::End();
        return focused;
    }

    drawSummary();
    if (ImGui::BeginTabBar("ledger_tabs"))
    {
        constexpr std::pair<Tab, const char*> kTabs[] = {
            {Tab::Positions, "Positions"},
            {Tab::Closed, "Closed"},
            {Tab::Fills, "Fills"},
            {Tab::Cash, "Cash"},
        };
        const Tab restore = tab_;
        for (const auto& [tab, label] : kTabs)
        {
            const ImGuiTabItemFlags flags =
                select_tab_ && tab == restore ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if (ImGui::BeginTabItem(label, nullptr, flags))
            {
                tab_ = tab;
                ImGui::EndTabItem();
            }
        }
        select_tab_ = false;
        ImGui::EndTabBar();
    }

    float footer = 0.f;
    if (editable() && tab_ != Tab::Closed)
    {
        footer = ImGui::GetFrameHeightWithSpacing();
        if (tab_ != Tab::Cash && kind_ == 1)
        {
            footer *= 2.f;
        }
    }
    if (ImGui::BeginChild("ledger_table", ImVec2(0.f, footer > 0.f ? -footer : 0.f), ImGuiChildFlags_Borders))
    {
        switch (tab_)
        {
        case Tab::Positions:
            drawPositions();
            break;
        case Tab::Closed:
            drawClosed();
            break;
        case Tab::Fills:
            drawFills(*store);
            break;
        case Tab::Cash:
            drawCash(*store);
            break;
        }
    }
    ImGui::EndChild();
    if (footer > 0.f)
    {
        if (tab_ == Tab::Cash)
        {
            drawAddCash(*store);
        }
        else
        {
            drawAddFill(*store, ingest);
        }
    }
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::End();
    return focused;
}

}  // namespace terminal
