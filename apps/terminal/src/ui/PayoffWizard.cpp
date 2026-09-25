// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/PayoffWizard.h"

#include "ui/Theme.h"

#include "market_data/Time.h"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

namespace terminal {
namespace {

constexpr std::size_t kNoTemplate = static_cast<std::size_t>(-1);
constexpr double kStrikeMatch = 1e-4;
constexpr const char* kBasisLabels[] = {"MID", "NATURAL", "LAST"};
constexpr const char* kInstrumentLabels[] = {"CALL", "PUT", "SHARES"};
// ImPlot splits fit padding across both ends, so 0.24 leaves 12% of the P&L range above and below.
constexpr float kPayoffFitPadding = 0.24f;

// "1,234.56". sign adds a leading + to a gain.
[[nodiscard]] std::string formatDollars(double value, bool sign)
{
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.2f", std::fabs(value));
    const std::string plain = buf;
    const std::size_t dot = plain.find('.');
    const std::string whole = plain.substr(0, dot);
    std::string out;
    if (value < 0.0 && plain != "0.00")
    {
        out.push_back('-');
    }
    else if (sign && value > 0.0 && plain != "0.00")
    {
        out.push_back('+');
    }
    for (std::size_t index = 0; index < whole.size(); ++index)
    {
        if (index > 0 && (whole.size() - index) % 3 == 0)
        {
            out.push_back(',');
        }
        out.push_back(whole[index]);
    }
    out += plain.substr(dot);
    return out;
}

[[nodiscard]] std::string formatStrike(double strike)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", strike);
    return buf;
}

[[nodiscard]] std::string contractLabel(const PayoffLeg& leg)
{
    switch (leg.instrument)
    {
    case LegInstrument::Call:
        return formatStrike(leg.strike) + " CALL";
    case LegInstrument::Put:
        return formatStrike(leg.strike) + " PUT";
    case LegInstrument::Underlying:
        return "SHARES";
    }
    return {};
}

[[nodiscard]] bool strikeListed(std::span<const double> strikes, double strike) noexcept
{
    return std::ranges::any_of(strikes, [strike](double listed) { return std::fabs(listed - strike) <= kStrikeMatch; });
}

// Lays label/widget groups left to right and wraps a group that would cross the edge.
class RowFlow
{
public:
    RowFlow() : right_(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x) {}

    void next(float width)
    {
        if (!first_ && ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= right_)
        {
            ImGui::SameLine();
        }
        first_ = false;
    }

    // A caption and the widget after it.
    void labelled(const char* label, float widget_width)
    {
        next(ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x + widget_width);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(widget_width);
    }

    // A widget with no caption.
    void bare(float widget_width)
    {
        next(widget_width);
        ImGui::SetNextItemWidth(widget_width);
    }

    // A button sized to its label.
    void button(const char* label)
    {
        next(ImGui::CalcTextSize(label).x + (ImGui::GetStyle().FramePadding.x * 2.0f));
    }

    // A caption and a colored value, both text.
    void fact(const char* label, const std::string& value, const ImVec4& color)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        next(ImGui::CalcTextSize(label).x + style.ItemSpacing.x + ImGui::CalcTextSize(value.c_str()).x);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Theme::kMuted, "%s", label);
        ImGui::SameLine();
        ImGui::TextColored(color, "%s", value.c_str());
    }

private:
    float right_{0.0f};
    bool first_{true};
};

[[nodiscard]] bool primaryButton(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::kGo);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::kAccentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::kAccentPressed);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kBg0);
    const bool clicked = ImGui::Button(label);
    ImGui::PopStyleColor(4);
    return clicked;
}

