// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "options/OptionPayoff.h"

#include "market_data/Types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// How a chain quote becomes an entry price.
enum class PriceBasis : std::uint8_t
{
    Mid,
    // Buys pay the ask and sells take the bid.
    Natural,
    Last,
};

// Per-share entry price for trading quote. Mid falls back to the bid/ask
// average, then last. Natural falls back to that mid when there is no ask;
// a sell with no bid is priced at 0.
[[nodiscard]] double quotePrice(const OptionQuote& quote, PriceBasis basis, bool buy) noexcept;

// quantity is signed contracts. price is per share.
[[nodiscard]] PayoffLeg legFromQuote(const OptionQuote& quote, double quantity, double price) noexcept;

// shares is signed. price is per share.
[[nodiscard]] PayoffLeg underlyingLeg(double shares, double price) noexcept;

// Ascending strikes in the chain, without repeats.
[[nodiscard]] std::vector<double> listedStrikes(std::span<const OptionQuote> chain);

// Index of the strike closest to spot. A tie takes the lower strike.
[[nodiscard]] std::optional<std::size_t> nearestStrikeIndex(std::span<const double> strikes, double spot) noexcept;

[[nodiscard]] const OptionQuote* findQuote(std::span<const OptionQuote> chain,
                                         OptionRight right,
                                         double strike) noexcept;

// strike + call - put at the strike whose call and put prices are closest.
// Put-call parity without carry, so it is an estimate the user can override.
// Empty when no strike has a priced call and put.
[[nodiscard]] std::optional<double> parityImpliedSpot(std::span<const OptionQuote> chain);

// One strike the template asks for. offset counts listed strikes from the one
// nearest spot, so -1 is one strike below at-the-money.
struct StrikeSlot
{
    const char* label{""};
    int offset{0};
};

// ratio is signed contracts per unit of quantity. An underlying leg has no
// slot (-1) and its ratio counts contracts' worth of shares.
struct TemplateLeg
{
    LegInstrument instrument{LegInstrument::Call};
    int slot{-1};
    double ratio{1.0};
};

// A named recipe. Slots are in ascending strike order. New strategies are a
// table row in OptionStrategy.cpp; nothing else needs to change.
struct StrategyTemplate
{
    const char* id{""};
    const char* name{""};
    const char* note{""};
    std::span<const StrikeSlot> slots{};
    std::span<const TemplateLeg> legs{};
};

[[nodiscard]] std::span<const StrategyTemplate> strategyTemplates() noexcept;

[[nodiscard]] const StrategyTemplate* findStrategyTemplate(std::string_view id) noexcept;

// Indexes into a strike list of strike_count entries for each slot, offset from
// atm_index and clamped to the list. Empty when strike_count is 0.
[[nodiscard]] std::vector<std::size_t> defaultSlotStrikes(const StrategyTemplate& recipe,
                                                          std::size_t strike_count,
                                                          std::size_t atm_index);

struct TemplateInputs
{
    // One strike per slot, ascending.
    std::span<const double> slot_strikes{};
    // Positive multiple of every ratio.
    double quantity{1.0};
    PriceBasis basis{PriceBasis::Mid};
    // Entry price for underlying legs.
    double underlying_price{0.0};
};

// legs is empty exactly when error is set.
struct TemplateBuild
{
    std::vector<PayoffLeg> legs;
    std::string error;
};

// Prices every option leg from chain. Fails when a slot is missing or out of
// order, the quantity is not positive, a contract is not listed, or a share leg
// has no underlying price.
[[nodiscard]] TemplateBuild buildTemplate(const StrategyTemplate& recipe,
                                          std::span<const OptionQuote> chain,
                                          const TemplateInputs& inputs);

}  // namespace terminal
