// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "options/OptionPayoff.h"
#include "options/OptionStrategy.h"

#include "market_data/Types.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// Edit state and drawing for one payoff strategy. The math lives in options/;
// the window around it (PayoffPanel) decides where the two halves go.
//
// The graph half shows the summary and the expiration payoff. The entry half
// adds legs from a recipe or one at a time, and lists them for editing.
class PayoffWizard
{
public:
    // Drops every leg. The panel calls this when the underlying changes.
    void clear();

    // Restores saved legs. A set that does not validate is dropped.
    void restore(std::vector<PayoffLeg> legs, double manual_spot);

    [[nodiscard]] const std::vector<PayoffLeg>& legs() const noexcept;

    // The spot typed by hand, or 0 while it comes from the chain.
    [[nodiscard]] double manualSpot() const noexcept;

    // Call once per frame before drawing. chain may be empty; then strikes and
    // prices are typed by hand.
    void syncChain(std::span<const OptionQuote> chain);

    void drawGraph();

    // expiration is the chain's selected date, or 0 when none is selected.
    void drawEntry(std::span<const OptionQuote> chain, SessionDate expiration);

private:
    struct LegDraft
    {
        LegInstrument instrument{LegInstrument::Call};
        bool buy{true};
        double strike{0.0};
        double price{0.0};
        bool price_typed{false};
    };

    void addLegs(std::span<const PayoffLeg> incoming, std::string_view what);
    void drawRecipeRow(std::span<const OptionQuote> chain);
    void drawLegRow(std::span<const OptionQuote> chain, SessionDate expiration);
    void drawLegTable();
    void drawSummary(const PayoffSummary& summary);
    void drawPlot(const PayoffSummary& summary);
    void autoPriceDraft(std::span<const OptionQuote> chain);
    void setStatus(std::string text, bool error);

    std::vector<PayoffLeg> legs_;
    std::vector<double> strikes_;
    std::vector<std::size_t> slot_picks_;
    std::size_t template_index_{0};
    std::size_t picks_for_template_{static_cast<std::size_t>(-1)};
    LegDraft draft_;
    int quantity_{1};
    PriceBasis basis_{PriceBasis::Mid};
    double spot_{0.0};
    bool spot_manual_{false};
    bool refit_{true};
    std::string status_;
    bool status_error_{false};
};

}  // namespace terminal