// A combo over the listed strikes. True when the pick changed.
[[nodiscard]] bool strikeCombo(const char* id, std::span<const double> strikes, std::size_t& pick)
{
    bool changed = false;
    const std::string preview = pick < strikes.size() ? formatStrike(strikes[pick]) : std::string("-");
    if (ImGui::BeginCombo(id, preview.c_str()))
    {
        for (std::size_t index = 0; index < strikes.size(); ++index)
        {
            const std::string label = formatStrike(strikes[index]);
            const bool chosen = index == pick;
            if (ImGui::Selectable(label.c_str(), chosen) && !chosen)
            {
                pick = index;
                changed = true;
            }
            if (chosen)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

void drawRightAligned(const std::string& text)
{
    const float width = ImGui::GetContentRegionAvail().x;
    const float text_w = ImGui::CalcTextSize(text.c_str()).x;
    if (text_w < width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width - text_w);
    }
    ImGui::TextUnformatted(text.c_str());
}

}  // namespace

void PayoffWizard::clear()
{
    const bool had_legs = !legs_.empty();
    legs_.clear();
    spot_manual_ = false;
    spot_ = 0.0;
    draft_ = LegDraft{};
    refit_ = true;
    if (had_legs)
    {
        setStatus("new underlying; legs cleared", false);
    }
    else
    {
        setStatus({}, false);
    }
}

void PayoffWizard::restore(std::vector<PayoffLeg> legs, double manual_spot)
{
    legs_.clear();
    if (!legs.empty() && !validateLegs(legs).has_value())
    {
        legs_ = std::move(legs);
    }
    spot_manual_ = std::isfinite(manual_spot) && manual_spot > 0.0;
    spot_ = spot_manual_ ? manual_spot : 0.0;
    refit_ = true;
}

const std::vector<PayoffLeg>& PayoffWizard::legs() const noexcept
{
    return legs_;
}

double PayoffWizard::manualSpot() const noexcept
{
    return spot_manual_ ? spot_ : 0.0;
}

void PayoffWizard::setStatus(std::string text, bool error)
{
    status_ = std::move(text);
    status_error_ = error;
}

void PayoffWizard::addLegs(std::span<const PayoffLeg> incoming, std::string_view what)
{
    const std::optional<std::int32_t> held = strategyExpiration(legs_);
    const std::optional<std::int32_t> adding = strategyExpiration(incoming);
    if (held.has_value() && adding.has_value() && *held != *adding)
    {
        const std::string held_text = *held == 0 ? std::string("no date") : formatSessionDate(*held);
        const std::string adding_text = *adding == 0 ? std::string("no date") : formatSessionDate(*adding);
        setStatus("legs expire " + held_text + ". CLEAR them to build on " + adding_text + ".", true);
        return;
    }
    if (std::optional<std::string> problem = appendLegs(legs_, incoming); problem.has_value())
    {
        setStatus(std::move(*problem), true);
        return;
    }
    refit_ = true;
    setStatus("added " + std::string(what), false);
}

void PayoffWizard::syncChain(std::span<const OptionQuote> chain)
{
    // A chain is a few hundred quotes, so rereading it each frame is cheaper than tracking reloads.
    if (std::vector<double> strikes = listedStrikes(chain); strikes != strikes_)
    {
        strikes_ = std::move(strikes);
        picks_for_template_ = kNoTemplate;
    }
    if (!spot_manual_)
    {
        if (const std::optional<double> implied = parityImpliedSpot(chain); implied.has_value())
        {
            spot_ = *implied;
        }
    }
    const std::optional<std::size_t> atm = nearestStrikeIndex(strikes_, spot_);
    const std::size_t atm_index = atm.value_or(strikes_.size() / 2);
    const std::span<const StrategyTemplate> recipes = strategyTemplates();
    template_index_ = std::min(template_index_, recipes.size() - 1);
    if (picks_for_template_ != template_index_)
    {
        slot_picks_ = defaultSlotStrikes(recipes[template_index_], strikes_.size(), atm_index);
        picks_for_template_ = template_index_;
    }
    if (!strikes_.empty() && !strikeListed(strikes_, draft_.strike))
    {
        draft_.strike = strikes_[atm_index];
        draft_.price_typed = false;
    }
    autoPriceDraft(chain);
}

void PayoffWizard::autoPriceDraft(std::span<const OptionQuote> chain)
{
    if (draft_.price_typed)
    {
        return;
    }
    if (draft_.instrument == LegInstrument::Underlying)
    {
        draft_.price = spot_;
        return;
    }
    const OptionRight right = draft_.instrument == LegInstrument::Put ? OptionRight::Put : OptionRight::Call;
    if (const OptionQuote* quote = findQuote(chain, right, draft_.strike); quote != nullptr)
    {
        draft_.price = quotePrice(*quote, basis_, draft_.buy);
    }
}

void PayoffWizard::drawRecipeRow(std::span<const OptionQuote> chain)
{
    const std::span<const StrategyTemplate> recipes = strategyTemplates();
    const StrategyTemplate& recipe = recipes[template_index_];
    RowFlow row;

    row.labelled("QTY", 48.0f);
    if (ImGui::InputInt("##payoff_qty", &quantity_, 0, 0))
    {
        quantity_ = std::clamp(quantity_, 1, 9999);
    }
    ImGui::SetItemTooltip("contracts per leg; a share leg is 100 shares per contract");

    row.labelled("PRICING", 90.0f);
    const auto basis_index = static_cast<std::size_t>(basis_);
    if (ImGui::BeginCombo("##payoff_basis", kBasisLabels[basis_index]))
    {
        for (std::size_t index = 0; index < std::size(kBasisLabels); ++index)
        {
            if (ImGui::Selectable(kBasisLabels[index], index == basis_index) && index != basis_index)
            {
                basis_ = static_cast<PriceBasis>(index);
                draft_.price_typed = false;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("MID trades at the middle; NATURAL buys the ask and sells the bid");

    row.labelled("STRATEGY", 150.0f);
    if (ImGui::BeginCombo("##payoff_template", recipe.name))
    {
        for (std::size_t index = 0; index < recipes.size(); ++index)
        {
            if (ImGui::Selectable(recipes[index].name, index == template_index_))
            {
                template_index_ = index;
            }
            ImGui::SetItemTooltip("%s", recipes[index].note);
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("%s", recipe.note);

    for (std::size_t slot = 0; slot < recipe.slots.size() && slot < slot_picks_.size(); ++slot)
    {
        ImGui::PushID(static_cast<int>(slot));
        row.labelled(recipe.slots[slot].label, 80.0f);
        (void)strikeCombo("##slot_strike", strikes_, slot_picks_[slot]);
        ImGui::PopID();
    }

    row.button("ADD STRATEGY");
    if (!primaryButton("ADD STRATEGY"))
    {
        return;
    }
    if (!recipe.slots.empty() && (strikes_.empty() || slot_picks_.size() != recipe.slots.size()))
    {
        setStatus(std::string(recipe.name) + " needs a chain; pick a symbol and expiration first", true);
        return;
    }
    std::vector<double> slot_strikes;
    slot_strikes.reserve(slot_picks_.size());
    for (const std::size_t pick : slot_picks_)
    {
        slot_strikes.push_back(strikes_[pick]);
    }
    TemplateInputs inputs;
    inputs.slot_strikes = slot_strikes;
    inputs.quantity = static_cast<double>(quantity_);
    inputs.basis = basis_;
    inputs.underlying_price = spot_;
    TemplateBuild build = buildTemplate(recipe, chain, inputs);
    if (!build.error.empty())
    {
        setStatus(std::move(build.error), true);
        return;
    }
    addLegs(build.legs, std::to_string(quantity_) + " " + recipe.name);
}

void PayoffWizard::drawLegRow(std::span<const OptionQuote> chain, SessionDate expiration)
{
    RowFlow row;

    row.labelled("LEG", 80.0f);
    const auto instrument_index = static_cast<std::size_t>(draft_.instrument);
    if (ImGui::BeginCombo("##leg_instrument", kInstrumentLabels[instrument_index]))
    {
        for (std::size_t index = 0; index < std::size(kInstrumentLabels); ++index)
        {
            if (ImGui::Selectable(kInstrumentLabels[index], index == instrument_index) && index != instrument_index)
            {
                draft_.instrument = static_cast<LegInstrument>(index);
                draft_.price_typed = false;
            }
        }
        ImGui::EndCombo();
    }

    row.bare(64.0f);
    if (ImGui::BeginCombo("##leg_side", draft_.buy ? "BUY" : "SELL"))
    {
        if (ImGui::Selectable("BUY", draft_.buy) && !draft_.buy)
        {
            draft_.buy = true;
            draft_.price_typed = false;
        }
        if (ImGui::Selectable("SELL", !draft_.buy) && draft_.buy)
        {
            draft_.buy = false;
            draft_.price_typed = false;
        }
        ImGui::EndCombo();
    }

    const bool option = draft_.instrument != LegInstrument::Underlying;
    if (option)
    {
        row.labelled("STRIKE", 80.0f);
        if (!strikes_.empty())
        {
            std::size_t pick = strikes_.size();
            for (std::size_t index = 0; index < strikes_.size(); ++index)
            {
                if (std::fabs(strikes_[index] - draft_.strike) <= kStrikeMatch)
                {
                    pick = index;
                }
            }
            if (strikeCombo("##leg_strike", strikes_, pick))
            {
                draft_.strike = strikes_[pick];
                draft_.price_typed = false;
            }
        }
        else if (ImGui::InputDouble("##leg_strike", &draft_.strike, 0.0, 0.0, "%.2f"))
        {
            draft_.strike = std::isfinite(draft_.strike) ? std::max(draft_.strike, 0.0) : 0.0;
        }
    }

    row.labelled("PRICE", 72.0f);
    if (ImGui::InputDouble("##leg_price", &draft_.price, 0.0, 0.0, "%.2f"))
    {
        draft_.price = std::isfinite(draft_.price) ? std::max(draft_.price, 0.0) : 0.0;
        draft_.price_typed = true;
    }
    ImGui::SetItemTooltip("%s", draft_.price_typed ? "typed by hand" : "from the chain at the PRICING basis");
    if (draft_.price_typed)
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("AUTO##leg_price"))
        {
            draft_.price_typed = false;
        }
    }

    row.button("ADD LEG");
    const bool add = primaryButton("ADD LEG");
    ImGui::SameLine();
    ImGui::BeginDisabled(legs_.empty());
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kCancel);
    if (ImGui::Button("CLEAR"))
    {
        legs_.clear();
        refit_ = true;
        setStatus("cleared", false);
    }
    ImGui::PopStyleColor();
    ImGui::EndDisabled();

    if (!add)
    {
        return;
    }
    const auto contracts = static_cast<double>(quantity_);
    const double signed_contracts = draft_.buy ? contracts : -contracts;
    PayoffLeg leg;
    if (!option)
    {
        if (!(draft_.price > 0.0))
        {
            setStatus("enter a price for the shares", true);
            return;
        }
        leg = underlyingLeg(signed_contracts * kContractMultiplier, draft_.price);
    }
    else
    {
        if (!(draft_.strike > 0.0))
        {
            setStatus("enter a strike", true);
            return;
        }
        leg.instrument = draft_.instrument;
        leg.strike = draft_.strike;
        leg.expiration = expiration;
        leg.quantity = signed_contracts;
        leg.price = draft_.price;
        leg.multiplier = kContractMultiplier;
        const OptionRight right = draft_.instrument == LegInstrument::Put ? OptionRight::Put : OptionRight::Call;
        if (const OptionQuote* quote = findQuote(chain, right, draft_.strike); quote != nullptr)
        {
            leg.expiration = quote->expiration;
        }
    }
    char what[96];
    std::snprintf(what, sizeof(what), "%s %d %s @ %.2f", draft_.buy ? "BUY" : "SELL", quantity_,
                  contractLabel(leg).c_str(), leg.price);
    addLegs(std::span<const PayoffLeg>(&leg, 1), what);
}

void PayoffWizard::drawLegTable()
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY |
                                      ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable("payoff_legs", 7, flags))
    {
        return;
    }
    ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.6f);
    ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthStretch, 0.8f);
    ImGui::TableSetupColumn("Contract", ImGuiTableColumnFlags_WidthStretch, 1.2f);
    ImGui::TableSetupColumn("Expires", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthStretch, 0.9f);
    ImGui::TableSetupColumn("Cost", ImGuiTableColumnFlags_WidthStretch, 1.1f);
    ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    std::optional<std::size_t> remove;
    ImFont* const mono = Theme::monoFont();
    for (std::size_t index = 0; index < legs_.size(); ++index)
    {
        PayoffLeg& leg = legs_[index];
        ImGui::PushID(static_cast<int>(index));
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(leg.quantity > 0.0 ? "BUY" : "SELL");

        ImGui::TableNextColumn();
        double quantity = leg.quantity;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputDouble("##qty", &quantity, 0.0, 0.0, "%.0f") && quantity != leg.quantity)
        {
            std::vector<PayoffLeg> edited = legs_;
            edited[index].quantity = quantity;
            if (std::optional<std::string> problem = validateLegs(edited); problem.has_value())
            {
                setStatus(std::move(*problem), true);
            }
            else
            {
                leg.quantity = quantity;
            }
        }
        ImGui::SetItemTooltip("%s", isOption(leg) ? "contracts; negative sells" : "shares; negative is short");

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(contractLabel(leg).c_str());

        ImGui::TableNextColumn();
        if (isOption(leg) && leg.expiration != 0)
        {
            ImGui::TextUnformatted(formatSessionDate(leg.expiration).c_str());
        }
        else
        {
            ImGui::TextColored(Theme::kMuted, "%s", isOption(leg) ? "no date" : "-");
        }

        ImGui::TableNextColumn();
        double price = leg.price;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputDouble("##price", &price, 0.0, 0.0, "%.2f") && std::isfinite(price))
        {
            leg.price = std::max(price, 0.0);
        }

        ImGui::TableNextColumn();
        if (mono != nullptr)
        {
            ImGui::PushFont(mono);
        }
        drawRightAligned(formatDollars(leg.quantity * leg.multiplier * leg.price, false));
        if (mono != nullptr)
        {
            ImGui::PopFont();
        }

        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::kCancel);
        if (ImGui::SmallButton("x"))
        {
            remove = index;
        }
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::EndTable();

    if (remove.has_value())
    {
        legs_.erase(legs_.begin() + static_cast<std::ptrdiff_t>(*remove));
        refit_ = true;
        setStatus("removed a leg", false);
    }
}

