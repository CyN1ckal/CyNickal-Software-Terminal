// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/OptionsChainPanel.h"

#include "chart/CChartLoad.h"
#include "data/IngestWorker.h"
#include "ui/Theme.h"

#include "market_data/Store.h"
#include "market_data/Time.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>
#include <utility>

namespace terminal {
namespace {

constexpr int kChainColumns = 15;

[[nodiscard]] bool isBusyError(std::string_view what) noexcept
{
    return what.find("busy") != std::string_view::npos || what.find("locked") != std::string_view::npos;
}

[[nodiscard]] std::string formatGrouped(std::int64_t value)
{
    const bool negative = value < 0;
    std::string digits = std::to_string(negative ? -value : value);
    std::string out;
    if (negative)
    {
        out.push_back('-');
    }
    const int lead = static_cast<int>(digits.size() % 3);
    for (int index = 0; std::cmp_less(index, digits.size()); ++index)
    {
        if (index > 0 && (index - (lead == 0 ? 3 : lead)) % 3 == 0)
        {
            out.push_back(',');
        }
        out.push_back(digits[static_cast<std::size_t>(index)]);
    }
    return out;
}

[[nodiscard]] std::string formatFetched(UnixSeconds ts)
{
    std::tm parts{};
    if (!tryUtcTm(static_cast<std::time_t>(ts), parts))
    {
        return {};
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d UTC", parts.tm_year + 1900, parts.tm_mon + 1,
                  parts.tm_mday, parts.tm_hour, parts.tm_min);
    return buf;
}

[[nodiscard]] std::string expiryLabel(const OptionExpiry& expiry)
{
    return formatSessionDate(expiry.expiration) + " " + std::string(toSql(expiry.expiration_type));
}

void drawAligned(const char* text, const ImVec4* color)
{
    const float width = ImGui::GetContentRegionAvail().x;
    const float text_w = ImGui::CalcTextSize(text).x;
    if (text_w < width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width - text_w);
    }
    if (color != nullptr)
    {
        ImGui::TextColored(*color, "%s", text);
        return;
    }
    ImGui::TextUnformatted(text);
}

void drawPrice(const OptionQuote* quote, double value, bool color_change, bool itm)
{
    if (itm)
    {
        const ImVec4 wash = Theme::WithAlpha(Theme::kAccent, 0.16f);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(wash));
    }
    if (quote == nullptr)
    {
        return;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", value);
    const ImVec4* color = nullptr;
    ImVec4 tint{};
    if (color_change && quote->price_change > 0.0)
    {
        tint = Theme::kUp;
        color = &tint;
    }
    else if (color_change && quote->price_change < 0.0)
    {
        tint = Theme::kDown;
        color = &tint;
    }
    drawAligned(buf, color);
}

void drawPercent(const OptionQuote* quote, double fraction, bool itm)
{
    if (itm)
    {
        const ImVec4 wash = Theme::WithAlpha(Theme::kAccent, 0.16f);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(wash));
    }
    if (quote == nullptr)
    {
        return;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", fraction * 100.0);
    drawAligned(buf, nullptr);
}

void drawGreek(const OptionQuote* quote, double value, bool itm)
{
    if (itm)
    {
        const ImVec4 wash = Theme::WithAlpha(Theme::kAccent, 0.16f);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(wash));
    }
    if (quote == nullptr)
    {
        return;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", value);
    drawAligned(buf, nullptr);
}

void drawCount(const OptionQuote* quote, std::int64_t value, bool itm)
{
    if (itm)
    {
        const ImVec4 wash = Theme::WithAlpha(Theme::kAccent, 0.16f);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(wash));
    }
    if (quote == nullptr)
    {
        return;
    }
    const std::string text = formatGrouped(value);
    drawAligned(text.c_str(), nullptr);
}

