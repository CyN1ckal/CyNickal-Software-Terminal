// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/OptionsChainPanel.h"

#include "ui/Theme.h"

#include "market_data/Time.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace terminal {
namespace {

// Declaration order matches kOptionChainColumns.
enum class ChainField : std::uint8_t
{
    Bid,
    Ask,
    Last,
    Change,
    Percent,
    Mid,
    Iv,
    Delta,
    Theta,
    Vega,
    Rho,
    Volume,
    OpenInterest,
    OpenInterestChange,
    Count,
};

static_assert(static_cast<int>(ChainField::Count) == kOptionChainColumnCount);

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

void paintInTheMoney(bool itm)
{
    if (!itm)
    {
        return;
    }
    const ImVec4 wash = Theme::WithAlpha(Theme::kAccent, 0.16f);
    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(wash));
}

void drawSigned(const char* text, double sign)
{
    const ImVec4* color = nullptr;
    ImVec4 tint{};
    if (sign > 0.0)
    {
        tint = Theme::kUp;
        color = &tint;
    }
    else if (sign < 0.0)
    {
        tint = Theme::kDown;
        color = &tint;
    }
    drawAligned(text, color);
}

void drawTip(const OptionQuote& quote);

[[nodiscard]] bool findChainField(std::string_view id, ChainField& field) noexcept
{
    for (int index = 0; index < kOptionChainColumnCount; ++index)
    {
        if (id == kOptionChainColumns[index].id)
        {
            field = static_cast<ChainField>(index);
            return true;
        }
    }
    return false;
}

void drawQuoteField(std::string_view id, const OptionQuote* quote, bool itm)
{
    paintInTheMoney(itm);
    ChainField field{};
    if (quote == nullptr || !findChainField(id, field))
    {
        return;
    }
    char buf[32];
    switch (field)
    {
    case ChainField::Bid:
        std::snprintf(buf, sizeof(buf), "%.2f", quote->bid);
        drawAligned(buf, nullptr);
        break;
    case ChainField::Ask:
        std::snprintf(buf, sizeof(buf), "%.2f", quote->ask);
        drawAligned(buf, nullptr);
        break;
    case ChainField::Last:
        std::snprintf(buf, sizeof(buf), "%.2f", quote->last);
        drawSigned(buf, quote->price_change);
        break;
    case ChainField::Change:
        std::snprintf(buf, sizeof(buf), "%+.2f", quote->price_change);
        drawSigned(buf, quote->price_change);
        break;
    case ChainField::Percent:
        std::snprintf(buf, sizeof(buf), "%+.1f", quote->percent_change * 100.0);
        drawSigned(buf, quote->percent_change);
        break;
    case ChainField::Mid:
        std::snprintf(buf, sizeof(buf), "%.2f", quote->mid);
        drawAligned(buf, nullptr);
        break;
    case ChainField::Iv:
        std::snprintf(buf, sizeof(buf), "%.1f", quote->implied_vol * 100.0);
        drawAligned(buf, nullptr);
        break;
    case ChainField::Delta:
        std::snprintf(buf, sizeof(buf), "%.3f", quote->delta);
        drawAligned(buf, nullptr);
        break;
    case ChainField::Theta:
        std::snprintf(buf, sizeof(buf), "%.4f", quote->theta);
        drawAligned(buf, nullptr);
        break;
    case ChainField::Vega:
        std::snprintf(buf, sizeof(buf), "%.4f", quote->vega);
        drawAligned(buf, nullptr);
        break;
    case ChainField::Rho:
        std::snprintf(buf, sizeof(buf), "%.4f", quote->rho);
        drawAligned(buf, nullptr);
        break;
    case ChainField::Volume:
        drawAligned(formatGrouped(quote->volume).c_str(), nullptr);
        break;
    case ChainField::OpenInterest:
        drawAligned(formatGrouped(quote->open_interest).c_str(), nullptr);
        break;
    case ChainField::OpenInterestChange:
        drawSigned(formatGrouped(quote->open_interest_change).c_str(),
                   static_cast<double>(quote->open_interest_change));
        break;
    case ChainField::Count:
        return;
    }
    drawTip(*quote);
}

void setupChainColumn(const char* side, std::string_view id)
{
    char header[32];
    std::snprintf(header, sizeof(header), "%s %s", side, optionChainColumnLabel(id));
    ImGui::TableSetupColumn(header);
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
    columns_.clear();
    for (const std::string& id : state.columns)
    {
        setOptionChainColumnVisible(columns_, id, true);
    }
}

ChartbookOptions OptionsChainPanel::exportState() const
{
    ChartbookOptions state;
    state.id = id_;
    state.symbol = source_.symbol();
    state.figi = source_.figi();
    state.expiration = source_.hasExpiration() ? source_.expiration() : 0;
    state.expiration_type = source_.hasExpiration() ? std::string(toSql(source_.expirationType())) : std::string{};
    state.columns = columns_;
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

void OptionsChainPanel::drawColumnMenu()
{
    constexpr float kMenuStride = 148.0f;
    ImGui::PushID(id_);
    if (ImGui::Button("COLUMNS"))
    {
        ImGui::OpenPopup("columns");
    }
    if (ImGui::BeginPopup("columns"))
    {
        ImGui::TextColored(Theme::kMuted, "Strike stays in the middle. Puts mirror these.");
        for (int index = 0; index < kOptionChainColumnCount; ++index)
        {
            if (index % 2 == 1)
            {
                ImGui::SameLine(kMenuStride);
            }
            const OptionChainColumn& column = kOptionChainColumns[index];
            const auto found = std::ranges::find_if(columns_, [&](const std::string& column_id) {
                return column_id == column.id;
            });
            const bool shown = found != columns_.end();
            bool next = shown;
            ImGui::PushID(column.id);
            if (ImGui::Checkbox(column.label, &next) && next != shown)
            {
                setOptionChainColumnVisible(columns_, column.id, next);
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        ImGui::BeginDisabled(optionChainColumnsAreDefault(columns_));
        if (ImGui::Button("Reset"))
        {
            columns_ = defaultOptionChainColumns();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    ImGui::PopID();
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

    const int side_count = static_cast<int>(columns_.size());
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable("options_chain", (side_count * 2) + 1, flags, ImVec2(0.0f, 0.0f)))
    {
        return;
    }
    for (const std::string& id : columns_)
    {
        setupChainColumn("C", id);
    }
    ImGui::TableSetupColumn("Strike");
    for (int index = side_count - 1; index >= 0; --index)
    {
        setupChainColumn("P", columns_[static_cast<std::size_t>(index)]);
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
        for (const std::string& id : columns_)
        {
            ImGui::TableNextColumn();
            drawQuoteField(id, row.call, call_itm);
        }
        ImGui::TableNextColumn();
        char strike[32];
        std::snprintf(strike, sizeof(strike), "%.2f", row.strike);
        drawAligned(strike, nullptr);
        for (int index = side_count - 1; index >= 0; --index)
        {
            ImGui::TableNextColumn();
            drawQuoteField(columns_[static_cast<std::size_t>(index)], row.put, put_itm);
        }
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
    ImGui::SameLine();
    drawColumnMenu();
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