void PayoffWizard::drawSummary(const PayoffSummary& summary)
{
    RowFlow row;
    row.labelled("SPOT", 80.0f);
    if (ImGui::InputDouble("##payoff_spot", &spot_, 0.0, 0.0, "%.2f"))
    {
        spot_ = std::isfinite(spot_) ? std::max(spot_, 0.0) : 0.0;
        spot_manual_ = true;
    }
    ImGui::SetItemTooltip("%s", spot_manual_ ? "typed by hand" : "put-call parity at the money");
    if (spot_manual_)
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("AUTO##spot"))
        {
            spot_manual_ = false;
        }
    }
    if (legs_.empty())
    {
        return;
    }

    if (const std::optional<std::int32_t> expires = strategyExpiration(legs_); expires.has_value() && *expires != 0)
    {
        row.fact("EXPIRES", formatSessionDate(*expires), Theme::kText);
    }
    const bool credit = summary.net_cost < 0.0;
    row.fact(credit ? "NET CREDIT" : "NET DEBIT", formatDollars(std::fabs(summary.net_cost), false), Theme::kText);
    row.fact("MAX PROFIT",
             summary.max_profit.has_value() ? formatDollars(*summary.max_profit, true) : std::string("unlimited"),
             summary.max_profit.has_value() && *summary.max_profit <= 0.0 ? Theme::kDown : Theme::kUp);
    row.fact("MAX LOSS",
             summary.max_loss.has_value() ? formatDollars(*summary.max_loss, true) : std::string("unlimited"),
             summary.max_loss.has_value() && *summary.max_loss >= 0.0 ? Theme::kUp : Theme::kDown);
    std::string breakevens;
    for (const double breakeven : summary.breakevens)
    {
        if (!breakevens.empty())
        {
            breakevens += ", ";
        }
        breakevens += formatStrike(breakeven);
    }
    row.fact("BREAKEVEN", breakevens.empty() ? std::string("none") : breakevens, Theme::kText);
    if (spot_ > 0.0)
    {
        const double here = strategyPayoff(legs_, spot_);
        row.fact("AT SPOT", formatDollars(here, true), here < 0.0 ? Theme::kDown : Theme::kUp);
    }
}