void drawTip(const OptionQuote& quote)
{
    if (!ImGui::IsItemHovered())
    {
        return;
    }
    std::string when = "no trade";
    if (quote.trade_minute.has_value())
    {
        char clock[16];
        std::snprintf(clock, sizeof(clock), "%02d:%02d ET", *quote.trade_minute / 60, *quote.trade_minute % 60);
        when = clock;
    }
    else if (quote.trade_date.has_value())
    {
        when = formatSessionDate(*quote.trade_date);
    }
    ImGui::BeginTooltip();
    ImGui::Text("mid %.2f   chg %+.2f   %+.2f%%", quote.mid, quote.price_change, quote.percent_change * 100.0);
    ImGui::Text("theta %.4f   vega %.4f   rho %.4f", quote.theta, quote.vega, quote.rho);
    ImGui::Text("oi chg %s   %s   %d dte", formatGrouped(quote.open_interest_change).c_str(), when.c_str(),
                quote.days_to_expiration);
    ImGui::EndTooltip();
}

struct ChainRow
{
    double strike{};
    const OptionQuote* call{nullptr};
    const OptionQuote* put{nullptr};
};

}  // namespace

OptionsChainPanel::OptionsChainPanel(int id) : id_(id) {}

int OptionsChainPanel::id() const noexcept
{
    return id_;
}

bool OptionsChainPanel::windowOpen() const noexcept
{
    return window_open_;
}

void OptionsChainPanel::closeWindow()
{
    window_open_ = false;
}

void OptionsChainPanel::requestFocus()
{
    focus_on_appear_ = true;
}

void OptionsChainPanel::importState(const ChartbookOptions& state)
{
    active_symbol_ = normalizeChartSymbol(state.symbol);
    std::snprintf(symbol_, sizeof(symbol_), "%s", active_symbol_.c_str());
    expiration_ = 0;
    has_expiration_ = false;
    expiration_type_ = OptionExpirationType::Weekly;
    if (state.expiration != 0 && (state.expiration_type == "weekly" || state.expiration_type == "monthly"))
    {
        expiration_ = state.expiration;
        expiration_type_ = optionExpirationTypeFromSql(state.expiration_type);
        has_expiration_ = true;
    }
    expiries_.clear();
    quotes_.clear();
    underlying_.reset();
    loaded_key_.clear();
    failed_key_.clear();
    inflight_key_.clear();
    inflight_serial_ = 0;
    inflight_ = false;
    have_slice_ = false;
    busy_ = false;
    blocked_ = false;
    fetch_now_ = false;
    needs_reload_ = true;
    error_.clear();
    status_ = active_symbol_.empty() ? "enter a symbol" : "not fetched";
}

ChartbookOptions OptionsChainPanel::exportState() const
{
    ChartbookOptions state;
    state.id = id_;
    state.symbol = active_symbol_;
    state.expiration = has_expiration_ ? expiration_ : 0;
    state.expiration_type = has_expiration_ ? std::string(toSql(expiration_type_)) : std::string{};
    return state;
}

void OptionsChainPanel::setWindowScope(int runtime_id) noexcept
{
    runtime_id_ = runtime_id;
}

void OptionsChainPanel::setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    place_force_ = force;
    place_floating_ = floating;
    place_dock_ = dock;
    place_pos_ = pos;
    place_size_ = size;
}

std::string OptionsChainPanel::viewKey() const
{
    std::string key = active_symbol_;
    key.push_back('|');
    if (!has_expiration_)
    {
        key += "none";
        return key;
    }
    key += formatSessionDate(expiration_);
    key.push_back('|');
    key += toSql(expiration_type_);
    return key;
}

const OptionExpiry* OptionsChainPanel::selectedExpiry() const
{
    if (!has_expiration_)
    {
        return nullptr;
    }
    for (const OptionExpiry& expiry : expiries_)
    {
        if (expiry.expiration == expiration_ && expiry.expiration_type == expiration_type_)
        {
            return &expiry;
        }
    }
    return nullptr;
}

void OptionsChainPanel::requestFetch(IngestWorker* ingest, bool force)
{
    if (ingest == nullptr || active_symbol_.empty() || blocked_)
    {
        return;
    }
    const std::string key = viewKey();
    if (!force && (busy_ || have_slice_ || key == failed_key_ || (inflight_ && inflight_key_ == key)))
    {
        return;
    }
    IngestWorker::Job job;
    job.symbol = active_symbol_;
    job.options = true;
    job.option_expiration = has_expiration_ ? expiration_ : 0;
    const IngestWorker::EnqueueResult result = ingest->enqueue(std::move(job));
    inflight_ = true;
    inflight_serial_ = result.serial;
    inflight_key_ = key;
    if (force)
    {
        failed_key_.clear();
        error_.clear();
    }
    status_ = std::string("fetching ") + active_symbol_;
    if (has_expiration_)
    {
        status_ += ' ';
        status_ += formatSessionDate(expiration_);
    }
}

