// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/OptionsChainPanel.h"

#include "ui/Theme.h"

#include "market_data/Time.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace terminal {
namespace {

constexpr int kChainColumns = 15;

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
    source_.restore(state.symbol, state.figi, state.expiration, state.expiration_type);
}

ChartbookOptions OptionsChainPanel::exportState() const
{
    ChartbookOptions state;
    state.id = id_;
    state.symbol = source_.symbol();
    state.figi = source_.figi();
    state.expiration = source_.hasExpiration() ? source_.expiration() : 0;
    state.expiration_type = source_.hasExpiration() ? std::string(toSql(source_.expirationType())) : std::string{};
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

void OptionsChainPanel::drawChain() const
{
    const std::optional<OptionUnderlying>& underlying = source_.underlying();
    const OptionExpiry* const selected_expiry = source_.selectedExpiry();
    if (underlying.has_value() || (selected_expiry != nullptr && selected_expiry->average_iv.has_value()))
    {
        std::string facts;
        if (underlying.has_value() && underlying->historic_vol_30d.has_value())
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "HV30 %.1f%%", *underlying->historic_vol_30d * 100.0);
            facts += buf;
        }
        if (underlying.has_value() && underlying->iv_rank_1y.has_value())
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "   IV rank %.1f%%", *underlying->iv_rank_1y * 100.0);
            facts += buf;
        }
        if (const OptionExpiry* expiry = selected_expiry; expiry != nullptr && expiry->average_iv.has_value())
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "   ATM %.1f%%", *expiry->average_iv * 100.0);
            facts += buf;
        }
        if (underlying.has_value() && underlying->next_earnings.has_value())
        {
            facts += "   earnings ";
            facts += formatSessionDate(*underlying->next_earnings);
        }
        if (underlying.has_value() && underlying->dividend_ex.has_value())
        {
            facts += "   ex-div ";
            facts += formatSessionDate(*underlying->dividend_ex);
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
    for (const OptionQuote& quote : source_.quotes())
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
    if (source_.symbol().empty())
    {
        std::snprintf(title, sizeof(title), "OPTIONS %d###cb%d_options%d", id_, runtime_id_, id_);
    }
    else if (!source_.hasExpiration())
    {
        std::snprintf(title, sizeof(title), "%s###cb%d_options%d", source_.symbol().c_str(), runtime_id_, id_);
    }
    else
    {
        const std::string when = source_.expirationLabel();
        std::snprintf(title, sizeof(title), "%s  %s###cb%d_options%d", source_.symbol().c_str(), when.c_str(),
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

    source_.drawPicker(ingest);
    source_.refresh(store, ingest);
    ImGui::Separator();
    ImVec4 status_color = Theme::kMuted;
    if (source_.fetching())
    {
        status_color = Theme::kAccent;
    }
    else if (source_.failed())
    {
        status_color = Theme::kDown;
    }
    ImGui::TextColored(status_color, "%s", source_.status().c_str());

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