void PayoffWizard::drawPlot(const PayoffSummary& summary)
{
    constexpr ImPlotFlags plot_flags = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus |
                                       ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText | ImPlotFlags_Crosshairs;
    // The Y fit is applied in EndPlot, so the padding stays pushed until then.
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.0f, kPayoffFitPadding));
    if (!ImPlot::BeginPlot("##payoff_plot", ImVec2(-1.0f, -1.0f), plot_flags))
    {
        ImPlot::PopStyleVar();
        return;
    }
    ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoHighlight);
    ImPlot::SetupAxis(ImAxis_Y1, nullptr,
                      ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit | ImPlotAxisFlags_Opposite |
                          ImPlotAxisFlags_NoHighlight);
    ImPlot::SetupAxisFormat(ImAxis_X1, "%.2f");
    ImPlot::SetupAxisFormat(ImAxis_Y1, "%.0f");
    if (refit_)
    {
        const SpotRange range = defaultSpotRange(legs_, spot_);
        ImPlot::SetupAxisLimits(ImAxis_X1, range.low, range.high, ImPlotCond_Always);
        refit_ = false;
    }

    const ImPlotRect limits = ImPlot::GetPlotLimits();
    const PayoffPath path = payoffPath(legs_, limits.X.Min, limits.X.Max);
    const int count = static_cast<int>(path.spots.size());
    if (count >= 2)
    {
        std::vector<double> gains(path.profits.size());
        std::vector<double> losses(path.profits.size());
        std::ranges::transform(path.profits, gains.begin(), [](double profit) { return std::max(profit, 0.0); });
        std::ranges::transform(path.profits, losses.begin(), [](double profit) { return std::min(profit, 0.0); });

        ImPlotSpec gain_fill;
        gain_fill.FillColor = Theme::kUp;
        gain_fill.FillAlpha = 0.22f;
        ImPlot::PlotShaded("##gain", path.spots.data(), gains.data(), count, 0.0, gain_fill);

        ImPlotSpec loss_fill;
        loss_fill.FillColor = Theme::kDown;
        loss_fill.FillAlpha = 0.22f;
        ImPlot::PlotShaded("##loss", path.spots.data(), losses.data(), count, 0.0, loss_fill);

        ImPlotSpec line;
        line.LineColor = Theme::kAccent;
        line.LineWeight = 2.0f;
        ImPlot::PlotLine("##payoff", path.spots.data(), path.profits.data(), count, line);
    }

    // A plotted segment rather than an infinite line, so the Y fit always keeps breakeven in view.
    if (count >= 2)
    {
        const double zero_xs[] = {path.spots.front(), path.spots.back()};
        const double zero_ys[] = {0.0, 0.0};
        ImPlotSpec zero_line;
        zero_line.LineColor = Theme::kLine2;
        ImPlot::PlotLine("##zero", zero_xs, zero_ys, 2, zero_line);
    }

    const std::vector<double> strikes = legStrikes(legs_);
    if (!strikes.empty())
    {
        ImPlotSpec strike_lines;
        strike_lines.LineColor = Theme::WithAlpha(Theme::kTextFaint, 0.8f);
        ImPlot::PlotInfLines("##strikes", strikes.data(), static_cast<int>(strikes.size()), strike_lines);
    }

    if (spot_ > 0.0)
    {
        ImPlotSpec spot_line;
        spot_line.LineColor = Theme::WithAlpha(Theme::kAccent, 0.6f);
        ImPlot::PlotInfLines("##spot", &spot_, 1, spot_line);
        ImPlot::TagX(spot_, Theme::kAccent, "%.2f", spot_);
    }

    if (!summary.breakevens.empty())
    {
        const std::vector<double> zeros(summary.breakevens.size(), 0.0);
        ImPlotSpec marks;
        marks.Marker = ImPlotMarker_Diamond;
        marks.MarkerSize = 4.0f;
        marks.MarkerFillColor = Theme::kText;
        marks.MarkerLineColor = Theme::kText;
        ImPlot::PlotScatter("##breakevens", summary.breakevens.data(), zeros.data(),
                            static_cast<int>(summary.breakevens.size()), marks);
    }

    if (ImPlot::IsPlotHovered())
    {
        const ImPlotPoint mouse = ImPlot::GetPlotMousePos();
        if (mouse.x >= 0.0)
        {
            const double profit = strategyPayoff(legs_, mouse.x);
            ImPlotSpec dot;
            dot.Marker = ImPlotMarker_Circle;
            dot.MarkerSize = 3.5f;
            dot.MarkerFillColor = Theme::kText;
            dot.MarkerLineColor = Theme::kText;
            ImPlot::PlotScatter("##hover", &mouse.x, &profit, 1, dot);
            ImPlot::TagX(mouse.x, Theme::kAccent, "%.2f", mouse.x);
            const std::string label = formatDollars(profit, true);
            ImPlot::TagY(profit, profit < 0.0 ? Theme::kDown : Theme::kUp, "%s", label.c_str());
        }
    }
    ImPlot::EndPlot();
    ImPlot::PopStyleVar();
}

void PayoffWizard::drawGraph()
{
    const PayoffSummary summary = summarizePayoff(legs_);
    drawSummary(summary);
    ImGui::Separator();
    if (legs_.empty())
    {
        ImGui::TextColored(Theme::kMuted, "Add legs below to draw the payoff at expiration.");
        return;
    }
    drawPlot(summary);
}

void PayoffWizard::drawEntry(std::span<const OptionQuote> chain, SessionDate expiration)
{
    drawRecipeRow(chain);
    drawLegRow(chain, expiration);
    if (!status_.empty())
    {
        ImGui::TextColored(status_error_ ? Theme::kDown : Theme::kMuted, "%s", status_.c_str());
    }
    if (legs_.empty())
    {
        ImGui::TextColored(Theme::kMuted,
                           "Pick a strategy and press ADD STRATEGY, or build one leg at a time with ADD LEG.");
        return;
    }
    drawLegTable();
}

}  // namespace terminal