void OptionsChainPanel::refresh(Store* store, IngestWorker* ingest)
{
    bool just_finished = false;
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
                just_finished = true;
            }
        }
    }

    if (!needs_reload_ && viewKey() == loaded_key_)
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
        quotes_.clear();
        expiries_.clear();
        underlying_.reset();
        have_slice_ = false;
        loaded_key_ = viewKey();
        fetch_now_ = false;
        status_ = "market data is unavailable";
        return;
    }
    if (active_symbol_.empty())
    {
        quotes_.clear();
        expiries_.clear();
        underlying_.reset();
        have_slice_ = false;
        loaded_key_ = viewKey();
        error_.clear();
        status_ = "enter a symbol";
        fetch_now_ = false;
        return;
    }

    try
    {
        std::vector<Instrument> found = store->findInstrumentsBySymbol(active_symbol_);
        if (found.empty() && active_symbol_.front() != '$')
        {
            found = store->findInstrumentsBySymbol("$" + active_symbol_);
            if (found.size() == 1)
            {
                active_symbol_ = found.front().symbol;
                std::snprintf(symbol_, sizeof(symbol_), "%s", active_symbol_.c_str());
            }
        }
        if (found.size() > 1)
        {
            quotes_.clear();
            expiries_.clear();
            underlying_.reset();
            have_slice_ = false;
            blocked_ = true;
            loaded_key_ = viewKey();
            error_ = "multiple instruments named " + active_symbol_;
            status_ = error_;
            fetch_now_ = false;
            return;
        }
        if (found.empty())
        {
            quotes_.clear();
            expiries_.clear();
            underlying_.reset();
            have_slice_ = false;
            loaded_key_ = viewKey();
        }
        else
        {
            const InstrumentId id = found.front().id;
            expiries_ = store->queryOptionExpiries(id);
            underlying_ = store->findOptionUnderlying(id);
            if (!has_expiration_)
            {
                const OptionExpiry* best = nullptr;
                UnixSeconds best_fetched = 0;
                OptionExpirationType best_type = OptionExpirationType::Weekly;
                bool have_best = false;
                for (const OptionExpiry& expiry : expiries_)
                {
                    if (!expiry.fetched_at.has_value())
                    {
                        continue;
                    }
                    const UnixSeconds fetched = *expiry.fetched_at;
                    const bool newer = !have_best || fetched > best_fetched;
                    const bool monthly_tie = have_best && fetched == best_fetched &&
                                             expiry.expiration_type == OptionExpirationType::Monthly &&
                                             best_type != OptionExpirationType::Monthly;
                    if (newer || monthly_tie)
                    {
                        best = &expiry;
                        best_fetched = fetched;
                        best_type = expiry.expiration_type;
                        have_best = true;
                    }
                }
                if (best != nullptr)
                {
                    expiration_ = best->expiration;
                    expiration_type_ = best->expiration_type;
                    has_expiration_ = true;
                }
            }
            const OptionExpiry* selected = selectedExpiry();
            have_slice_ = selected != nullptr && selected->fetched_at.has_value();
            quotes_.clear();
            if (have_slice_)
            {
                quotes_ = store->queryOptionQuotes(id, expiration_, expiration_type_);
            }
            loaded_key_ = viewKey();
            error_.clear();
            if (!have_slice_)
            {
                status_ = "not fetched";
            }
            else
            {
                status_ = active_symbol_ + "  " + formatSessionDate(expiration_) + " " +
                          std::string(toSql(expiration_type_)) + "  " + std::to_string(quotes_.size()) +
                          " contracts";
                if (selected->fetched_at.has_value())
                {
                    status_ += "  ";
                    status_ += formatFetched(*selected->fetched_at);
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        if (isBusyError(ex.what()))
        {
            busy_ = true;
            if (loaded_key_ != viewKey())
            {
                quotes_.clear();
                have_slice_ = false;
            }
            needs_reload_ = true;
            status_ = "database busy";
            return;
        }
        quotes_.clear();
        expiries_.clear();
        underlying_.reset();
        have_slice_ = false;
        loaded_key_ = viewKey();
        error_ = ex.what();
        status_ = error_;
        fetch_now_ = false;
        return;
    }

    if (just_finished && !have_slice_)
    {
        failed_key_ = viewKey();
        status_ = expiries_.empty() ? "no option chain" : "no contracts for that expiration";
    }
    if (fetch_now_)
    {
        requestFetch(ingest, true);
        fetch_now_ = false;
    }
    else if (!have_slice_ && !just_finished && !blocked_ && !busy_ && error_.empty())
    {
        requestFetch(ingest, false);
    }
}

void OptionsChainPanel::drawToolbar(IngestWorker* ingest)
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::kField);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Theme::kBg3);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Theme::kBg3);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("SYMBOL");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(96.0f);
    const bool symbol_go =
        ImGui::InputText("##opt_symbol", symbol_, sizeof(symbol_),
                         ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::TextUnformatted("EXPIRATION");
    ImGui::SameLine();
    const OptionExpiry* selected = selectedExpiry();
    const char* current = selected != nullptr ? nullptr : "select";
    std::string current_label;
    if (selected != nullptr)
    {
        current_label = expiryLabel(*selected);
        current = current_label.c_str();
    }
    else if (has_expiration_)
    {
        current_label = formatSessionDate(expiration_) + " " + std::string(toSql(expiration_type_));
        current = current_label.c_str();
    }
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("##opt_expiration", current))
    {
        for (const OptionExpiry& expiry : expiries_)
        {
            const std::string label = expiryLabel(expiry);
            const bool chosen = has_expiration_ && expiry.expiration == expiration_ &&
                                expiry.expiration_type == expiration_type_;
            if (ImGui::Selectable(label.c_str(), chosen) && !chosen)
            {
                expiration_ = expiry.expiration;
                expiration_type_ = expiry.expiration_type;
                has_expiration_ = true;
                error_.clear();
                failed_key_.clear();
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
        const std::string next = normalizeChartSymbol(symbol_);
        if (next != active_symbol_)
        {
            has_expiration_ = false;
            expiration_ = 0;
            expiries_.clear();
            quotes_.clear();
            underlying_.reset();
            have_slice_ = false;
        }
        active_symbol_ = next;
        std::snprintf(symbol_, sizeof(symbol_), "%s", active_symbol_.c_str());
        failed_key_.clear();
        error_.clear();
        fetch_now_ = true;
        needs_reload_ = true;
    }
}

void OptionsChainPanel::drawChain() const
{
    if (underlying_.has_value() || (selectedExpiry() != nullptr && selectedExpiry()->average_iv.has_value()))
    {
        std::string facts;
        if (underlying_.has_value() && underlying_->historic_vol_30d.has_value())
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "HV30 %.1f%%", *underlying_->historic_vol_30d * 100.0);
            facts += buf;
        }
        if (underlying_.has_value() && underlying_->iv_rank_1y.has_value())
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "   IV rank %.1f%%", *underlying_->iv_rank_1y * 100.0);
            facts += buf;
        }
        if (const OptionExpiry* expiry = selectedExpiry(); expiry != nullptr && expiry->average_iv.has_value())
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "   ATM %.1f%%", *expiry->average_iv * 100.0);
            facts += buf;
        }
        if (underlying_.has_value() && underlying_->next_earnings.has_value())
        {
            facts += "   earnings ";
            facts += formatSessionDate(*underlying_->next_earnings);
        }
        if (underlying_.has_value() && underlying_->dividend_ex.has_value())
        {
            facts += "   ex-div ";
            facts += formatSessionDate(*underlying_->dividend_ex);
        }
        if (!facts.empty())
        {
            ImGui::TextColored(Theme::kMuted, "%s", facts.c_str());
        }
    }

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable("options_chain", kChainColumns, flags, ImVec2(0.0f, 0.0f)))
    {
        return;
    }
    constexpr const char* kHeaders[] = {"C Bid", "C Ask", "C Last", "C IV",  "C Delta", "C Vol", "C OI", "Strike",
                                        "P OI",  "P Vol", "P Delta", "P IV", "P Last",  "P Ask", "P Bid",};
    for (const char* header : kHeaders)
    {
        ImGui::TableSetupColumn(header);
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    std::vector<ChainRow> rows;
    for (const OptionQuote& quote : quotes_)
    {
        if (rows.empty() || std::fabs(rows.back().strike - quote.strike) > 0.0001)
        {
            ChainRow row;
            row.strike = quote.strike;
            rows.push_back(row);
        }
        if (quote.right == OptionRight::Call)
        {
            rows.back().call = &quote;
        }
        else
        {
            rows.back().put = &quote;
        }
    }

    ImFont* const mono = Theme::monoFont();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    for (const ChainRow& row : rows)
    {
        ImGui::TableNextRow();
        const bool call_itm = row.call != nullptr && row.call->moneyness > 0.0;
        const bool put_itm = row.put != nullptr && row.put->moneyness < 0.0;
        ImGui::TableNextColumn();
        drawPrice(row.call, row.call != nullptr ? row.call->bid : 0.0, false, call_itm);
        ImGui::TableNextColumn();
        drawPrice(row.call, row.call != nullptr ? row.call->ask : 0.0, false, call_itm);
        ImGui::TableNextColumn();
        drawPrice(row.call, row.call != nullptr ? row.call->last : 0.0, true, call_itm);
        if (row.call != nullptr)
        {
            drawTip(*row.call);
        }
        ImGui::TableNextColumn();
        drawPercent(row.call, row.call != nullptr ? row.call->implied_vol : 0.0, call_itm);
        ImGui::TableNextColumn();
        drawGreek(row.call, row.call != nullptr ? row.call->delta : 0.0, call_itm);
        ImGui::TableNextColumn();
        drawCount(row.call, row.call != nullptr ? row.call->volume : 0, call_itm);
        ImGui::TableNextColumn();
        drawCount(row.call, row.call != nullptr ? row.call->open_interest : 0, call_itm);
        ImGui::TableNextColumn();
        char strike[32];
        std::snprintf(strike, sizeof(strike), "%.2f", row.strike);
        drawAligned(strike, nullptr);
        ImGui::TableNextColumn();
        drawCount(row.put, row.put != nullptr ? row.put->open_interest : 0, put_itm);
        ImGui::TableNextColumn();
        drawCount(row.put, row.put != nullptr ? row.put->volume : 0, put_itm);
        ImGui::TableNextColumn();
        drawGreek(row.put, row.put != nullptr ? row.put->delta : 0.0, put_itm);
        ImGui::TableNextColumn();
        drawPercent(row.put, row.put != nullptr ? row.put->implied_vol : 0.0, put_itm);
        ImGui::TableNextColumn();
        drawPrice(row.put, row.put != nullptr ? row.put->last : 0.0, true, put_itm);
        if (row.put != nullptr)
        {
            drawTip(*row.put);
        }
        ImGui::TableNextColumn();
        drawPrice(row.put, row.put != nullptr ? row.put->ask : 0.0, false, put_itm);
        ImGui::TableNextColumn();
        drawPrice(row.put, row.put != nullptr ? row.put->bid : 0.0, false, put_itm);
    }
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }
    ImGui::EndTable();
}

bool OptionsChainPanel::draw(Store* store, std::string_view store_error, IngestWorker* ingest)
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
        std::snprintf(title, sizeof(title), "OPTIONS %d###cb%d_options%d", id_, runtime_id_, id_);
    }
    else if (!has_expiration_)
    {
        std::snprintf(title, sizeof(title), "%s###cb%d_options%d", active_symbol_.c_str(), runtime_id_, id_);
    }
    else
    {
        const std::string when = formatSessionDate(expiration_) + " " + std::string(toSql(expiration_type_));
        std::snprintf(title, sizeof(title), "%s  %s###cb%d_options%d", active_symbol_.c_str(), when.c_str(),
                      runtime_id_, id_);
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
    ImGui::TextColored(status_color, "%s", status_.c_str());

    if (ImGui::BeginChild("options_body", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
    {
        drawChain();
    }
    ImGui::EndChild();
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::End();
    return focused;
}

}  // namespace terminal
